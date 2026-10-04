// Saltbound - saving and loading the game (D46): the boat, the player and the sea.

#include "Core/SaltSaveSubsystem.h"

#include "Boat/SaltHullShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UObject/UObjectIterator.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPickup.h"
#include "World/SaltWindSubsystem.h"

namespace
{
	template <typename T>
	T* FindFirst(UWorld* World)
	{
		for (TActorIterator<T> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	FString MapOf(const UWorld* World)
	{
		return UWorld::RemovePIEPrefix(World->GetMapName());
	}
}

USaltSaveSubsystem* USaltSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<USaltSaveSubsystem>() : nullptr;
}

bool USaltSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

FDateTime USaltSaveSubsystem::GetSaveTime() const
{
	const USaltSaveGame* Save = HasSave() ? Cast<USaltSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)) : nullptr;
	return Save ? Save->SavedAt : FDateTime();
}

bool USaltSaveSubsystem::SaveGame(UWorld* World, const TCHAR* Reason)
{
	if (!World)
	{
		return false;
	}
	OnSaveStarted.Broadcast();
	USaltSaveGame* Save = Cast<USaltSaveGame>(UGameplayStatics::CreateSaveGameObject(USaltSaveGame::StaticClass()));
	Save->SavedAt = FDateTime::Now();
	Save->Map = MapOf(World);

	ASaltBoat* Boat = FindFirst<ASaltBoat>(World);
	if (Boat)
	{
		Save->bHasBoat = true;
		Save->BoatLocation = Boat->GetActorLocation();
		Save->BoatYaw = Boat->GetYawDeg();
		Save->BoatHold = Boat->GetHold();
		Save->bBoatSail = Boat->IsSailSet();
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (const ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr)
	{
		const FVector Feet = Player->GetActorLocation() - FVector(0.0, 0.0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FRotator View = PC->GetControlRotation();
		Save->PlayerFeet = Feet;
		Save->PlayerYaw = View.Yaw;
		if (const USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			Save->bHasInventory = true;
			Save->Inventory = Satchel->GetSlots();
		}
		Save->PlayerPitch = FRotator::NormalizeAxis(View.Pitch);
		if (Boat)
		{
			// Aboard: anywhere over the deck (standing, at the helm or on the gear), kept in the boat's frame.
			const FVector Local = Boat->GetActorTransform().InverseTransformPosition(Feet) * 0.01;
			Save->bPlayerAboard = FMath::Abs(Local.X) < SaltHull::HalfLength && FMath::Abs(Local.Y) < SaltHull::HalfBeamAtSheer(Local.X)
				&& Local.Z > 0.2 && Local.Z < 3.0;
			if (Save->bPlayerAboard)
			{
				Save->PlayerFeet = Local * 100.0;
				Save->PlayerYaw = FRotator::NormalizeAxis(View.Yaw - Boat->GetYawDeg());
			}
		}
	}

	if (const ASaltLightingRig* Rig = FindFirst<ASaltLightingRig>(World))
	{
		Save->Preset = Rig->CurrentPreset;
		Save->Region = Rig->CurrentRegion;
		Save->Weather = Rig->CurrentWeather;
		Save->SeaState = Rig->CurrentSeaState;
	}
	if (const USaltWindSubsystem* Wind = World->GetSubsystem<USaltWindSubsystem>())
	{
		Save->Wind = FVector(Wind->GetBaseFromDeg(), Wind->GetBaseSpeed(), Wind->GetGustiness());
	}
	Save->Collected = Collected.Array();
	for (TObjectIterator<USaltInventoryComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && !It->GetStorageId().IsNone())
		{
			Save->Stores.Add({ It->GetStorageId(), It->GetSlots() });
		}
	}

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	UE_LOG(LogSaltbound, Display, TEXT("SB_SAVE saved=%d slot=%s reason=%s boat=(%.0f, %.0f) hold=%d sail=%d aboard=%d sea=%s"),
		bOk ? 1 : 0, *Slot, Reason, Save->BoatLocation.X, Save->BoatLocation.Y, (int32)Save->BoatHold, Save->bBoatSail ? 1 : 0,
		Save->bPlayerAboard ? 1 : 0, *Save->SeaState.ToString());
	// The save sign in the corner shows it (D55); no notice.
	OnSaveFinished.Broadcast();
	return bOk;
}

bool USaltSaveSubsystem::LoadGame(UWorld* World)
{
	const USaltSaveGame* Save = World && HasSave() ? Cast<USaltSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)) : nullptr;
	if (!Save)
	{
		return false;
	}
	if (Save->Map != MapOf(World))
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_SAVE slot %s belongs to %s, not %s; not loaded"), *Slot, *Save->Map, *MapOf(World));
		return false;
	}

	// The sea and the weather first, so the boat settles on the right waves.
	if (ASaltLightingRig* Rig = FindFirst<ASaltLightingRig>(World))
	{
		if (!Save->Preset.IsNone() && Save->Preset != Rig->CurrentPreset)
		{
			Rig->ApplyPreset(Save->Preset);
		}
		if (!Save->Region.IsNone() && Save->Region != Rig->CurrentRegion)
		{
			Rig->SetRegion(Save->Region, 0.f);
		}
		if (!Save->Weather.IsNone() && Save->Weather != Rig->CurrentWeather)
		{
			Rig->SetWeather(Save->Weather, 0.f);
		}
		if (!Save->SeaState.IsNone() && Save->SeaState != Rig->CurrentSeaState)
		{
			Rig->SetSeaState(Save->SeaState, 0.f);
		}
	}
	if (USaltWindSubsystem* Wind = World->GetSubsystem<USaltWindSubsystem>())
	{
		Wind->SetBaseWind(Save->Wind.X, Save->Wind.Y, Save->Wind.Z);
	}

	ASaltBoat* Boat = FindFirst<ASaltBoat>(World);
	if (Boat && Save->bHasBoat)
	{
		Boat->RestoreState(Save->BoatLocation, Save->BoatYaw, Save->BoatHold, Save->bBoatSail);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr)
	{
		FVector Feet = Save->PlayerFeet;
		float Yaw = Save->PlayerYaw;
		if (Save->bPlayerAboard && Boat)
		{
			Feet = Boat->GetActorTransform().TransformPosition(Feet);
			Yaw += Boat->GetYawDeg();
		}
		Player->TeleportFeetTo(Feet, Yaw);
		PC->SetControlRotation(FRotator(Save->PlayerPitch, Yaw, 0.f));
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			if (Save->bHasInventory)
			{
				Satchel->SetSlots(Save->Inventory);
			}
		}
	}
	// The stores' contents.
	for (TObjectIterator<USaltInventoryComponent> It; It; ++It)
	{
		if (It->GetWorld() != World || It->GetStorageId().IsNone())
		{
			continue;
		}
		if (const FSaltSavedStore* Saved = Save->Stores.FindByPredicate([&It](const FSaltSavedStore& S) { return S.Id == It->GetStorageId(); }))
		{
			It->SetSlots(Saved->Slots);
		}
	}
	// Pickups taken in that game stay gone; the others are back.
	Collected = TSet<FName>(Save->Collected);
	for (TActorIterator<ASaltPickup> It(World); It; ++It)
	{
		It->SetCollected(IsCollected(It->PickupId));
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_SAVE loaded slot=%s from=%s boat=(%.0f, %.0f) hold=%d aboard=%d"),
		*Slot, *Save->SavedAt.ToString(), Save->BoatLocation.X, Save->BoatLocation.Y, (int32)Save->BoatHold, Save->bPlayerAboard ? 1 : 0);
	Notify(World, NSLOCTEXT("Saltbound", "GameLoaded", "Game loaded"));
	return true;
}

void USaltSaveSubsystem::Notify(UWorld* World, const FText& Text) const
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
	{
		PC->ShowNotice(Text);
	}
}

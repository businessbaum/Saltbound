// Saltbound - saving and loading the game (D46): the boat, the player and the sea.

#include "Core/SaltSaveSubsystem.h"

#include "Boat/SaltHullShape.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltGameUserSettings.h"
#include "Core/SaltPlaces.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Hub/SaltSeaGate.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UObject/UObjectIterator.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPickup.h"
#include "World/SaltPrologue.h"
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

void USaltSaveSubsystem::SetUnlocked(FName ZoneId, bool bUnlocked)
{
	if (ZoneId.IsNone())
	{
		return;
	}
	if (bUnlocked)
	{
		Unlocked.Add(ZoneId);
	}
	else
	{
		Unlocked.Remove(ZoneId);
	}
	OnHubChanged.Broadcast();
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
	if (!bQuiet)
	{
		OnSaveStarted.Broadcast();
	}
	USaltSaveGame* Save = Cast<USaltSaveGame>(UGameplayStatics::CreateSaveGameObject(USaltSaveGame::StaticClass()));
	Save->SavedAt = FDateTime::Now();
	Save->Map = MapOf(World);
	Save->PlayTime = PlayTime;

	ASaltBoat* Boat = FindFirst<ASaltBoat>(World);
	if (Boat)
	{
		Save->bHasBoat = true;
		Save->BoatLocation = Boat->GetActorLocation();
		Save->BoatYaw = Boat->GetYawDeg();
		Save->BoatHold = Boat->GetHold();
		Save->bBoatSail = Boat->IsSailSet();
		Save->BoatMooring = Boat->GetMooring();
		if (const USaltHullDamageComponent* Damage = Boat->GetHullDamage())
		{
			Save->HullHoles = Damage->GetHoles();
			Save->BilgeWaterKg = Damage->GetWaterKg();
			Save->SailTear = Damage->GetSailTear();
		}
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
		Save->bHasOwnedTools = true;
		Save->OwnedTools = Player->GetOwnedToolList();
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
		Save->TimeOfDay = Rig->GetTimeOfDay();
		Save->DayNumber = Rig->GetDay();
		Save->Region = Rig->CurrentRegion;
		Save->Weather = Rig->CurrentWeather;
		Save->SeaState = Rig->CurrentSeaState;
	}
	if (const USaltWindSubsystem* Wind = World->GetSubsystem<USaltWindSubsystem>())
	{
		Save->Wind = FVector(Wind->GetBaseFromDeg(), Wind->GetBaseSpeed(), Wind->GetGustiness());
	}
	Save->Collected = Collected.Array();
	Save->FoundPlaces = Found.Array();
	Save->DigStages = DigStages;
	Save->StoryBeats = StoryBeats;
	// The hub (D62).
	Save->HubBerth = HubBerth;
	Save->HubUnlocked = Unlocked.Array();
	Save->Salt = Salt;
	if (const ASaltSeaGate* Gate = FindFirst<ASaltSeaGate>(World))
	{
		bSeaGateOpen = Gate->GetOpenAmount() >= 0.5f;
	}
	Save->bSeaGateOpen = bSeaGateOpen;
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
	if (bOk && !bQuiet)
	{
		// D66: the voyage remembers when it was saved, and a picture of the world for the voyages screen (no UI on it).
		PlayTimeAtSave = PlayTime;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".png")), false, false);
	}
	// The save sign in the corner shows it (D55); no notice.
	if (!bQuiet)
	{
		OnSaveFinished.Broadcast();
	}
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
		// D72: the clock goes on from the saved hour; saves from before it keep only a preset (its hour, running).
		const float SavedHour = Save->TimeOfDay >= 0.f ? Save->TimeOfDay : Rig->GetLookHour(Save->Preset);
		if (Rig->DayCycle.Num() > 0 && SavedHour >= 0.f)
		{
			Rig->SetDay(Save->DayNumber);
			Rig->SetTimeOfDay(SavedHour, false);
		}
		else if (!Save->Preset.IsNone() && Save->Preset != Rig->CurrentPreset)
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
		Boat->RestoreState(Save->BoatLocation, Save->BoatYaw, Save->BoatHold, Save->bBoatSail, Save->Version >= 5 ? Save->BoatMooring : FSaltMooring());
		if (USaltHullDamageComponent* Damage = Boat->GetHullDamage())
		{
			Damage->RestoreState(Save->HullHoles, Save->BilgeWaterKg, Save->SailTear);
		}
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
		if (Save->bHasOwnedTools)
		{
			Player->SetOwnedTools(Save->OwnedTools);
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
	Found = TSet<FName>(Save->FoundPlaces);
	DigStages = Save->DigStages;
	StoryBeats = Save->StoryBeats;
	HubBerth = Save->HubBerth;
	Unlocked = TSet<FName>(Save->HubUnlocked);
	Salt = Save->Salt;
	bSeaGateOpen = Save->Version >= 5 ? Save->bSeaGateOpen : true;
	if (ASaltSeaGate* Gate = FindFirst<ASaltSeaGate>(World))
	{
		Gate->SetOpenAmountNow(bSeaGateOpen ? 1.f : 0.f);
	}
	for (TActorIterator<ASaltPickup> It(World); It; ++It)
	{
		It->SetCollected(IsCollected(It->PickupId));
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_SAVE loaded slot=%s from=%s boat=(%.0f, %.0f) hold=%d aboard=%d"),
		*Slot, *Save->SavedAt.ToString(), Save->BoatLocation.X, Save->BoatLocation.Y, (int32)Save->BoatHold, Save->bPlayerAboard ? 1 : 0);
	PlayTime = Save->PlayTime;
	PlayTimeAtSave = PlayTime;
	if (!bQuiet)
	{
		Notify(World, NSLOCTEXT("Saltbound", "GameLoaded", "Game loaded"));
	}
	OnGameLoaded.Broadcast();
	OnHubChanged.Broadcast();
	return true;
}

void USaltSaveSubsystem::Notify(UWorld* World, const FText& Text) const
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
	{
		PC->ShowNotice(Text);
	}
}

// --- Voyages (D66) -----------------------------------------------------------------------------------------------------

FString USaltSaveSubsystem::VoyageSlot(int32 Index)
{
	return Index <= 0 ? FString(TEXT("Main")) : FString::Printf(TEXT("Voyage%d"), Index + 1);
}

int32 USaltSaveSubsystem::GetActiveVoyage() const
{
	for (int32 I = 0; I < VoyageCount; ++I)
	{
		if (Slot == VoyageSlot(I))
		{
			return I;
		}
	}
	return INDEX_NONE;
}

void USaltSaveSubsystem::SetActiveVoyage(int32 Index)
{
	Index = FMath::Clamp(Index, 0, VoyageCount - 1);
	Slot = VoyageSlot(Index);
	if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		S->LastVoyage = Index;
		S->SaveSettings();
	}
}

FSaltVoyageInfo USaltSaveSubsystem::GetVoyageInfo(int32 Index) const
{
	FSaltVoyageInfo Info;
	const FString Name = VoyageSlot(Index);
	const USaltSaveGame* Save = UGameplayStatics::DoesSaveGameExist(Name, 0) ? Cast<USaltSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, 0)) : nullptr;
	if (!Save)
	{
		return Info;
	}
	Info.bExists = true;
	Info.SavedAt = Save->SavedAt;
	Info.PlayTime = Save->PlayTime;
	// Where the player is: aboard, where the boat lies.
	const FVector Where = Save->bPlayerAboard && Save->bHasBoat ? Save->BoatLocation : Save->PlayerFeet;
	const FVector2D WorldM(Where.X * 0.01, Where.Y * 0.01);
	const FSaltPlace* Nearest = nullptr;
	float Best = 1500.f;
	for (const FSaltPlace& P : SaltPlaces::All())
	{
		const float D = SaltPlaces::DistanceToShoreM(P, WorldM);
		if (D < Best)
		{
			Best = D;
			Nearest = &P;
		}
	}
	Info.Place = Nearest ? Nearest->Name : NSLOCTEXT("SaltVoyages", "OpenSea", "the open sea");
	Info.bNearPlace = Nearest != nullptr;
	Info.Hold = Save->BoatHold;
	switch (Save->BoatHold)
	{
	case ESaltBoatHold::Moored: Info.Boat = NSLOCTEXT("SaltVoyages", "Moored", "moored"); break;
	case ESaltBoatHold::Anchored: Info.Boat = NSLOCTEXT("SaltVoyages", "Anchored", "at anchor"); break;
	default: Info.Boat = Save->bBoatSail ? NSLOCTEXT("SaltVoyages", "UnderSail", "under sail") : NSLOCTEXT("SaltVoyages", "Adrift", "adrift"); break;
	}
	const FString Picture = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Name + TEXT(".png"));
	if (IFileManager::Get().FileExists(*Picture))
	{
		Info.Picture = Picture;
	}
	return Info;
}

bool USaltSaveSubsystem::DeleteVoyage(int32 Index)
{
	const FString Name = VoyageSlot(Index);
	IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Name + TEXT(".png"))), false, true, true);
	const bool bOk = UGameplayStatics::DeleteGameInSlot(Name, 0);
	UE_LOG(LogSaltbound, Display, TEXT("SB_SAVE deleted=%d slot=%s"), bOk ? 1 : 0, *Name);
	return bOk;
}

void USaltSaveSubsystem::TakeNewVoyageSnapshot(UWorld* World)
{
	if (!World || bHasSnapshot)
	{
		return;
	}
	const FString Keep = Slot;
	Slot = TEXT("NewVoyage");
	bQuiet = true;
	bHasSnapshot = SaveGame(World, TEXT("snapshot"));
	bQuiet = false;
	Slot = Keep;
}

bool USaltSaveSubsystem::RestoreNewVoyageSnapshot(UWorld* World)
{
	if (!World || !bHasSnapshot)
	{
		return false;
	}
	// The world as it was at the start: the boat at her mooring, the player on the pier, the satchel and stores as given,
	// every pickup back, no island found, the hub as built.
	const FString Keep = Slot;
	Slot = TEXT("NewVoyage");
	bQuiet = true;
	const bool bLoaded = LoadGame(World);
	bQuiet = false;
	Slot = Keep;
	PlayTime = 0.0;
	PlayTimeAtSave = 0.0;
	return bLoaded;
}

void USaltSaveSubsystem::SetBeat(FName Beat, float When)
{
	if (Beat.IsNone() || StoryBeats.Contains(Beat))
	{
		return;
	}
	StoryBeats.Add(Beat, When);
	UE_LOG(LogSaltbound, Display, TEXT("SB_STORY beat=%s at day %d %.2f h"), *Beat.ToString(), FMath::FloorToInt(When / 24.f), FMath::Fmod(When, 24.f));
	OnStoryBeat.Broadcast(Beat);
}

bool USaltSaveSubsystem::StartNewVoyage(UWorld* World, int32 Index)
{
	if (!World || !bHasSnapshot)
	{
		return false;
	}
	const bool bLoaded = RestoreNewVoyageSnapshot(World);
	SetActiveVoyage(Index);
	// FH1 (D77): a new voyage is the prologue: the castaway wakes on the wreck beach at dawn with nothing.
	if (USaltPrologueSubsystem* Prologue = bLoaded ? World->GetSubsystem<USaltPrologueSubsystem>() : nullptr)
	{
		Prologue->Begin();
	}
	const bool bSaved = bLoaded && SaveGame(World, TEXT("new voyage"));
	UE_LOG(LogSaltbound, Display, TEXT("SB_SAVE new_voyage=%d slot=%s loaded=%d"), bSaved ? 1 : 0, *Slot, bLoaded ? 1 : 0);
	return bSaved;
}

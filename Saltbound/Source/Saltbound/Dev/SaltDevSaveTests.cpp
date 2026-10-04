// Saltbound - save and load checks (sb.Dev.RunSaveTests, D46), in a test slot so the player's own save is untouched.

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"
#include "World/SaltLightingRig.h"
#include "World/SaltWindSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunSaveTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		ASaltBoat* Boat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			Boat = *It;
			break;
		}
		ASaltLightingRig* Rig = nullptr;
		for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
		{
			Rig = *It;
			break;
		}
		USaltWindSubsystem* Wind = World ? World->GetSubsystem<USaltWindSubsystem>() : nullptr;
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		if (!Saves || !Boat || !Rig || !Wind || !Player)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST SaveSuite FAIL reason=missing_subsystem_boat_rig_wind_or_player"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST SaveSuite DONE passed=0 failed=1"));
			return;
		}
		auto Feet = [Player]() { return Player->GetActorLocation() - FVector(0.0, 0.0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); };
		const FString OwnSlot = Saves->Slot;
		const FName OwnSea = Rig->CurrentSeaState;
		const FVector OwnWind(Wind->GetBaseFromDeg(), Wind->GetBaseSpeed(), Wind->GetGustiness());
		const FVector OwnFeet = Feet();
		const float OwnYaw = PC->GetControlRotation().Yaw;
		Saves->Slot = TEXT("SaltDevTest");
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);

		// A voyage: anchored off the cove with the sail set, the player standing on deck, a moderate sea.
		const FVector At(6000.f, -9000.f, 0.f);
		Rig->SetSeaState(TEXT("Moderate"), 0.f);
		Wind->SetBaseWind(120.f, 650.f, 0.2f);
		Boat->RestoreState(At, 37.f, ESaltBoatHold::Anchored, true);
		const FVector DeckLocal(-100.f, 45.f, SaltHull::SheerZ(-1.f) * 100.f + 2.f);
		Player->TeleportFeetTo(Boat->GetActorTransform().TransformPosition(DeckLocal), Boat->GetYawDeg() + 20.f);
		const bool bSaved = Saves->SaveGame(World, TEXT("test"));
		Report(TEXT("SaveWrites"), bSaved && Saves->HasSave(), FString::Printf(TEXT("saved=%d exists=%d"), bSaved ? 1 : 0, Saves->HasSave() ? 1 : 0));

		// Everything somewhere else, then load.
		Boat->ResetToStart();
		Player->TeleportFeetTo(FVector(1200.f, 0.f, 112.f), 0.f);
		Rig->SetSeaState(TEXT("Calm"), 0.f);
		Wind->SetBaseWind(0.f, 0.f, 0.f);
		const bool bLoaded = Saves->LoadGame(World);
		const float BoatOff = (float)FVector::Dist2D(Boat->GetActorLocation(), At);
		const float YawOff = FMath::Abs(FRotator::NormalizeAxis(Boat->GetYawDeg() - 37.f));
		Report(TEXT("LoadBoat"), bLoaded && BoatOff < 30.f && YawOff < 1.f && Boat->GetHold() == ESaltBoatHold::Anchored && Boat->IsSailSet()
			&& Boat->GetAnchorHold() > 0.99f, FString::Printf(TEXT("loaded=%d offset=%.1fcm yawOff=%.2fdeg hold=%d sail=%d anchorHold=%.2f"),
			bLoaded ? 1 : 0, BoatOff, YawOff, (int32)Boat->GetHold(), Boat->IsSailSet() ? 1 : 0, Boat->GetAnchorHold()));
		const FVector Local = Boat->GetActorTransform().InverseTransformPosition(Feet());
		const float PlayerOff = (float)FVector::Dist(Local, DeckLocal);
		const float ViewOff = FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Yaw - (Boat->GetYawDeg() + 20.f)));
		Report(TEXT("LoadPlayerAboard"), PlayerOff < 20.f && ViewOff < 1.f, FString::Printf(
			TEXT("deckOffset=%.1fcm local=(%.0f, %.0f, %.0f) viewYawOff=%.2fdeg"), PlayerOff, Local.X, Local.Y, Local.Z, ViewOff));
		Report(TEXT("LoadSea"), Rig->CurrentSeaState == TEXT("Moderate") && FMath::IsNearlyEqual(Wind->GetBaseFromDeg(), 120.f, 0.5f)
			&& FMath::IsNearlyEqual(Wind->GetBaseSpeed(), 650.f, 1.f), FString::Printf(TEXT("sea=%s wind=%.0fdeg %.0fcm/s"),
			*Rig->CurrentSeaState.ToString(), Wind->GetBaseFromDeg(), Wind->GetBaseSpeed()));

		// Mooring at the windlass ends a voyage and saves.
		Boat->ResetToStart();                    // lying still at the mooring...
		Boat->SetHold(ESaltBoatHold::Free);      // ...cast off
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Boat->GetWindlass()->Interact(Player);
		const USaltSaveGame* AfterMoor = Saves->HasSave() ? Cast<USaltSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->Slot, 0)) : nullptr;
		Report(TEXT("MooringSaves"), Boat->GetHold() == ESaltBoatHold::Moored && AfterMoor && AfterMoor->BoatHold == ESaltBoatHold::Moored,
			FString::Printf(TEXT("hold=%d saved=%d savedHold=%d"), (int32)Boat->GetHold(), AfterMoor ? 1 : 0, AfterMoor ? (int32)AfterMoor->BoatHold : -1));

		// Leave things as they were.
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = OwnSlot;
		Boat->ResetToStart();
		Rig->SetSeaState(OwnSea, 0.f);
		Wind->SetBaseWind(OwnWind.X, OwnWind.Y, OwnWind.Z);
		Player->TeleportFeetTo(OwnFeet, OwnYaw);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST SaveSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunSaveTests(
		TEXT("sb.Dev.RunSaveTests"),
		TEXT("Checks saving and loading (boat, player aboard, sea, wind) and the save when mooring, in a test slot."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunSaveTests));
}
#endif

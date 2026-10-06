// Saltbound - checks of the first hour's prologue (sb.Dev.RunPrologueTests, FH1, D77): the castaway's start on the wreck
// beach, the beach finds, the sea chest, the Keeper's shovel and lantern, the cache, the sail locked until it is mended, the
// beats in the save. It leaves the world in the prologue (the play session ends after it).

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"
#include "World/SaltDig.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPickup.h"
#include "World/SaltPrologue.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunPrologueTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		USaltPrologueSubsystem* Prologue = World ? World->GetSubsystem<USaltPrologueSubsystem>() : nullptr;
		USaltDigSubsystem* Digs = World ? World->GetSubsystem<USaltDigSubsystem>() : nullptr;
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		ASaltBoat* Boat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			Boat = *It;
			break;
		}
		const ASaltLightingRig* Rig = nullptr;
		for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
		{
			Rig = *It;
			break;
		}
		USaltHullDamageComponent* Damage = Boat ? Boat->GetHullDamage() : nullptr;
		if (!Prologue || !Digs || !Saves || !Player || !Satchel || !Boat || !Damage || !Rig)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST PrologueSuite FAIL reason=missing_prologue_save_player_boat_or_rig"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST PrologueSuite DONE passed=0 failed=1"));
			return;
		}

		// The castaway's start: the wreck beach at dawn of day 1, nothing owned, an empty satchel, her sail ripped and locked.
		Prologue->Begin(false);
		const FVector Wake = Prologue->GetWakeSpot();
		const double FromWake = FVector::Dist2D(Player->GetActorLocation(), Wake);
		int32 Items = 0;
		for (const FSaltItemStack& S : Satchel->GetSlots())
		{
			Items += S.Count;
		}
		const bool bNoTools = !Player->IsToolOwned(TEXT("Compass")) && !Player->IsToolOwned(TEXT("Spyglass"))
			&& !Player->IsToolOwned(TEXT("Lantern")) && !Player->IsToolOwned(SaltDig::ShovelTool);
		Report(TEXT("StartOnWreckBeach"), FromWake < 300.0 && Rig->GetDay() == 1 && FMath::IsNearlyEqual(Rig->GetTimeOfDay(), SaltStory::StartHour, 0.05f)
			&& bNoTools && Items == 0 && Saves->HasBeat(SaltStory::Prologue) && Saves->HasBeat(SaltStory::Woke), FString::Printf(
			TEXT("from the wake spot=%.0f cm day=%d hour=%.2f noTools=%d items=%d beats=%d"), FromWake, Rig->GetDay(), Rig->GetTimeOfDay(),
			bNoTools ? 1 : 0, Items, Saves->GetBeats().Num()));

		// What washed up with you lies round the wreck, on the sand.
		int32 Finds = 0;
		int32 Near = 0;
		for (TActorIterator<ASaltPickup> It(World); It; ++It)
		{
			if (It->PickupId.ToString().StartsWith(TEXT("Prologue")) && !It->IsCollected())
			{
				++Finds;
				Near += FVector::Dist2D(It->GetActorLocation(), Wake) < 1500.0 && It->GetActorLocation().Z > 5.0 ? 1 : 0;
			}
		}
		Report(TEXT("BeachFinds"), Finds == 4 && Near == 4, FString::Printf(TEXT("finds=%d within 15 m and above the water=%d"), Finds, Near));

		// The ripped sail does not set.
		Boat->GetSailRope(true)->Interact(Player);
		const bool bSetWhileRipped = Boat->IsSailSet();
		Report(TEXT("SailLocked"), Damage->GetSailTear() == SaltStory::StartTear && Boat->IsSailLocked() && !bSetWhileRipped, FString::Printf(
			TEXT("tear=%d locked=%d set=%d"), Damage->GetSailTear(), Boat->IsSailLocked() ? 1 : 0, bSetWhileRipped ? 1 : 0));

		// The sea chest: the Cartographer's compass and spyglass.
		USaltInteractableComponent* Chest = Prologue->GetChest();
		if (Chest)
		{
			Chest->Interact(Player);
		}
		Report(TEXT("SeaChest"), Chest && Chest->GetAttachParent() == Boat->Hull.Get() && Player->IsToolOwned(TEXT("Compass"))
			&& Player->IsToolOwned(TEXT("Spyglass")) && Saves->HasBeat(SaltStory::Chest), FString::Printf(TEXT("chest=%d compass=%d spyglass=%d beat=%d"),
			Chest ? 1 : 0, Player->IsToolOwned(TEXT("Compass")) ? 1 : 0, Player->IsToolOwned(TEXT("Spyglass")) ? 1 : 0, Saves->HasBeat(SaltStory::Chest) ? 1 : 0));

		// The Harbour Keeper: his shovel and a lantern.
		Digs->HandleKeeper(nullptr, Player);
		Report(TEXT("KeeperShovelLantern"), Player->IsToolOwned(SaltDig::ShovelTool) && Player->IsToolOwned(TEXT("Lantern")) && Saves->HasBeat(SaltStory::Keeper),
			FString::Printf(TEXT("shovel=%d lantern=%d beat=%d"), Player->IsToolOwned(SaltDig::ShovelTool) ? 1 : 0, Player->IsToolOwned(TEXT("Lantern")) ? 1 : 0,
			Saves->HasBeat(SaltStory::Keeper) ? 1 : 0));

		// The cache dug up, two sailcloths: the sail is mended twice, then it sets.
		Saves->SetDigStage(SaltStory::CacheSpot, 2);
		Satchel->Add(TEXT("Canvas"), 2);
		const bool bFirst = Damage->MendSail(Player);
		const bool bSecond = Damage->MendSail(Player);
		Prologue->UpdateBeats();
		Boat->GetSailRope(true)->Interact(Player);
		const bool bSetNow = Boat->IsSailSet();
		Boat->SetSailSet(false);
		Report(TEXT("CacheMendsTheSail"), Saves->HasBeat(SaltStory::Cache) && bFirst && bSecond && Damage->GetSailTear() == 0
			&& Saves->HasBeat(SaltStory::SailMended) && !Boat->IsSailLocked() && bSetNow, FString::Printf(
			TEXT("cache=%d mended=%d,%d tear=%d beat=%d locked=%d sets=%d"), Saves->HasBeat(SaltStory::Cache) ? 1 : 0, bFirst ? 1 : 0, bSecond ? 1 : 0,
			Damage->GetSailTear(), Saves->HasBeat(SaltStory::SailMended) ? 1 : 0, Boat->IsSailLocked() ? 1 : 0, bSetNow ? 1 : 0));

		// The beats are saved with the game, and a loaded prologue keeps its lock (a fresh one: locked; mended: free).
		const FString OwnSlot = Saves->Slot;
		Saves->Slot = TEXT("SaltDevTest");
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		const int32 Beats = Saves->GetBeats().Num();
		const bool bSaved = Saves->SaveGame(World, TEXT("test"));
		Saves->ClearBeats();
		const bool bLoaded = bSaved && Saves->LoadGame(World);
		const int32 LoadedBeats = Saves->GetBeats().Num();
		const bool bBack = LoadedBeats == Beats && Saves->HasBeat(SaltStory::SailMended) && !Boat->IsSailLocked();
		Prologue->Begin(false);
		const bool bFreshSaved = Saves->SaveGame(World, TEXT("test"));
		Boat->SetSailLocked(false);
		const bool bFreshLoaded = bFreshSaved && Saves->LoadGame(World);
		Report(TEXT("BeatsSaved"), bLoaded && bBack && bFreshLoaded && Boat->IsSailLocked(), FString::Printf(
			TEXT("loaded=%d beats=%d/%d mendedFree=%d freshLocked=%d"), bLoaded ? 1 : 0, LoadedBeats, Beats, bBack ? 1 : 0,
			Boat->IsSailLocked() ? 1 : 0));
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = OwnSlot;
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST PrologueSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunPrologueTests(
		TEXT("sb.Dev.RunPrologueTests"),
		TEXT("FH1 (D77): checks the first hour's prologue (the start, the beach, the chest, the Keeper, the cache, the sail, the save)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunPrologueTests));
}
#endif

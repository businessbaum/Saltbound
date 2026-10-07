// Saltbound - checks of digging (sb.Dev.RunDigTests, D73): the spots placed, the keeper's shovel, a spot without and with the
// shovel, digging up and taking the find, loose ground, and the save. Everything is put back afterwards.

#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"
#include "World/SaltDig.h"
#include "World/SaltTalk.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunDigTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		USaltDigSubsystem* Dig = World ? World->GetSubsystem<USaltDigSubsystem>() : nullptr;
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		if (!Dig || !Saves || !Player || !Satchel)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST DigSuite FAIL reason=missing_dig_save_or_player"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST DigSuite DONE passed=0 failed=1"));
			return;
		}
		// What to put back.
		const TArray<FSaltItemStack> OwnSlots = Satchel->GetSlots();
		const TArray<FName> OwnTools = Player->GetOwnedToolList();
		const FName OwnHeld = Player->GetHeldTool();
		const int32 OwnSalt = Saves->GetSalt();
		const FString OwnSlot = Saves->Slot;

		// Every spot of the table lies on ground.
		int32 Placed = 0;
		for (const FSaltDigSpotDef& Def : SaltDig::Spots())
		{
			Placed += Dig->FindSpot(Def.Id) ? 1 : 0;
		}
		Report(TEXT("DigSpotsPlaced"), Placed == SaltDig::Spots().Num(), FString::Printf(TEXT("placed=%d of %d"), Placed, SaltDig::Spots().Num()));

		ASaltDigSpot* Spot = Dig->FindSpot(TEXT("LagoonEast"));
		const FSaltDigSpotDef* Def = SaltDig::Find(TEXT("LagoonEast"));
		if (!Spot || !Def)
		{
			Report(TEXT("DigSpot"), false, TEXT("LagoonEast missing"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST DigSuite DONE passed=%d failed=%d"), Passed, Failed);
			return;
		}
		const int32 OwnStage = Spot->GetStage();
		Spot->SetStage(0, true);

		// Without a shovel nothing comes up.
		Player->SetOwnedTools({ TEXT("Compass"), TEXT("Spyglass"), TEXT("Lantern") });
		Spot->Use_(Player);
		Report(TEXT("NeedsShovel"), Spot->GetStage() == 0 && Spot->GetPrompt(Player).ToString().Contains(TEXT("buried")),
			FString::Printf(TEXT("stage=%d prompt='%s'"), Spot->GetStage(), *Spot->GetPrompt(Player).ToString()));

		// The keeper hands it over (once).
		// FH2 (D81): the shovel is the Keeper's first talk's gift: answer him plainly until he is done.
		USaltTalkSubsystem* Talk = World->GetSubsystem<USaltTalkSubsystem>();
		if (Talk)
		{
			Talk->SetInstant(true);
		}
		Dig->HandleKeeper(nullptr, Player);
		for (int32 Say = 0; Talk && Talk->IsTalking() && Say < 8; ++Say)
		{
			Talk->Answer();
		}
		if (Talk)
		{
			Talk->SetInstant(false);
		}
		Report(TEXT("KeeperGivesShovel"), Player->IsToolOwned(SaltDig::ShovelTool), FString::Printf(TEXT("owned=%d"),
			Player->IsToolOwned(SaltDig::ShovelTool) ? 1 : 0));

		// A press takes the shovel in hand; then E is a hold that digs.
		Player->SetHeldTool(NAME_None);
		Spot->Use_(Player);
		Spot->Tick(0.02f);
		Report(TEXT("TakesShovel"), Player->GetHeldTool() == SaltDig::ShovelTool && Spot->GetUse()->HoldTime > 1.f,
			FString::Printf(TEXT("held=%s hold=%.1fs"), *Player->GetHeldTool().ToString(), Spot->GetUse()->HoldTime));

		// Dug up: the find lies in the hole; taking it fills the satchel (and the purse).
		TArray<FSaltItemStack> Empty;
		Empty.SetNum(OwnSlots.Num());
		Satchel->SetSlots(Empty);
		const bool bDug = Spot->DigUp(Player);
		const int32 StageDug = Spot->GetStage();
		const bool bTaken = Spot->TakeFind(Player);
		int32 Want = 0;
		int32 Got = 0;
		for (const FSaltItemStack& S : Def->Loot)
		{
			Want += S.Count;
			Got += FMath::Min(Satchel->CountOf(S.Id), S.Count);
		}
		Report(TEXT("DigUpAndTake"), bDug && StageDug == 1 && bTaken && Spot->GetStage() == 2 && Got == Want
			&& Saves->GetDigStage(Def->Id) == 2, FString::Printf(TEXT("dug=%d stageDug=%d taken=%d stage=%d items=%d/%d saved=%d"),
			bDug ? 1 : 0, StageDug, bTaken ? 1 : 0, Spot->GetStage(), Got, Want, Saves->GetDigStage(Def->Id)));

		// Loose ground: something small, once per cell.
		if (ASaltGroundDig* Ground = Dig->GetGroundDig())
		{
			FHitResult Hit;
			const FVector Beach(-1600.0, -3540.0, 0.0);   // the lagoon's south beach (island_shape: 0.39 m above the water)
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltDigTest), true);
			bool bGround = World->LineTraceSingleByChannel(Hit, Beach + FVector(0.0, 0.0, 3000.0), Beach - FVector(0.0, 0.0, 1000.0),
				ECC_Visibility, Params);
			for (int32 Tries = 0; bGround && !SaltDig::IsDiggable(Hit) && Tries < 5; ++Tries)
			{
				Params.AddIgnoredComponent(Hit.GetComponent());
				bGround = World->LineTraceSingleByChannel(Hit, Beach + FVector(0.0, 0.0, 3000.0), Beach - FVector(0.0, 0.0, 1000.0),
					ECC_Visibility, Params);
			}
			const bool bDiggable = bGround && SaltDig::IsDiggable(Hit);
			const FName First = bDiggable ? Ground->DigAt(Player, Hit.ImpactPoint) : NAME_None;
			const FName Second = bDiggable ? Ground->DigAt(Player, Hit.ImpactPoint + FVector(20.0, 0.0, 0.0)) : NAME_None;
			Report(TEXT("GroundDig"), bDiggable && !First.IsNone() && Second.IsNone() && Satchel->CountOf(First) > 0,
				FString::Printf(TEXT("diggable=%d mesh=%s first=%s second=%s"), bDiggable ? 1 : 0,
				Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("-"), *First.ToString(), *Second.ToString()));
		}

		// The save keeps the spot's stage and the shovel.
		Saves->Slot = TEXT("SaltDevTest");
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		const bool bSaved = Saves->SaveGame(World, TEXT("test"));
		Saves->SetDigStage(Def->Id, 0);
		Spot->SetStage(0, true);
		Player->SetOwnedTools({ TEXT("Compass") });
		const bool bLoaded = bSaved && Saves->LoadGame(World);
		Report(TEXT("DigSaved"), bLoaded && Spot->GetStage() == 2 && Player->IsToolOwned(SaltDig::ShovelTool), FString::Printf(
			TEXT("saved=%d loaded=%d stage=%d shovel=%d"), bSaved ? 1 : 0, bLoaded ? 1 : 0, Spot->GetStage(),
			Player->IsToolOwned(SaltDig::ShovelTool) ? 1 : 0));
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = OwnSlot;

		// Put everything back.
		Saves->SetDigStage(Def->Id, OwnStage);
		Spot->SetStage(OwnStage, true);
		Saves->SetSalt(OwnSalt);
		Satchel->SetSlots(OwnSlots);
		Player->SetOwnedTools(OwnTools);
		Player->SetHeldTool(OwnHeld);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST DigSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunDigTests(
		TEXT("sb.Dev.RunDigTests"),
		TEXT("D73: checks digging (spots placed, the keeper's shovel, digging up and taking a find, loose ground, the save)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunDigTests));

	FAutoConsoleCommandWithWorldAndArgs CmdGiveTool(
		TEXT("sb.Dev.GiveTool"),
		TEXT("D73: sb.Dev.GiveTool <Tool> [0] (own a tool of the wheel, or with 0 lose it), e.g. sb.Dev.GiveTool Shovel"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			if (ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr; Player && Args.Num() > 0)
			{
				Player->SetToolOwned(FName(*Args[0]), Args.Num() < 2 || FCString::Atoi(*Args[1]) != 0);
			}
		}));
}
#endif

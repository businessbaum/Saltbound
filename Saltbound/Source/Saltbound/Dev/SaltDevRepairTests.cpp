// Saltbound - checks of the ship repair system (sb.Dev.RunRepairTests, D74): knocks make holes, holes leak and the water
// weighs her down, patching takes driftwood and nails, bailing, a torn sail and its mending, the Shipwright's repair, the save.
// The boat, the satchel and the save slot are put back afterwards.

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Boat/SaltHullShape.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunRepairTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		ASaltBoat* Boat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			Boat = *It;
			break;
		}
		USaltHullDamageComponent* Damage = Boat ? Boat->GetHullDamage() : nullptr;
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		IConsoleVariable* DamageOn = IConsoleManager::Get().FindConsoleVariable(TEXT("sb.Boat.Damage"));
		if (!Damage || !Saves || !Satchel || !DamageOn)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST RepairSuite FAIL reason=missing_boat_damage_save_or_player"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST RepairSuite DONE passed=0 failed=1"));
			return;
		}
		const TArray<FSaltHullHole> OwnHoles = Damage->GetHoles();
		const float OwnWater = Damage->GetWaterKg();
		const int32 OwnTear = Damage->GetSailTear();
		const TArray<FSaltItemStack> OwnSlots = Satchel->GetSlots();
		const FString OwnSlot = Saves->Slot;
		const int32 OwnDamageOn = DamageOn->GetInt();
		DamageOn->Set(1, ECVF_SetByConsole);
		Damage->RepairAll();

		// A knock faster than HoleSpeed springs a plank on the side it came from; the next frames of the same knock do not.
		Damage->Knock(FVector(2.0, 1.45, 0.6), 1.0f);
		const int32 AfterSoft = Damage->GetHoles().Num();
		Damage->Knock(FVector(2.0, 1.45, 0.6), 3.5f);
		Damage->Knock(FVector(2.1, 1.45, 0.6), 3.6f);
		const TArray<FSaltHullHole>& Holes = Damage->GetHoles();
		Report(TEXT("KnockHoles"), AfterSoft == 0 && Holes.Num() == 1 && Holes[0].Side == 1 && Holes[0].Size == 2,
			FString::Printf(TEXT("soft=%d holes=%d side=%d size=%d"), AfterSoft, Holes.Num(), Holes.Num() ? Holes[0].Side : 0,
			Holes.Num() ? Holes[0].Size : 0));

		// It leaks; the water weighs her down and slows her.
		const float Rate = Damage->GetLeakRate();
		Damage->RestoreState(Damage->GetHoles(), 1200.f, 0);
		Report(TEXT("LeakAndWeight"), FMath::IsNearlyEqual(Rate, (float)Damage->LeakLitresPerSecond.Y) && FMath::IsNearlyEqual(Boat->BilgeWaterKg, 1200.f)
			&& Boat->SailCondition < 1.f && Boat->SailCondition > 0.85f, FString::Printf(TEXT("leak=%.1f l/s bilge=%.0f kg drive=%.3f"), Rate,
			Boat->BilgeWaterKg, Boat->SailCondition));

		// Patching takes a piece of driftwood and two nails.
		TArray<FSaltItemStack> Empty;
		Empty.SetNum(OwnSlots.Num());
		Satchel->SetSlots(Empty);
		const bool bWithout = Damage->PatchHole(0, Player);
		Satchel->Add(TEXT("Driftwood"), 1);
		Satchel->Add(TEXT("IronNails"), 3);
		const bool bWith = Damage->PatchHole(0, Player);
		Report(TEXT("Patch"), !bWithout && bWith && Damage->CountOpenHoles() == 0 && Damage->GetLeakRate() == 0.f
			&& Satchel->CountOf(TEXT("Driftwood")) == 0 && Satchel->CountOf(TEXT("IronNails")) == 1 && Damage->GetHoles().Num() == 1,
			FString::Printf(TEXT("without=%d with=%d open=%d leak=%.1f wood=%d nails=%d patches=%d"), bWithout ? 1 : 0, bWith ? 1 : 0,
			Damage->CountOpenHoles(), Damage->GetLeakRate(), Satchel->CountOf(TEXT("Driftwood")), Satchel->CountOf(TEXT("IronNails")),
			Damage->GetHoles().Num()));

		// A bucketful at a time.
		const float Before = Damage->GetWaterKg();
		Damage->Bail(Damage->BucketLitres);
		Report(TEXT("Bail"), FMath::IsNearlyEqual(Damage->GetWaterKg(), Before - Damage->BucketLitres), FString::Printf(TEXT("%.0f -> %.0f kg"),
			Before, Damage->GetWaterKg()));

		// Torn: less drive; mended with sailcloth; shreds only at the Slip.
		Damage->RestoreState(Damage->GetHoles(), 0.f, 0);
		Damage->TearSail();
		const float Torn = Boat->SailCondition;
		const bool bNoCloth = Damage->MendSail(Player);
		Satchel->Add(TEXT("Canvas"), 1);
		const bool bMended = Damage->MendSail(Player);
		Damage->TearSail();
		Damage->TearSail();
		Damage->TearSail();
		Satchel->Add(TEXT("Canvas"), 1);
		const bool bShreds = Damage->MendSail(Player);
		Report(TEXT("SailTearMend"), Torn < 0.8f && !bNoCloth && bMended && Damage->GetSailTear() == 3 && !bShreds, FString::Printf(
			TEXT("torn=%.2f noCloth=%d mended=%d tear=%d shredsMended=%d"), Torn, bNoCloth ? 1 : 0, bMended ? 1 : 0, Damage->GetSailTear(),
			bShreds ? 1 : 0));

		// The Shipwright: as new.
		Damage->RepairAll();
		Report(TEXT("RepairAll"), Damage->GetHoles().Num() == 0 && Damage->GetWaterKg() == 0.f && Damage->GetSailTear() == 0
			&& Boat->BilgeWaterKg == 0.f && FMath::IsNearlyEqual(Boat->SailCondition, 1.f), FString::Printf(TEXT("holes=%d water=%.0f tear=%d drive=%.2f"),
			Damage->GetHoles().Num(), Damage->GetWaterKg(), Damage->GetSailTear(), Boat->SailCondition));

		// Never behind a fitting while a clear spot is left (seen in play: a hole on the pin rail, starboard 0.7-1.6 m); with none
		// left on that side, a knock opens the nearest hole wider instead (only when none can widen does one go behind a fitting).
		Damage->AddHole(1.2f, 1, 1);
		const bool bClearFirst = Damage->GetHoles().Num() == 1 && Damage->IsHoleSpotFree(Damage->GetHoles()[0].X, 1, 0.335f);
		const float FirstX = Damage->GetHoles().Num() ? Damage->GetHoles()[0].X : 0.f;
		int32 Clear = 0;
		for (int32 K = 0; K < 4; ++K)
		{
			Damage->AddHole(1.2f, 1, 1);
		}
		int32 Widened = 0;
		for (const FSaltHullHole& H : Damage->GetHoles())
		{
			Clear += Damage->IsHoleSpotFree(H.X, H.Side, 0.335f) ? 1 : 0;     // each was placed while still small
			Widened += H.Size > 1 ? 1 : 0;
		}
		Report(TEXT("HolesClearOfFittings"), bClearFirst && Clear == Damage->GetHoles().Num() && Widened > 0, FString::Printf(
			TEXT("first at x=%.1f clear=%d; after 5 knocks at 1.2 m: holes=%d all clear=%d widened=%d"), FirstX, bClearFirst ? 1 : 0,
			Damage->GetHoles().Num(), Clear == Damage->GetHoles().Num() ? 1 : 0, Widened));
		Damage->RepairAll();

		// The save keeps the holes and patches, the water and the tears.
		Damage->AddHole(-2.0f, -1, 3);
		Damage->AddHole(1.5f, 1, 1);
		Damage->PatchHole(1, Player, true);
		Damage->RestoreState(Damage->GetHoles(), 333.f, 1);
		Saves->Slot = TEXT("SaltDevTest");
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		const bool bSaved = Saves->SaveGame(World, TEXT("test"));
		Damage->RepairAll();
		const bool bLoaded = bSaved && Saves->LoadGame(World);
		const TArray<FSaltHullHole>& Loaded = Damage->GetHoles();
		Report(TEXT("RepairSaved"), bLoaded && Loaded.Num() == 2 && Loaded[0].Size == 3 && !Loaded[0].bPatched && Loaded[1].bPatched
			&& FMath::IsNearlyEqual(Damage->GetWaterKg(), 333.f, 1.f) && Damage->GetSailTear() == 1, FString::Printf(
			TEXT("loaded=%d holes=%d water=%.0f tear=%d"), bLoaded ? 1 : 0, Loaded.Num(), Damage->GetWaterKg(), Damage->GetSailTear()));
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = OwnSlot;

		// Put everything back.
		Damage->RestoreState(OwnHoles, OwnWater, OwnTear);
		Satchel->SetSlots(OwnSlots);
		DamageOn->Set(OwnDamageOn, ECVF_SetByConsole);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST RepairSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunRepairTests(
		TEXT("sb.Dev.RunRepairTests"),
		TEXT("D74: checks the ship repair system (knocks, leaks, patching, bailing, the sail, the Shipwright, the save)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunRepairTests));

	FAutoConsoleCommandWithWorldAndArgs CmdHull(
		TEXT("sb.Boat.Hull"),
		TEXT("D74: sb.Boat.Hull hole <x m> <side 1|-1> <size 1-3> | water <kg> | tear | repair | info | spots (where a hole may go)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<ASaltBoat> It(World); It; ++It)
			{
				USaltHullDamageComponent* D = It->GetHullDamage();
				if (!D || Args.Num() == 0)
				{
					continue;
				}
				if (Args[0] == TEXT("hole") && Args.Num() >= 4)
				{
					D->AddHole(FCString::Atof(*Args[1]), FCString::Atoi(*Args[2]), FCString::Atoi(*Args[3]));
				}
				else if (Args[0] == TEXT("water") && Args.Num() >= 2)
				{
					D->RestoreState(D->GetHoles(), FCString::Atof(*Args[1]), D->GetSailTear());
				}
				else if (Args[0] == TEXT("tear"))
				{
					D->TearSail();
				}
				else if (Args[0] == TEXT("repair"))
				{
					D->RepairAll();
				}
				else if (Args[0] == TEXT("spots"))
				{
					// Every 0.1 m along both bulwarks: '.' free, '#' behind a fitting, ' ' a boarding gap; and how far things stand out.
					for (const int32 Side : { 1, -1 })
					{
						FString Map, Out;
						for (int32 I = -40; I <= 39; ++I)
						{
							const float X = I * 0.1f;
							float Protrusion = 0.f;
							const bool bGap = FMath::Abs(X) < SaltHull::GateHalfWidth + 0.25f;
							const bool bFree = D->IsHoleSpotFree(X, Side, 0.335f, &Protrusion);
							Map += bGap ? TEXT(" ") : (bFree ? TEXT(".") : TEXT("#"));
							Out += FString::Printf(TEXT("%.0f "), Protrusion);
						}
						UE_LOG(LogSaltbound, Display, TEXT("SB_HULL_SPOTS side=%d x=-4..3.9 [%s]"), Side, *Map);
						UE_LOG(LogSaltbound, Display, TEXT("SB_HULL_SPOTS side=%d protrusion_cm %s"), Side, *Out);
					}
				}
				UE_LOG(LogSaltbound, Display, TEXT("SB_HULL holes=%d open=%d water=%.0f kg (%.0f%%) leak=%.1f l/s tear=%d drive=%.2f"),
					D->GetHoles().Num(), D->CountOpenHoles(), D->GetWaterKg(), D->GetFill() * 100.f, D->GetLeakRate(), D->GetSailTear(),
					D->GetDriveFactor());
			}
		}));
}
#endif

// Saltbound - satchel checks (sb.Dev.RunInventoryTests, D50): stacking, a pickup, the satchel screen and its map, and
// saving the satchel (in a test slot; the player's own satchel and save are restored afterwards).

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltItems.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltSatchelWidget.h"
#include "UI/SaltStoreWidget.h"
#include "World/SaltPickup.h"

#if !UE_BUILD_SHIPPING
namespace
{
	/**
	 * The UI checks that need time (D55): a hold fills, drains when let go early and fires when full (raising the anchor at
	 * the windlass); the save sign shows for a save, stays at least 1.4 s and hides after it. They run on a ticker after the
	 * instant checks and log the suite's DONE line at the end.
	 */
	void RunTimedUIChecks(UWorld* World, int32 PassedSoFar, int32 FailedSoFar)
	{
		struct FState
		{
			TWeakObjectPtr<UWorld> World;
			int32 Passed = 0;
			int32 Failed = 0;
			float Clock = 0.f;
			int32 Step = 0;
			float StepAt = 0.f;
			float Value = 0.f;
			bool bFlag = false;
			FString OwnSlot;
			ESaltBoatHold OwnHold = ESaltBoatHold::Moored;
		};
		TSharedRef<FState> S = MakeShared<FState>();
		S->World = World;
		S->Passed = PassedSoFar;
		S->Failed = FailedSoFar;
		auto Report = [S](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? S->Passed : S->Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S, Report](float Dt) -> bool
		{
			UWorld* W = S->World.Get();
			ASaltPlayerController* PC = W ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(W, 0)) : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(W); W && It; ++It) { Boat = *It; break; }
			USaltHUDWidget* Hud = PC ? PC->GetHUDWidget() : nullptr;
			USaltSaveSubsystem* Saves = W ? USaltSaveSubsystem::Get(W) : nullptr;
			if (!PC || !P || !Boat || !Hud || !Saves)
			{
				Report(TEXT("UITimedChecks"), false, TEXT("no world, player, boat, HUD or saves"));
				UE_LOG(LogSaltbound, Display, TEXT("SB_TEST InventorySuite DONE passed=%d failed=%d"), S->Passed, S->Failed);
				return false;
			}
			S->Clock += Dt;
			const float T = S->Clock - S->StepAt;
			auto Next = [&]() { ++S->Step; S->StepAt = S->Clock; };
			auto Aim = [&]()
			{
				const FVector Target = Boat->GetWindlass()->GetComponentLocation();
				PC->SetControlRotation((Target - P->GetCamera()->GetComponentLocation()).Rotation());
			};
			switch (S->Step)
			{
			case 0:
			{
				// Anchor the boat where it lies and stand at the windlass, looking at it.
				S->OwnHold = Boat->GetHold();
				Boat->SetHold(ESaltBoatHold::Anchored);
				const FVector Local(2.95f * 100.f, 0.55f * 100.f, (SaltHull::SheerZ(2.95f) + 0.05f) * 100.f);
				P->TeleportFeetTo(Boat->GetActorTransform().TransformPosition(Local), Boat->GetYawDeg());
				Next();
				break;
			}
			case 1:
				Aim();
				if (T > 1.2f)
				{
					P->DebugInteract();
					Next();
				}
				break;
			case 2:
				Aim();
				S->Value = FMath::Max(S->Value, Hud->GetShownHoldProgress());
				if (T > 0.4f)
				{
					const float Prog = P->GetHoldProgress();
					Report(TEXT("HoldFills"), Prog > 0.3f && Prog < 0.75f && S->Value > 0.2f && Boat->GetHold() == ESaltBoatHold::Anchored,
						FString::Printf(TEXT("after 0.4 s of 0.8: progress=%.2f inkShown=%.2f stillAnchored=%d prompt=%s"), Prog, S->Value,
							Boat->GetHold() == ESaltBoatHold::Anchored ? 1 : 0, *Hud->GetShownPrompt().ToString()));
					P->DebugInteractRelease();
					Next();
				}
				break;
			case 3:
				Aim();
				if (T > 0.35f)
				{
					Report(TEXT("HoldDrains"), P->GetHoldProgress() < 0.01f && Boat->GetHold() == ESaltBoatHold::Anchored,
						FString::Printf(TEXT("0.35 s after letting go: progress=%.2f stillAnchored=%d"), P->GetHoldProgress(),
							Boat->GetHold() == ESaltBoatHold::Anchored ? 1 : 0));
					P->DebugInteract();
					Next();
				}
				break;
			case 4:
				Aim();
				if (Boat->GetHold() != ESaltBoatHold::Anchored && !S->bFlag)
				{
					S->bFlag = true;
					S->Value = T;
				}
				if (T > 1.4f)
				{
					P->DebugInteractRelease();
					Report(TEXT("HoldFires"), S->bFlag && S->Value > 0.7f && S->Value < 1.0f,
						FString::Printf(TEXT("anchor raised=%d after %.2f s of holding (0.8 s)"), S->bFlag ? 1 : 0, S->Value));
					// A save in the test slot: the sign shows.
					S->OwnSlot = Saves->Slot;
					Saves->Slot = TEXT("SaltDevTest");
					Saves->SaveGame(W, TEXT("UITest"));
					S->Value = 0.f;
					S->bFlag = false;
					Next();
				}
				break;
			case 5:
				if (T > 0.3f && !S->bFlag)
				{
					S->bFlag = true;
					S->Value = Hud->GetSaveSignOpacity();
				}
				if (T > 1.3f)
				{
					const float Mid = Hud->GetSaveSignOpacity();
					Report(TEXT("SaveSignShows"), S->Value > 0.9f && Mid > 0.9f, FString::Printf(TEXT("opacity at 0.3 s=%.2f, at 1.3 s=%.2f"), S->Value, Mid));
					Next();
				}
				break;
			case 6:
				if (T > 1.6f)
				{
					Report(TEXT("SaveSignHides"), Hud->GetSaveSignOpacity() < 0.05f, FString::Printf(TEXT("opacity 2.9 s after the save=%.2f"),
						Hud->GetSaveSignOpacity()));
					UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
					Saves->Slot = S->OwnSlot;
					Boat->SetHold(S->OwnHold);
					UE_LOG(LogSaltbound, Display, TEXT("SB_TEST InventorySuite DONE passed=%d failed=%d"), S->Passed, S->Failed);
					return false;
				}
				break;
			default:
				return false;
			}
			return true;
		}));
	}

	void RunInventoryTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
		ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		if (!Satchel || !Saves)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST InventorySuite FAIL reason=no_player_satchel_or_saves"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST InventorySuite DONE passed=0 failed=1"));
			return;
		}
		const TArray<FSaltItemStack> Own = Satchel->GetSlots();

		// Stacking: shells stack to 30, driftwood to 20; a full satchel takes no more.
		Satchel->SetSlots({});
		const int32 Shells = Satchel->Add(TEXT("Seashell"), 25);
		const int32 Wood = Satchel->Add(TEXT("Driftwood"), 45);
		int32 Used = 0;
		for (const FSaltItemStack& S : Satchel->GetSlots())
		{
			Used += S.IsEmpty() ? 0 : 1;
		}
		const int32 Removed = Satchel->Remove(TEXT("Driftwood"), 22);
		Report(TEXT("SatchelStacks"), Shells == 25 && Wood == 45 && Used == 4 && Removed == 22 && Satchel->CountOf(TEXT("Driftwood")) == 23,
			FString::Printf(TEXT("shells=%d wood=%d slotsUsed=%d removed=%d woodLeft=%d"), Shells, Wood, Used, Removed, Satchel->CountOf(TEXT("Driftwood"))));
		Satchel->SetSlots({});
		const int32 Fitted = Satchel->Add(TEXT("AncientShard"), 1000);
		const int32 Refused = Satchel->Add(TEXT("Seashell"), 1);
		Report(TEXT("SatchelFull"), Fitted == USaltInventoryComponent::Capacity * 5 && Refused == 0,
			FString::Printf(TEXT("fitted=%d (24 x 5) refusedWhenFull=%d"), Fitted, Refused));

		// A pickup: taken into the satchel, gone from the world, remembered for the save.
		Satchel->SetSlots({});
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASaltPickup* Pickup = World->SpawnActor<ASaltPickup>(Player->GetActorLocation() + Player->GetActorForwardVector() * 120.0, FRotator::ZeroRotator, Params);
		Pickup->ItemId = TEXT("GlassFloat");
		Pickup->PickupId = TEXT("DevTest_GlassFloat");
		const bool bTaken = Pickup->Collect(Player);
		Report(TEXT("PickupCollects"), bTaken && Satchel->CountOf(TEXT("GlassFloat")) == 1 && Pickup->IsCollected() && Pickup->IsHidden()
			&& Saves->IsCollected(TEXT("DevTest_GlassFloat")), FString::Printf(TEXT("taken=%d floats=%d hidden=%d remembered=%d"),
			bTaken ? 1 : 0, Satchel->CountOf(TEXT("GlassFloat")), Pickup->IsHidden() ? 1 : 0, Saves->IsCollected(TEXT("DevTest_GlassFloat")) ? 1 : 0));

		// The satchel screen: inventory page with the float in slot 0, then the map with the player's marker.
		PC->SetSatchelOpen(true, 0);
		USaltSatchelWidget* W = PC->GetSatchel();
		const bool bOpen = PC->IsSatchelOpen() && PC->bShowMouseCursor && !World->IsPaused();
		if (W)
		{
			W->SelectSlot(1);
			W->SelectSlot(0);
		}
		const FString Shown = W ? W->GetSelectedName().ToString() : FString();
		Report(TEXT("SatchelOpens"), bOpen && W && Shown == SaltItems::Find(TEXT("GlassFloat"))->Name.ToString(),
			FString::Printf(TEXT("open=%d cursor=%d paused=%d selected=%s"), PC->IsSatchelOpen() ? 1 : 0, PC->bShowMouseCursor ? 1 : 0,
				World->IsPaused() ? 1 : 0, *Shown));
		if (W)
		{
			W->SetPage(USaltSatchelWidget::Map);
			W->SetChart(0);
			const FVector2D Expected = W->WorldToChart(Player->GetActorLocation());
			const FVector2D Shown2 = W->GetPlayerMarkerPosition();
			const bool bOnChart = Expected.X > 14.0 && Expected.Y > 14.0 && Expected.X < USaltSatchelWidget::MapSize - 14.0 && Expected.Y < USaltSatchelWidget::MapSize - 14.0;
			Report(TEXT("MapMarksPlayer"), W->GetPage() == USaltSatchelWidget::Map && bOnChart && FVector2D::Distance(Expected, Shown2) < 1.0,
				FString::Printf(TEXT("page=%d expected=(%.1f, %.1f) marker=(%.1f, %.1f)"), W->GetPage(), Expected.X, Expected.Y, Shown2.X, Shown2.Y));
		}
		PC->SetSatchelOpen(false);
		Report(TEXT("SatchelCloses"), !PC->IsSatchelOpen() && !PC->bShowMouseCursor, FString::Printf(TEXT("open=%d cursor=%d"),
			PC->IsSatchelOpen() ? 1 : 0, PC->bShowMouseCursor ? 1 : 0));

		// The boat's stores (D52): the barrel's lid opens the store screen; a click moves a stack across and back.
		ASaltBoat* Boat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			Boat = *It;
			break;
		}
		USaltInventoryComponent* Barrel = Boat ? Boat->GetStore(false) : nullptr;
		USaltInventoryComponent* Crate = Boat ? Boat->GetStore(true) : nullptr;
		const TArray<FSaltItemStack> OwnBarrel = Barrel ? Barrel->GetSlots() : TArray<FSaltItemStack>();
		const TArray<FSaltItemStack> OwnCrate = Crate ? Crate->GetSlots() : TArray<FSaltItemStack>();
		if (Boat && Barrel && Crate)
		{
			Report(TEXT("BoatStores"), Barrel->GetCapacity() == 10 && Crate->GetCapacity() == 10 && Crate->CountOf(TEXT("Driftwood")) >= 4
				&& Barrel->GetStorageId() == TEXT("Boat_Barrel"), FString::Printf(TEXT("barrelSlots=%d crateSlots=%d crateWood=%d"),
				Barrel->GetCapacity(), Crate->GetCapacity(), Crate->CountOf(TEXT("Driftwood"))));
			Barrel->SetSlots({});
			Boat->GetStoreLid(false)->Interact(Player);
			USaltStoreWidget* SW = PC->GetStoreWidget();
			const bool bStoreOpen = PC->IsStoreOpen() && PC->bShowMouseCursor && SW && SW->GetStore() == Barrel;
			const int32 FloatsBefore = Satchel->CountOf(TEXT("GlassFloat"));
			if (SW)
			{
				SW->ClickSatchelSlot(0);
			}
			const int32 InBarrel = Barrel->CountOf(TEXT("GlassFloat"));
			const int32 LeftInSatchel = Satchel->CountOf(TEXT("GlassFloat"));
			if (SW)
			{
				SW->ClickStoreSlot(0);
			}
			Report(TEXT("StoreMovesItems"), bStoreOpen && FloatsBefore == 1 && InBarrel == 1 && LeftInSatchel == 0
				&& Satchel->CountOf(TEXT("GlassFloat")) == 1 && Barrel->CountOf(TEXT("GlassFloat")) == 0,
				FString::Printf(TEXT("open=%d intoBarrel=%d satchelThen=%d back=%d"), bStoreOpen ? 1 : 0, InBarrel, LeftInSatchel,
					Satchel->CountOf(TEXT("GlassFloat"))));
			PC->CloseStore();
			Report(TEXT("StoreCloses"), !PC->IsStoreOpen() && !PC->bShowMouseCursor, FString::Printf(TEXT("open=%d"), PC->IsStoreOpen() ? 1 : 0));
			Barrel->Add(TEXT("Seashell"), 2);
		}
		else
		{
			Report(TEXT("BoatStores"), false, TEXT("no boat or stores"));
		}

		// Saved and loaded: the satchel's contents and the taken pickup.
		const FString OwnSlot = Saves->Slot;
		Saves->Slot = TEXT("SaltDevTest");
		Satchel->Add(TEXT("Seashell"), 3);
		Saves->SaveGame(World, TEXT("test"));
		Satchel->SetSlots({});
		Pickup->SetCollected(false);
		Saves->ClearCollected();
		if (Barrel)
		{
			Barrel->SetSlots({});
		}
		Saves->LoadGame(World);
		Report(TEXT("SatchelSaved"), Satchel->CountOf(TEXT("Seashell")) == 3 && Satchel->CountOf(TEXT("GlassFloat")) == 1 && Pickup->IsCollected()
			&& (!Barrel || Barrel->CountOf(TEXT("Seashell")) == 2), FString::Printf(TEXT("shells=%d floats=%d pickupGone=%d barrelShells=%d"),
				Satchel->CountOf(TEXT("Seashell")), Satchel->CountOf(TEXT("GlassFloat")), Pickup->IsCollected() ? 1 : 0,
				Barrel ? Barrel->CountOf(TEXT("Seashell")) : -1));
		if (Barrel && Crate)
		{
			Barrel->SetSlots(OwnBarrel);
			Crate->SetSlots(OwnCrate);
		}

		// Leave things as they were.
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = OwnSlot;
		Saves->ClearCollected();
		Pickup->Destroy();
		Satchel->SetSlots(Own);
		RunTimedUIChecks(World, Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdSatchel(
		TEXT("sb.Dev.Satchel"),
		TEXT("Open the satchel on a page: sb.Dev.Satchel 0 (inventory) | 1 (map) [chart 0 home | 1 sea] | off."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			if (!PC)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("off"))
			{
				PC->SetSatchelOpen(false);
				return;
			}
			PC->SetSatchelOpen(true, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
			if (Args.Num() > 1 && PC->GetSatchel())
			{
				PC->GetSatchel()->SetChart(FCString::Atoi(*Args[1]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGive(
		TEXT("sb.Dev.Give"),
		TEXT("Put items into the satchel: sb.Dev.Give <Driftwood|Seashell|RopeCoil|GlassFloat|AncientShard> [Count=1]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			if (Player && Player->GetInventory() && Args.Num() > 0)
			{
				const int32 Added = Player->GetInventory()->Add(FName(*Args[0]), Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1);
				UE_LOG(LogSaltbound, Display, TEXT("SB_GIVE %s added=%d"), *Args[0], Added);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdRunInventoryTests(
		TEXT("sb.Dev.RunInventoryTests"),
		TEXT("Checks the satchel: stacking, a pickup, the satchel screen and its map, and saving it (test slot)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunInventoryTests));
}
#endif

// Saltbound - satchel checks (sb.Dev.RunInventoryTests, D50): stacking, a pickup, the satchel screen and its map, and
// saving the satchel (in a test slot; the player's own satchel and save are restored afterwards).

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltItems.h"
#include "Core/SaltPlaces.h"
#include "Core/SaltSaveSubsystem.h"
#include "Core/SaltTools.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "InputMappingContext.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltHeldTools.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UI/SaltChartWidget.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltLandfallWidget.h"
#include "UI/SaltSatchelWidget.h"
#include "UI/SaltStoreWidget.h"
#include "UI/SaltToolWheelWidget.h"
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
			TArray<FSaltItemStack> OwnSatchel;
			TArray<FSaltItemStack> OwnCrate;
			FName OwnTool;
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
					// A flight into the crate: under way at once, landed after its ~0.25 s.
					S->OwnSatchel = P->GetInventory()->GetSlots();
					S->OwnCrate = Boat->GetStore(true)->GetSlots();
					P->GetInventory()->SetSlots({ { TEXT("GlassFloat"), 3 } });
					Boat->GetStore(true)->SetSlots({ { TEXT("Driftwood"), 4 }, { TEXT("RopeCoil"), 1 } });
					PC->OpenStore(Boat->GetStore(true));
					USaltStoreWidget* SW = PC->GetStoreWidget();
					S->bFlag = SW && SW->IsCrate() && SW->ClickSatchelSlot(0) == 3 && SW->IsFlying() && SW->GetFlightTargetSlot() == 2;
					Next();
				}
				break;
			case 7:
				if (T > 0.4f)
				{
					USaltStoreWidget* SW = PC->GetStoreWidget();
					const bool bLanded = SW && !SW->IsFlying();
					const int32 InCrate = Boat->GetStore(true)->GetSlots().IsValidIndex(2) ? Boat->GetStore(true)->GetSlots()[2].Count : 0;
					Report(TEXT("StoreFlightLands"), S->bFlag && bLanded && InCrate == 3,
						FString::Printf(TEXT("flew to compartment 2=%d landed after 0.4 s=%d floats there=%d"), S->bFlag ? 1 : 0, bLanded ? 1 : 0, InCrate));
					PC->CloseStore();
					P->GetInventory()->SetSlots(S->OwnSatchel);
					Boat->GetStore(true)->SetSlots(S->OwnCrate);
					// A landfall banner (Ruin Isle) runs its course.
					if (USaltLandfallWidget* L = Hud->GetLandfall())
					{
						L->Clear();
						L->Show(TEXT("RuinIsle"));
					}
					Next();
				}
				break;
			case 8:
				if (T > 1.7f)
				{
					USaltLandfallWidget* L = Hud->GetLandfall();
					const FString Line = L ? L->GetSubtitle().ToString() : FString();
					Report(TEXT("LandfallBanner"), L && L->IsShowing() && L->GetShownPlace() == TEXT("RuinIsle") && L->GetUnroll() > 0.99f && L->GetInk() > 0.99f
						&& Line == TEXT("1.7 km north-north-east of Saltbound Cove"), FString::Printf(TEXT("after 1.7 s: showing=%d unrolled=%.2f inked=%.2f line='%s'"),
							L && L->IsShowing() ? 1 : 0, L ? L->GetUnroll() : 0.f, L ? L->GetInk() : 0.f, *Line));
					Next();
				}
				break;
			case 9:
				if (T > 4.7f)
				{
					USaltLandfallWidget* L = Hud->GetLandfall();
					Report(TEXT("LandfallRollsUp"), L && !L->IsShowing(), FString::Printf(TEXT("6.4 s after it showed: showing=%d"), L && L->IsShowing() ? 1 : 0));
					// The held tools (D55): the compass first.
					S->OwnTool = P->GetHeldTool();
					P->SetHeldTool(TEXT("Compass"));
					Next();
				}
				break;
			case 10:
				if (T > 2.5f)
				{
					// The needle has settled on north; then the player turns a quarter and it settles again.
					S->Value = FVector::DotProduct(P->GetHeldTools()->GetNeedleNorth(), FVector::ForwardVector);
					PC->SetControlRotation(PC->GetControlRotation() + FRotator(0.f, 90.f, 0.f));
					Next();
				}
				break;
			case 11:
				if (T > 2.5f)
				{
					const float After = FVector::DotProduct(P->GetHeldTools()->GetNeedleNorth(), FVector::ForwardVector);
					Report(TEXT("HeldCompassNorth"), P->GetHeldTools()->GetRaise() > 0.99f && S->Value > 0.97f && After > 0.97f,
						FString::Printf(TEXT("needle . north: %.3f, after a quarter turn %.3f"), S->Value, After));
					P->SetHeldTool(TEXT("Spyglass"));
					Next();
				}
				break;
			case 12:
				if (T > 0.6f)
				{
					const float Fov = P->GetCamera()->FieldOfView;
					const float Look = P->GetHeldTools()->GetLookScale();
					const float Mask = Hud->GetSpyglassMaskOpacity();
					Report(TEXT("HeldSpyglass"), FMath::Abs(Fov - USaltHeldToolsComponent::SpyglassFOV) < 0.5f && Look < 0.3f && Mask > 0.9f,
						FString::Printf(TEXT("field of view %.1f, look speed x%.2f, round view %.2f"), Fov, Look, Mask));
					P->SetHeldTool(TEXT("Lantern"));
					Next();
				}
				break;
			case 13:
				if (T > 1.0f)
				{
					USaltHeldToolsComponent* H = P->GetHeldTools();
					const float Fov = P->GetCamera()->FieldOfView;
					Report(TEXT("HeldLantern"), H->GetRaise() > 0.99f && FMath::Abs(Fov - P->FieldOfView) < 0.5f && Hud->GetSpyglassMaskOpacity() < 0.01f,
						FString::Printf(TEXT("lantern raised %.2f, field of view back to %.1f (%.1f)"), H->GetRaise(), Fov, P->FieldOfView));
					// D58: E at the chart: the eyes glide to the sheet on the mast (the lantern in hand is lowered meanwhile).
					PC->OpenChart(Boat);
					Next();
				}
				break;
			case 14:
				if (T > 0.85f)
				{
					int32 SizeX = 0, SizeY = 0;
					PC->GetViewportSize(SizeX, SizeY);
					const FTransform Want = Boat->GetChartViewLocal(P->FieldOfView, SizeY > 0 ? (float)SizeX / SizeY : 16.f / 9.f) * Boat->Hull->GetComponentTransform();
					const float Off = (float)FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Want.GetLocation());
					const float Turn = FMath::RadiansToDegrees(PC->PlayerCameraManager->GetCameraRotation().Quaternion().AngularDistance(Want.GetRotation()));
					const USaltChartWidget* CW = PC->GetChartWidget();
					const float Ink = CW ? CW->GetInkOpacity() : 0.f;
					const FName Shown = P->GetHeldTools()->GetShownTool();
					Report(TEXT("ChartGlides"), PC->IsChartOpen() && P->GetViewFocusAlpha() > 0.999f && Off < 6.f && Turn < 1.f && Ink > 0.99f && Shown.IsNone(),
						FString::Printf(TEXT("after 0.85 s: at the chart %.1f cm, turned %.2f deg off, names inked %.2f, the lantern put away=%d"),
							Off, Turn, Ink, Shown.IsNone() ? 1 : 0));
					S->Value = (float)FVector::Dist(P->GetCamera()->GetComponentLocation(), Want.GetLocation());
					PC->CloseChart();
					Next();
				}
				break;
			case 15:
				if (T > 0.5f)
				{
					const float Back = (float)FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), P->GetCamera()->GetComponentLocation());
					const FName Shown = P->GetHeldTools()->GetShownTool();
					Report(TEXT("ChartGlidesBack"), !PC->IsChartOpen() && P->GetViewFocusAlpha() <= 0.f && Back < 40.f && !PC->bShowMouseCursor && Shown == TEXT("Lantern"),
						FString::Printf(TEXT("0.5 s after putting it back: focus %.2f, view %.1f cm from the eyes (they were %.0f cm from the chart view), in hand again: %s"),
							P->GetViewFocusAlpha(), Back, S->Value, *Shown.ToString()));
					P->SetHeldTool(S->OwnTool);
					// D58: at the steering oar the controls show in the key-scrap prompt.
					P->EnterStation(Boat->GetHelm());
					Next();
				}
				break;
			case 16:
				if (T > 0.6f)
				{
					const FString Controls = Hud->GetShownControls().ToString();
					Report(TEXT("HelmControls"), Controls == TEXT("Steer"), FString::Printf(TEXT("controls shown at the helm: '%s'"), *Controls));
					P->LeaveStation();
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
		PC->SetSatchelOpen(true);
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
		// D55: the tag describes the selected pocket (name, how many it holds and how many fit), an empty one says so; G drops
		// one in front of the player.
		if (W)
		{
			Satchel->Add(TEXT("Seashell"), 3);
			const int32 ShellSlot = Satchel->GetSlots().IndexOfByPredicate([](const FSaltItemStack& St) { return St.Id == TEXT("Seashell"); });
			W->SelectSlot(ShellSlot);
			const FString Name = W->GetTagLine(0).ToString(), Line = W->GetTagLine(1).ToString();
			W->SelectSlot(USaltInventoryComponent::Capacity - 1);
			const FString Empty = W->GetTagLine(0).ToString();
			Report(TEXT("SatchelTag"), Name == TEXT("Seashell") && Line.StartsWith(TEXT("3 in this pocket")) && Line.EndsWith(TEXT("30 fit"))
				&& Empty == TEXT("Empty pocket"), FString::Printf(TEXT("name=%s line=%s emptyPocket=%s"), *Name, *Line, *Empty));
			W->SelectSlot(ShellSlot);
			int32 PickupsBefore = 0;
			for (TActorIterator<ASaltPickup> It(World); It; ++It) { ++PickupsBefore; }
			const bool bDropped = W->DropSelected();
			int32 PickupsAfter = 0;
			ASaltPickup* Dropped = nullptr;
			for (TActorIterator<ASaltPickup> It(World); It; ++It) { ++PickupsAfter; Dropped = *It; }
			Report(TEXT("SatchelDrops"), bDropped && Satchel->CountOf(TEXT("Seashell")) == 2 && PickupsAfter == PickupsBefore + 1,
				FString::Printf(TEXT("dropped=%d shellsLeft=%d pickups %d -> %d"), bDropped ? 1 : 0, Satchel->CountOf(TEXT("Seashell")), PickupsBefore, PickupsAfter));
			if (bDropped && Dropped && Dropped != Pickup)
			{
				Dropped->Destroy();
			}
			Satchel->Remove(TEXT("Seashell"), 2);
			W->SelectSlot(0);

			// D58: drag and drop in the satchel: into an empty pocket, onto the same item (as much as fits), onto another item
			// (they change places); and which pocket lies under a point.
			const TArray<FSaltItemStack> Before = Satchel->GetSlots();
			Satchel->SetSlots({ { TEXT("Seashell"), 3 }, { TEXT("Driftwood"), 2 }, {}, { TEXT("Seashell"), 28 } });
			const TArray<FSaltItemStack>& Sl = Satchel->GetSlots();
			const int32 IntoEmpty = W->DropOnSlot(1, 2);
			const bool bEmpty = Sl[1].IsEmpty() && Sl[2].Id == TEXT("Driftwood") && Sl[2].Count == 2;
			const int32 Merged = W->DropOnSlot(0, 3);
			const bool bMerged = Sl[3].Count == 30 && Sl[0].Id == TEXT("Seashell") && Sl[0].Count == 1;
			const int32 Swapped = W->DropOnSlot(0, 2);
			const bool bSwapped = Sl[0].Id == TEXT("Driftwood") && Sl[0].Count == 2 && Sl[2].Id == TEXT("Seashell") && Sl[2].Count == 1;
			const int32 Under = W->SlotAt(FVector2D(401.f + 124.f * 2.f + 20.f, 616.f + 140.f - 15.f));
			const int32 Nothing = W->SlotAt(FVector2D(120.f, 120.f));
			Report(TEXT("SatchelDragDrop"), IntoEmpty == 2 && bEmpty && Merged == 2 && bMerged && Swapped == 1 && bSwapped && Under == 10 && Nothing == INDEX_NONE
				&& W->GetSelectedSlot() == 2, FString::Printf(TEXT("into an empty pocket %d (%d), merged %d (%d), changed places %d (%d), pocket under (669, 741) %d, "
				"off the bag %d, chosen %d"), IntoEmpty, bEmpty ? 1 : 0, Merged, bMerged ? 1 : 0, Swapped, bSwapped ? 1 : 0, Under, Nothing, W->GetSelectedSlot()));
			// The pointer's way (as the mouse): press on pocket 0, move to pocket 5 (it is dragged), let go there (it lies in 5);
			// then a drag let go off the bag leaves it where it was.
			Satchel->SetSlots({ { TEXT("Driftwood"), 2 } });
			const bool bPicked = W->PointerDown(USaltSatchelWidget::PocketAt(0));
			W->PointerMove(USaltSatchelWidget::PocketAt(5) + FVector2D(10.f, -8.f));
			const bool bDragging = W->IsDragging();
			W->PointerUp(USaltSatchelWidget::PocketAt(5) + FVector2D(10.f, -8.f));
			const bool bInFive = Sl[5].Id == TEXT("Driftwood") && Sl[0].IsEmpty() && !W->IsDragging();
			W->PointerDown(USaltSatchelWidget::PocketAt(5));
			W->PointerMove(FVector2D(150.f, 150.f));
			W->PointerUp(FVector2D(150.f, 150.f));
			const bool bStays = Sl[5].Id == TEXT("Driftwood") && Sl[5].Count == 2;
			Report(TEXT("SatchelPointerDrag"), bPicked && bDragging && bInFive && bStays, FString::Printf(TEXT("picked up=%d dragging=%d put in pocket 5=%d "
				"let go off the bag, still in 5=%d"), bPicked ? 1 : 0, bDragging ? 1 : 0, bInFive ? 1 : 0, bStays ? 1 : 0));
			Satchel->SetSlots(Before);
			W->SelectSlot(0);
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
			// D55: Shift-click moves one, a click the rest of the stack (into the same compartment); it flies there, and the
			// counts read "N / 24" and "M / 10".
			if (SW)
			{
				Satchel->Add(TEXT("Seashell"), 5);
				const int32 ShellSlot = Satchel->GetSlots().IndexOfByPredicate([](const FSaltItemStack& St) { return St.Id == TEXT("Seashell"); });
				const int32 One = SW->ClickSatchelSlot(ShellSlot, true);
				const int32 OneTarget = SW->GetFlightTargetSlot();
				const bool bFlies = SW->IsFlying() && SW->IsFlyingToStore();
				const int32 AfterOne = Barrel->CountOf(TEXT("Seashell"));
				const int32 Rest = SW->ClickSatchelSlot(ShellSlot);
				const int32 RestTarget = SW->GetFlightTargetSlot();
				const int32 Back = SW->ClickStoreSlot(RestTarget, true);
				const bool bBackFlies = SW->IsFlying() && !SW->IsFlyingToStore();
				Report(TEXT("StoreShiftMovesOne"), One == 1 && AfterOne == 1 && Rest == 4 && RestTarget == OneTarget && Back == 1 && bFlies && bBackFlies
					&& Satchel->CountOf(TEXT("Seashell")) == 1 && Barrel->CountOf(TEXT("Seashell")) == 4,
					FString::Printf(TEXT("one=%d (barrel %d) rest=%d same compartment=%d back one=%d satchel=%d barrel=%d flies=%d/%d"), One, AfterOne, Rest,
						RestTarget == OneTarget ? 1 : 0, Back, Satchel->CountOf(TEXT("Seashell")), Barrel->CountOf(TEXT("Seashell")), bFlies ? 1 : 0, bBackFlies ? 1 : 0));
				int32 Pockets = 0;
				for (const FSaltItemStack& St : Satchel->GetSlots())
				{
					Pockets += St.IsEmpty() ? 0 : 1;
				}
				const FString SatchelCount = SW->GetSatchelCountText().ToString(), StoreCount = SW->GetStoreCountText().ToString();
				Report(TEXT("StoreCounts"), SatchelCount == FString::Printf(TEXT("%d / 24"), Pockets) && StoreCount == TEXT("1 / 10"),
					FString::Printf(TEXT("satchel='%s' store='%s'"), *SatchelCount, *StoreCount));
				Satchel->Remove(TEXT("Seashell"), 1);
				Barrel->SetSlots({});
				// The cursor crosses from the satchel's right column to the barrel and back.
				SW->SetCursor(false, 5);
				SW->MoveCursor(FVector2D(1.f, 0.f));
				const bool bOnStore = SW->IsCursorOnStore();
				SW->MoveCursor(FVector2D(-1.f, 0.f));
				Report(TEXT("StoreCursor"), bOnStore && !SW->IsCursorOnStore(), FString::Printf(TEXT("right of dish 5 -> store=%d, back left -> satchel slot %d"),
					bOnStore ? 1 : 0, SW->GetCursorSlot()));

				// D58: drag and drop across: the stack into a chosen compartment, changing places inside the barrel, one (Shift)
				// back into a chosen pocket; the slot under a point on either side.
				const TArray<FSaltItemStack> SatchelBefore = Satchel->GetSlots();
				Satchel->SetSlots({ { TEXT("Seashell"), 2 } });
				Barrel->SetSlots({ {}, {}, {}, { TEXT("Driftwood"), 1 } });
				const TArray<FSaltItemStack>& Bs = Barrel->GetSlots();
				const int32 ToSeven = SW->DropOnSlot(false, 0, true, 7);
				const bool bSeven = Bs[7].Id == TEXT("Seashell") && Bs[7].Count == 2 && Satchel->GetSlots()[0].IsEmpty();
				const int32 Swap = SW->DropOnSlot(true, 7, true, 3);
				const bool bSwap = Bs[3].Id == TEXT("Seashell") && Bs[7].Id == TEXT("Driftwood");
				const int32 OneIn = SW->DropOnSlot(true, 3, false, 23, true);
				const bool bOne = Satchel->GetSlots()[23].Id == TEXT("Seashell") && Satchel->GetSlots()[23].Count == 1 && Bs[3].Count == 1;
				bool bSide = false, bSideB = true;
				int32 At = INDEX_NONE, AtB = INDEX_NONE;
				const bool bHitStore = SW->SlotAt(SW->SlotCentre(true, 4), bSide, At) && bSide && At == 4;
				const bool bHitSatchel = SW->SlotAt(SW->SlotCentre(false, 9), bSideB, AtB) && !bSideB && AtB == 9;
				Report(TEXT("StoreDragDrop"), ToSeven == 2 && bSeven && Swap == 2 && bSwap && OneIn == 1 && bOne && bHitStore && bHitSatchel,
					FString::Printf(TEXT("into compartment 7: %d (%d), changed places: %d (%d), one into pocket 23: %d (%d), slot under compartment 4 / pocket 9: %d / %d"),
						ToSeven, bSeven ? 1 : 0, Swap, bSwap ? 1 : 0, OneIn, bOne ? 1 : 0, bHitStore ? 1 : 0, bHitSatchel ? 1 : 0));
				// The pointer's way: a drag from pocket 0 into compartment 6; a click (no move) on it sends it back, flying.
				Satchel->SetSlots({ { TEXT("Seashell"), 2 } });
				Barrel->SetSlots({});
				const bool bPicked = SW->PointerDown(SW->SlotCentre(false, 0));
				SW->PointerMove(SW->SlotCentre(true, 6) + FVector2D(12.f, 6.f));
				const bool bDragging = SW->IsDragging();
				SW->PointerUp(SW->SlotCentre(true, 6) + FVector2D(12.f, 6.f));
				const bool bInSix = Bs[6].Id == TEXT("Seashell") && Bs[6].Count == 2 && !SW->IsDragging();
				SW->PointerDown(SW->SlotCentre(true, 6));
				SW->PointerUp(SW->SlotCentre(true, 6));
				const bool bClickedBack = Bs[6].IsEmpty() && Satchel->CountOf(TEXT("Seashell")) == 2 && SW->IsFlying() && !SW->IsFlyingToStore();
				Report(TEXT("StorePointerDrag"), bPicked && bDragging && bInSix && bClickedBack, FString::Printf(TEXT("picked up=%d dragging=%d into compartment 6=%d "
					"a click sends it back=%d"), bPicked ? 1 : 0, bDragging ? 1 : 0, bInSix ? 1 : 0, bClickedBack ? 1 : 0));
				Satchel->SetSlots(SatchelBefore);
				Barrel->SetSlots({});
			}
			PC->CloseStore();
			Report(TEXT("StoreCloses"), !PC->IsStoreOpen() && !PC->bShowMouseCursor, FString::Printf(TEXT("open=%d"), PC->IsStoreOpen() ? 1 : 0));
			Barrel->Add(TEXT("Seashell"), 2);
		}
		else
		{
			Report(TEXT("BoatStores"), false, TEXT("no boat or stores"));
		}

		// D55: landfall. An island is found once, within 300 m of its shore, and its banner shows.
		TArray<FName> OwnFound;
		for (const FSaltPlace& Pl : SaltPlaces::All())
		{
			if (Saves->IsFound(Pl.Id))
			{
				OwnFound.Add(Pl.Id);
			}
		}
		Saves->ClearFound();
		USaltLandfallSubsystem* Landfalls = World->GetSubsystem<USaltLandfallSubsystem>();
		USaltHUDWidget* Hud = PC->GetHUDWidget();
		if (Landfalls && Hud && Hud->GetLandfall())
		{
			Hud->GetLandfall()->Clear();
			// The Spire (420, 150 m, its shore 28 m round): 418 m from home's shore is too far; 250 m off its centre finds it.
			const FName AtHome = Landfalls->CheckAt(FVector::ZeroVector);
			const FVector Near((420.0 - 250.0) * 100.0, 150.0 * 100.0, 0.0);
			const FName First = Landfalls->CheckAt(Near);
			const FName Again = Landfalls->CheckAt(Near);
			const bool bBanner = Hud->GetLandfall()->IsShowing() && Hud->GetLandfall()->GetShownPlace() == TEXT("Spire");
			Report(TEXT("LandfallFoundOnce"), AtHome.IsNone() && First == TEXT("Spire") && Again.IsNone() && bBanner && Saves->IsFound(TEXT("Spire")),
				FString::Printf(TEXT("at home=%s near the Spire=%s again=%s banner=%d"), *AtHome.ToString(), *First.ToString(), *Again.ToString(), bBanner ? 1 : 0));
			Hud->GetLandfall()->Clear();
		}
		else
		{
			Report(TEXT("LandfallFoundOnce"), false, TEXT("no landfall subsystem or HUD"));
		}

		// D55: the tool wheel. Q and LB hold it open (Tab and I still open the satchel); the mouse or the stick aims at a tool
		// by its direction; letting go takes it; Esc cancels; keys 1 to 6 take one directly; a tool not owned is not taken.
		if (USaltToolWheelWidget* Wheel = Hud ? Hud->GetToolWheel() : nullptr)
		{
			USaltInputConfig* Cfg = PC->GetInputConfig();
			auto Mapped = [Cfg](const UInputAction* Action, const FKey& Key)
			{
				return Cfg->DefaultContext->GetMappings().ContainsByPredicate([&](const FEnhancedActionKeyMapping& M) { return M.Action == Action && M.Key == Key; });
			};
			Report(TEXT("WheelOnQ"), Mapped(Cfg->ToolWheel, EKeys::Q) && Mapped(Cfg->ToolWheel, EKeys::Gamepad_LeftShoulder) && !Mapped(Cfg->ToolWheel, EKeys::Tab)
				&& Mapped(Cfg->Inventory, EKeys::Tab) && Mapped(Cfg->Inventory, EKeys::I) && !Mapped(Cfg->Inventory, EKeys::M) && Cfg->Tools.Num() == 5
				&& Mapped(Cfg->Tools[0], EKeys::One) && Mapped(Cfg->Tools[4], EKeys::Five), TEXT("Q / LB: wheel; Tab / I: satchel; 1..5: tools; no map key"));
			const FName OwnHeld = Player->GetHeldTool();
			Player->SetHeldTool(NAME_None);
			PC->OpenToolWheel();
			const bool bOpened = PC->IsToolWheelOpen() && Wheel->IsOpen() && Wheel->GetSelected() == 4;
			PC->AimToolWheel(FVector2D(0.f, 60.f), false);          // the mouse up: the compass at the top
			const int32 Up = Wheel->GetSelected();
			PC->AimToolWheel(FVector2D(-300.f, -300.f), false);     // left and down: the satchel, lower left (126 degrees)
			const int32 LowerLeft = Wheel->GetSelected();
			PC->AimToolWheel(FVector2D(0.2f, 0.f), true);            // inside the stick's dead zone: no change
			const int32 Dead = Wheel->GetSelected();
			PC->AimToolWheel(FVector2D(0.87f, -0.5f), true);         // the stick right and down: the lantern, lower right (54)
			const int32 LowerRight = Wheel->GetSelected();
			PC->AimToolWheel(FVector2D(0.87f, 0.5f), true);          // right and up: the spyglass (-18)
			const int32 UpperRight = Wheel->GetSelected();
			PC->CloseToolWheel(true);
			const FName Taken = Player->GetHeldTool();
			Report(TEXT("WheelByAngle"), bOpened && Up == 0 && LowerLeft == 3 && Dead == 3 && LowerRight == 2 && UpperRight == 1,
				FString::Printf(TEXT("opened on empty hands=%d up=%d lower left=%d dead zone=%d lower right=%d upper right=%d"), bOpened ? 1 : 0, Up, LowerLeft,
					Dead, LowerRight, UpperRight));
			Report(TEXT("WheelTakesTool"), Taken == TEXT("Spyglass") && !PC->IsToolWheelOpen(), FString::Printf(TEXT("in hand=%s"), *Taken.ToString()));
			PC->OpenToolWheel();
			PC->AimToolWheel(FVector2D(0.f, 60.f), false);
			PC->HandleMenuKey();
			Report(TEXT("WheelCancels"), !PC->IsToolWheelOpen() && !PC->IsMenuOpen() && Player->GetHeldTool() == TEXT("Spyglass"),
				FString::Printf(TEXT("open=%d menu=%d in hand=%s"), PC->IsToolWheelOpen() ? 1 : 0, PC->IsMenuOpen() ? 1 : 0, *Player->GetHeldTool().ToString()));
			PC->SelectTool(2);
			const FName Three = Player->GetHeldTool();
			PC->SelectTool(4);
			const FName Five = Player->GetHeldTool();
			PC->SelectTool(3);
			const bool bSatchel = PC->IsSatchelOpen();
			PC->SetSatchelOpen(false);
			Report(TEXT("WheelKeys"), Three == TEXT("Lantern") && Five.IsNone() && bSatchel, FString::Printf(TEXT("3=%s 5=%s 4 opens the satchel=%d"),
				*Three.ToString(), *Five.ToString(), bSatchel ? 1 : 0));
			Player->SetToolOwned(TEXT("Compass"), false);
			PC->SelectTool(0);
			const bool bRefused = Player->GetHeldTool() != TEXT("Compass");
			Player->SetToolOwned(TEXT("Compass"), true);
			Report(TEXT("WheelNeedsTool"), bRefused && Player->IsToolOwned(TEXT("Compass")), FString::Printf(TEXT("a compass not owned taken=%d"), bRefused ? 0 : 1));
			Player->SetHeldTool(OwnHeld);
		}
		else
		{
			Report(TEXT("WheelOnQ"), false, TEXT("no HUD tool wheel"));
		}

		// D57: the chart on the mast follows the boat (its window centred on her, her heading), E looks at it closely.
		ASaltBoat* ChartBoat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It) { ChartBoat = *It; break; }
		if (ChartBoat && ChartBoat->GetMastChartUse())
		{
			const FVector2D C = ChartBoat->GetChartCentre();
			const float U = ChartBoat->GetMastChartParameter(TEXT("CentreU")), V = ChartBoat->GetMastChartParameter(TEXT("CentreV"));
			const float H = FMath::RadiansToDegrees(ChartBoat->GetMastChartParameter(TEXT("Heading")));
			Report(TEXT("MastChartFollowsBoat"), FMath::Abs(U - C.X) < 2e-4f && FMath::Abs(V - C.Y) < 2e-4f && FMath::Abs(FRotator::NormalizeAxis(H - ChartBoat->GetYawDeg())) < 1.f,
				FString::Printf(TEXT("chart centre (%.4f, %.4f) for the boat at (%.4f, %.4f), heading %.1f (%.1f)"), U, V, C.X, C.Y, H, ChartBoat->GetYawDeg()));
			ChartBoat->GetMastChartUse()->Interact(Player);
			USaltChartWidget* CW = PC->GetChartWidget();
			if (CW)
			{
				CW->Update();
			}
			const bool bChartOpen = PC->IsChartOpen() && CW && FVector2D::Distance(CW->GetShownCentre(), C) < 1e-3;
			const TArray<FName> Named = CW ? CW->GetNamedPlaces() : TArray<FName>();
			PC->CloseChart();
			Report(TEXT("MastChartLook"), bChartOpen && Named.Contains(TEXT("SaltboundCove")) && !Named.Contains(TEXT("RuinIsle")) && !PC->IsChartOpen(),
				FString::Printf(TEXT("open=%d names %s; closed=%d"), bChartOpen ? 1 : 0, *FString::JoinBy(Named, TEXT(","), [](const FName& N) { return N.ToString(); }),
					PC->IsChartOpen() ? 0 : 1));
		}
		else
		{
			Report(TEXT("MastChartFollowsBoat"), false, TEXT("no boat with a mast chart"));
		}

		// Saved and loaded: the satchel's contents and the taken pickup.
		const FString OwnSlot = Saves->Slot;
		Saves->Slot = TEXT("SaltDevTest");
		Satchel->Add(TEXT("Seashell"), 3);
		Saves->SaveGame(World, TEXT("test"));
		Satchel->SetSlots({});
		Pickup->SetCollected(false);
		Saves->ClearCollected();
		Saves->ClearFound();
		if (Barrel)
		{
			Barrel->SetSlots({});
		}
		Saves->LoadGame(World);
		Report(TEXT("SatchelSaved"), Satchel->CountOf(TEXT("Seashell")) == 3 && Satchel->CountOf(TEXT("GlassFloat")) == 1 && Pickup->IsCollected()
			&& (!Barrel || Barrel->CountOf(TEXT("Seashell")) == 2), FString::Printf(TEXT("shells=%d floats=%d pickupGone=%d barrelShells=%d"),
				Satchel->CountOf(TEXT("Seashell")), Satchel->CountOf(TEXT("GlassFloat")), Pickup->IsCollected() ? 1 : 0,
				Barrel ? Barrel->CountOf(TEXT("Seashell")) : -1));
		Report(TEXT("LandfallSaved"), Saves->IsFound(TEXT("Spire")) && !Saves->IsFound(TEXT("RuinIsle")), FString::Printf(TEXT("after loading: spire found=%d ruin isle found=%d"),
			Saves->IsFound(TEXT("Spire")) ? 1 : 0, Saves->IsFound(TEXT("RuinIsle")) ? 1 : 0));
		Saves->ClearFound();
		for (const FName& Id : OwnFound)
		{
			Saves->MarkFound(Id);
		}
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
		TEXT("Open the satchel: sb.Dev.Satchel [off]."),
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
			PC->SetSatchelOpen(true);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSatchelDemo(
		TEXT("sb.Dev.SatchelDemo"),
		TEXT("For captures: fill the satchel as in the design's picture (driftwood 20 and 7, seashells 30 and 12, rope 2, glass floats 3, ")
		TEXT("the ancient shard), open it and select the shard. sb.Dev.SatchelDemo off empties it back to two rope coils."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			USaltInventoryComponent* Satchel = P ? P->GetInventory() : nullptr;
			if (!Satchel)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("off"))
			{
				Satchel->SetSlots({ { TEXT("RopeCoil"), 2 } });
				PC->SetSatchelOpen(false);
				return;
			}
			Satchel->SetSlots({ { TEXT("Driftwood"), 20 }, { TEXT("Driftwood"), 7 }, { TEXT("Seashell"), 30 }, { TEXT("Seashell"), 12 },
				{ TEXT("RopeCoil"), 2 }, { TEXT("GlassFloat"), 3 }, { TEXT("AncientShard"), 1 } });
			PC->SetSatchelOpen(true);
			if (USaltSatchelWidget* W = PC->GetSatchel())
			{
				W->SelectSlot(6);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdStore(
		TEXT("sb.Dev.Store"),
		TEXT("Open a boat store: sb.Dev.Store crate|barrel [demo [t]] | off. demo fills the satchel and the store as in the design's ")
		TEXT("picture and sends the three glass floats on their way (held at t of the arc, 0..1, when given; the design shows 0.4)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); World && It; ++It) { Boat = *It; break; }
			if (!PC || !P || !Boat)
			{
				return;
			}
			PC->CloseStore();
			if (Args.Num() > 0 && Args[0] == TEXT("off"))
			{
				return;
			}
			PC->SetSatchelOpen(false);
			USaltInventoryComponent* Store = Boat->GetStore(Args.Num() == 0 || Args[0] != TEXT("barrel"));
			const bool bDemo = Args.Num() > 1 && Args[1] == TEXT("demo");
			if (bDemo)
			{
				P->GetInventory()->SetSlots({ { TEXT("Driftwood"), 20 }, { TEXT("Driftwood"), 7 }, { TEXT("Seashell"), 30 }, { TEXT("Seashell"), 12 },
					{ TEXT("RopeCoil"), 2 }, { TEXT("GlassFloat"), 3 }, { TEXT("AncientShard"), 1 } });
				Store->SetSlots({ { TEXT("Driftwood"), 4 }, { TEXT("RopeCoil"), 1 } });
			}
			PC->OpenStore(Store);
			USaltStoreWidget* W = PC->GetStoreWidget();
			if (W && bDemo)
			{
				W->HoldFlightAt(Args.Num() > 2 ? FCString::Atof(*Args[2]) : -1.f);
				W->ClickSatchelSlot(5);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdLandfall(
		TEXT("sb.Dev.Landfall"),
		TEXT("Show an island's landfall banner: sb.Dev.Landfall <Spire|RuinIsle|Mistholm|GullRock|Longshoal> [t] (held at t s; the design ")
		TEXT("shows the full banner, e.g. 2) | reset (forget the islands found)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			USaltHUDWidget* Hud = PC ? PC->GetHUDWidget() : nullptr;
			if (!Hud || !Hud->GetLandfall() || Args.Num() == 0)
			{
				return;
			}
			if (Args[0] == TEXT("reset"))
			{
				if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World))
				{
					Saves->ClearFound();
				}
				return;
			}
			Hud->GetLandfall()->Clear();
			Hud->GetLandfall()->HoldAt(Args.Num() > 1 ? FCString::Atof(*Args[1]) : -1.f);
			Hud->GetLandfall()->Show(FName(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWheel(
		TEXT("sb.Dev.Wheel"),
		TEXT("The tool wheel for captures: sb.Dev.Wheel 1 [n] opens it (on tool n, 1 to 6) and keeps it open; sb.Dev.Wheel 0 closes it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			if (!PC)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("0"))
			{
				PC->CloseToolWheel(false);
				return;
			}
			PC->OpenToolWheel();
			USaltToolWheelWidget* Wheel = PC->GetHUDWidget() ? PC->GetHUDWidget()->GetToolWheel() : nullptr;
			if (Wheel && Args.Num() > 1)
			{
				Wheel->SetSelected(FCString::Atoi(*Args[1]) - 1);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTool(
		TEXT("sb.Dev.Tool"),
		TEXT("Take a tool in hand (captures): sb.Dev.Tool Compass|Spyglass|Lantern|none."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			if (ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr)
			{
				Player->SetHeldTool(Args.Num() > 0 && Args[0] != TEXT("none") ? FName(*Args[0]) : NAME_None);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdChart(
		TEXT("sb.Dev.Chart"),
		TEXT("Look at the chart on the boat's mast: sb.Dev.Chart [off]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); World && It; ++It) { Boat = *It; break; }
			if (!PC || !Boat)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("off"))
			{
				PC->CloseChart();
				return;
			}
			PC->OpenChart(Boat);
		}));

	/** D58: a drag on the open satchel or store, for captures: sb.Dev.Drag <x> <y> <x2> <y2> presses at the first point
	 *  (1080p design coordinates) and holds the item at the second; sb.Dev.Drag up lets go where it is. */
	FAutoConsoleCommandWithWorldAndArgs CmdDrag(
		TEXT("sb.Dev.Drag"),
		TEXT("Drag on the open satchel or store (1080p coordinates): sb.Dev.Drag <x> <y> <x2> <y2> | up."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
			USaltSatchelWidget* W = PC && PC->IsSatchelOpen() ? PC->GetSatchel() : nullptr;
			USaltStoreWidget* SW = PC && PC->IsStoreOpen() ? PC->GetStoreWidget() : nullptr;
			static FVector2D Last = FVector2D::ZeroVector;
			if (Args.Num() == 1 && Args[0] == TEXT("up"))
			{
				if (W) { W->PointerUp(Last); }
				if (SW) { SW->PointerUp(Last); }
				return;
			}
			if (Args.Num() < 4)
			{
				return;
			}
			const FVector2D From(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
			Last = FVector2D(FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
			if (W && W->PointerDown(From)) { W->PointerMove(Last); }
			if (SW && SW->PointerDown(From)) { SW->PointerMove(Last); }
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

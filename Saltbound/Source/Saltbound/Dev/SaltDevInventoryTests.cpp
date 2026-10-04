// Saltbound - satchel checks (sb.Dev.RunInventoryTests, D50): stacking, a pickup, the satchel screen and its map, and
// saving the satchel (in a test slot; the player's own satchel and save are restored afterwards).

#include "Components/CapsuleComponent.h"
#include "Core/SaltItems.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UI/SaltSatchelWidget.h"
#include "World/SaltPickup.h"

#if !UE_BUILD_SHIPPING
namespace
{
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

		// Saved and loaded: the satchel's contents and the taken pickup.
		const FString OwnSlot = Saves->Slot;
		Saves->Slot = TEXT("SaltDevTest");
		Satchel->Add(TEXT("Seashell"), 3);
		Saves->SaveGame(World, TEXT("test"));
		Satchel->SetSlots({});
		Pickup->SetCollected(false);
		Saves->ClearCollected();
		Saves->LoadGame(World);
		Report(TEXT("SatchelSaved"), Satchel->CountOf(TEXT("Seashell")) == 3 && Satchel->CountOf(TEXT("GlassFloat")) == 1 && Pickup->IsCollected(),
			FString::Printf(TEXT("shells=%d floats=%d pickupGone=%d"), Satchel->CountOf(TEXT("Seashell")), Satchel->CountOf(TEXT("GlassFloat")),
				Pickup->IsCollected() ? 1 : 0));

		// Leave things as they were.
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = OwnSlot;
		Saves->ClearCollected();
		Pickup->Destroy();
		Satchel->SetSlots(Own);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST InventorySuite DONE passed=%d failed=%d"), Passed, Failed);
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

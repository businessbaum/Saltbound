// Saltbound - the first hour's prologue (FH1, D77).

#include "World/SaltPrologue.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Boat/SaltHullShape.h"
#include "Components/CapsuleComponent.h"
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
#include "World/SaltDig.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPickup.h"

namespace
{
	struct FBeachFind
	{
		const TCHAR* Id;
		const TCHAR* Item;
		double XM;
		double YM;
		float Yaw;
	};
	// Washed up round the wreck with the castaway (the beach teaches picking up and the satchel).
	const FBeachFind BeachFinds[] = {
		{ TEXT("PrologueDriftwoodA"), TEXT("Driftwood"), -63.6, -101.4, 40.f },
		{ TEXT("PrologueDriftwoodB"), TEXT("Driftwood"), -59.8, -107.6, 115.f },
		{ TEXT("PrologueRope"), TEXT("RopeCoil"), -65.2, -105.4, 10.f },
		{ TEXT("PrologueFloat"), TEXT("GlassFloat"), -66.4, -111.6, 0.f },
	};

	/** Where the sea chest stands on her deck (build_boat.py CHEST: port side aft, 0.8 x 0.52 x 0.42 m). */
	constexpr float ChestXM = -3.3f;
	constexpr float ChestYM = -0.6f;

	template <typename T>
	T* FindFirstActor(UWorld* World)
	{
		TActorIterator<T> It(World);
		return It ? *It : nullptr;
	}

	ASaltPlayerController* FirstController(UWorld* World)
	{
		return Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	}

	/** The island's ground under a point (the first diggable hit from above: sand and earth, not the wreck's planks), stepping
	 *  toward Toward until it is above the water. */
	bool GroundAt(UWorld* World, FVector2D AtM, const FVector2D& TowardM, FVector& Out)
	{
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const FVector XY(AtM.X * 100.0, AtM.Y * 100.0, 0.0);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltPrologueGround), true);
			FHitResult Hit;
			for (int32 Tries = 0; Tries < 6 && World->LineTraceSingleByChannel(Hit, XY + FVector(0.0, 0.0, 5000.0), XY - FVector(0.0, 0.0, 1000.0),
				ECC_Visibility, Params); ++Tries)
			{
				if (SaltDig::IsDiggable(Hit))
				{
					Out = Hit.ImpactPoint;
					return true;
				}
				Params.AddIgnoredComponent(Hit.GetComponent());
			}
			AtM += (TowardM - AtM).GetSafeNormal() * 1.0;
		}
		return false;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdPrologue(
		TEXT("sb.Story.Prologue"),
		TEXT("FH1 (D77): start the prologue here (the wreck beach at dawn, nothing in hand, the sail ripped). 'nowake' skips the view rising."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltPrologueSubsystem* P = World ? World->GetSubsystem<USaltPrologueSubsystem>() : nullptr)
			{
				P->Begin(!(Args.Num() > 0 && Args[0] == TEXT("nowake")));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBeats(
		TEXT("sb.Story.Beats"),
		TEXT("FH1 (D77): print the story's beats reached (sb.Story.Beats <Beat> records one now)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltPrologueSubsystem* P = World ? World->GetSubsystem<USaltPrologueSubsystem>() : nullptr;
			const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
			if (!P || !Saves)
			{
				return;
			}
			if (Args.Num() > 0)
			{
				P->Reach(FName(*Args[0]));
			}
			FString Line;
			for (const TPair<FName, float>& B : Saves->GetBeats())
			{
				Line += FString::Printf(TEXT("%s@%.2f "), *B.Key.ToString(), B.Value);
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_STORY beats=%d %s"), Saves->GetBeats().Num(), *Line);
		}));
}

bool USaltPrologueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USaltPrologueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltPrologueSubsystem, STATGROUP_Tickables);
}

void USaltPrologueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The prologue's places are on the home island: only the game's map has them.
	if (!InWorld.GetMapName().EndsWith(TEXT("L_VisualTest")))
	{
		return;
	}
	bStarted = true;
	PlaceBeachFinds();
	AttachChest();
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(&InWorld))
	{
		LoadedHandle = Saves->OnGameLoaded.AddUObject(this, &USaltPrologueSubsystem::ApplyState);
	}
	ApplyState();
}

void USaltPrologueSubsystem::Deinitialize()
{
	if (USaltSaveSubsystem* Saves = LoadedHandle.IsValid() ? USaltSaveSubsystem::Get(GetWorld()) : nullptr)
	{
		Saves->OnGameLoaded.Remove(LoadedHandle);
	}
	LoadedHandle.Reset();
	Super::Deinitialize();
}

void USaltPrologueSubsystem::PlaceBeachFinds()
{
	UWorld* World = GetWorld();
	const FVector2D Wake(SaltStory::WakeXM, SaltStory::WakeYM);
	int32 Placed = 0;
	for (const FBeachFind& F : BeachFinds)
	{
		FVector At;
		if (!GroundAt(World, FVector2D(F.XM, F.YM), Wake, At))
		{
			continue;
		}
		// Placed like the map's own pickups: the save remembers them by PickupId.
		ASaltPickup* Pickup = World->SpawnActorDeferred<ASaltPickup>(ASaltPickup::StaticClass(), FTransform(FRotator(0.f, F.Yaw, 0.f), At),
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pickup)
		{
			Pickup->ItemId = F.Item;
			Pickup->Count = 1;
			Pickup->PickupId = F.Id;
			UGameplayStatics::FinishSpawningActor(Pickup, FTransform(FRotator(0.f, F.Yaw, 0.f), At));
			++Placed;
		}
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_STORY beach finds placed=%d of %d"), Placed, (int32)UE_ARRAY_COUNT(BeachFinds));
}

void USaltPrologueSubsystem::AttachChest()
{
	ASaltBoat* Boat = FindFirstActor<ASaltBoat>(GetWorld());
	if (!Boat || !Boat->Hull)
	{
		return;
	}
	USaltInteractableComponent* C = NewObject<USaltInteractableComponent>(Boat, TEXT("SeaChestUse"));
	C->SetupAttachment(Boat->Hull);
	C->SetBoxExtent(FVector(44.f, 30.f, 26.f));
	C->SetRelativeLocation(FVector(ChestXM * 100.f, ChestYM * 100.f, SaltHull::SheerZ(ChestXM) * 100.f + 24.f));
	C->PromptText = NSLOCTEXT("SaltStory", "OpenChest", "Open the sea chest");
	C->PromptOffset = FVector(0.f, 0.f, 30.f);
	C->OnInteract.AddDynamic(this, &USaltPrologueSubsystem::HandleChest);
	C->RegisterComponent();
	Chest = C;
}

FVector USaltPrologueSubsystem::GetWakeSpot() const
{
	FVector At(SaltStory::WakeXM * 100.0, SaltStory::WakeYM * 100.0, 60.0);
	GroundAt(GetWorld(), FVector2D(SaltStory::WakeXM, SaltStory::WakeYM), FVector2D(-22.0, 12.0), At);
	return At;
}

float USaltPrologueSubsystem::Now() const
{
	const ASaltLightingRig* Rig = FindFirstActor<ASaltLightingRig>(GetWorld());
	return Rig ? Rig->GetDay() * 24.f + Rig->GetTimeOfDay() : 24.f + SaltStory::StartHour;
}

void USaltPrologueSubsystem::Reach(FName Beat)
{
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld()))
	{
		Saves->SetBeat(Beat, Now());
	}
}

void USaltPrologueSubsystem::Begin(bool bWakeUp)
{
	UWorld* World = GetWorld();
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
	ASaltPlayerController* PC = FirstController(World);
	ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!World || !Saves || !Player)
	{
		return;
	}
	Saves->ClearBeats();
	// Dawn of the first day.
	if (ASaltLightingRig* Rig = FindFirstActor<ASaltLightingRig>(World))
	{
		Rig->SetDay(1);
		Rig->SetTimeOfDay(SaltStory::StartHour);
	}
	// Nothing but the clothes on your back: no tools, an empty satchel.
	Player->SetOwnedTools({});
	if (USaltInventoryComponent* Satchel = Player->GetInventory())
	{
		TArray<FSaltItemStack> Empty;
		Empty.SetNum(Satchel->GetSlots().Num());
		Satchel->SetSlots(Empty);
	}
	// The Cartographer's boat: whole and dry, her sail ripped.
	if (ASaltBoat* Boat = FindFirstActor<ASaltBoat>(World))
	{
		if (USaltHullDamageComponent* Damage = Boat->GetHullDamage())
		{
			Damage->RestoreState({}, 0.f, SaltStory::StartTear);
		}
		Boat->SetSailSet(false);
	}
	// On the wreck beach, facing the wreck.
	const FVector Spot = GetWakeSpot();
	const FRotator Facing(0.f, SaltStory::WakeYaw, 0.f);
	Player->TeleportTo(Spot + FVector(0.0, 0.0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), Facing);
	PC->SetControlRotation(Facing);
	if (bWakeUp)
	{
		Player->StartWakeUp(6.f);
	}
	Reach(SaltStory::Prologue);
	Reach(SaltStory::Woke);
	ApplyState();
	UE_LOG(LogSaltbound, Display, TEXT("SB_STORY prologue at=(%.0f, %.0f, %.0f) wake=%d"), Spot.X, Spot.Y, Spot.Z, bWakeUp ? 1 : 0);
}

void USaltPrologueSubsystem::ApplyState()
{
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
	ASaltBoat* Boat = FindFirstActor<ASaltBoat>(GetWorld());
	if (!Saves || !Boat)
	{
		return;
	}
	// Until the ripped sail has been mended once, it is not set (later tears let her limp home, D74).
	Boat->SetSailLocked(Saves->HasBeat(SaltStory::Prologue) && !Saves->HasBeat(SaltStory::SailMended),
		NSLOCTEXT("SaltStory", "SailRipped", "The sail is ripped. Mend it at the mast's foot: it takes sailcloth."));
}

void USaltPrologueSubsystem::UpdateBeats()
{
	UWorld* World = GetWorld();
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
	if (!Saves || !Saves->HasBeat(SaltStory::Prologue))
	{
		return;
	}
	if (!Saves->HasBeat(SaltStory::Cache) && Saves->GetDigStage(SaltStory::CacheSpot) >= 1)
	{
		Reach(SaltStory::Cache);
	}
	ASaltBoat* Boat = FindFirstActor<ASaltBoat>(World);
	const USaltHullDamageComponent* Damage = Boat ? Boat->GetHullDamage() : nullptr;
	if (!Saves->HasBeat(SaltStory::SailMended) && Damage && Damage->GetSailTear() == 0)
	{
		Reach(SaltStory::SailMended);
		ApplyState();
		if (ASaltPlayerController* PC = FirstController(World))
		{
			PC->ShowNotice(NSLOCTEXT("SaltStory", "SailHolds", "The sail will hold. She is yours now."));
		}
	}
	if (Saves->HasBeat(SaltStory::SailMended) && !Saves->HasBeat(SaltStory::CastOff) && Boat && Boat->IsSailSet()
		&& Boat->GetHold() == ESaltBoatHold::Free && Boat->GetSpeedKnots() > 1.5f)
	{
		Reach(SaltStory::CastOff);
	}
}

void USaltPrologueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bStarted)
	{
		return;
	}
	CheckIn -= DeltaTime;
	if (CheckIn <= 0.f)
	{
		CheckIn = 0.5f;
		UpdateBeats();
	}
}

void USaltPrologueSubsystem::HandleChest(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
	if (!Player || !Saves)
	{
		return;
	}
	const bool bFirst = !Saves->HasBeat(SaltStory::Chest);
	// The Cartographer's things (a castaway owns nothing): their compass and spyglass, and the log (FH3).
	Player->SetToolOwned(TEXT("Compass"), true);
	Player->SetToolOwned(TEXT("Spyglass"), true);
	Reach(SaltStory::Chest);
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(Player->GetController()))
	{
		PC->ShowNotice(bFirst ? NSLOCTEXT("SaltStory", "ChestFirst", "The Cartographer's things: a compass, a spyglass, and a log bound in oilcloth.")
			: NSLOCTEXT("SaltStory", "ChestAgain", "Old charts, a spare shirt, the Cartographer's log."));
	}
}

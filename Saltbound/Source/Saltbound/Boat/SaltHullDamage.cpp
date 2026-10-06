// Saltbound - the ship repair system (D74, M6 step 3).

#include "Boat/SaltHullDamage.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBoatDamage(
		TEXT("sb.Boat.Damage"), 1,
		TEXT("D74: 1 hard knocks hole the boat (the ship repair system), 0 they do not (the boat suite turns it off while it rams the pier)."));

	FString BoatMeshPath(const TCHAR* Name)
	{
		// Under Props (always cooked): nothing references them but this code.
		return FString::Printf(TEXT("/Game/Saltbound/Props/Repair/%s.%s"), Name, Name);
	}

	/** A hole's sizes: what patching it takes. */
	constexpr int32 PatchPlanks = 1;
	constexpr int32 PatchNails = 2;
	const FName Plank = TEXT("Driftwood");
	const FName Nails = TEXT("IronNails");
	const FName Cloth = TEXT("Canvas");

	/** What is left of the sail's drive at each tear (whole, a tear, a rip, shreds). */
	constexpr float TearDrive[] = { 1.f, 0.78f, 0.55f, 0.2f };

	/** Where the bucket stands: starboard, just aft of the boarding gap, against the bulwark. */
	constexpr float BucketX = -1.05f;
}

// --- The repair volumes ----------------------------------------------------------------------------------------------

bool USaltRepairUseComponent::CanInteract(const ASaltPlayerCharacter* Player) const
{
	const USaltHullDamageComponent* Damage = GetOwner() ? GetOwner()->FindComponentByClass<USaltHullDamageComponent>() : nullptr;
	return Super::CanInteract(Player) && Damage && Damage->CanUse(this, Player);
}

FText USaltRepairUseComponent::GetPromptText(const ASaltPlayerCharacter* Player) const
{
	const USaltHullDamageComponent* Damage = GetOwner() ? GetOwner()->FindComponentByClass<USaltHullDamageComponent>() : nullptr;
	return Damage ? Damage->GetPrompt(this, Player) : Super::GetPromptText(Player);
}

void USaltRepairUseComponent::Interact(ASaltPlayerCharacter* Player)
{
	if (USaltHullDamageComponent* Damage = GetOwner() ? GetOwner()->FindComponentByClass<USaltHullDamageComponent>() : nullptr)
	{
		Damage->UseBy(this, Player);
	}
	Super::Interact(Player);
}

// --- The hull's damage -----------------------------------------------------------------------------------------------

USaltHullDamageComponent::USaltHullDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USaltHullDamageComponent::BeginPlay()
{
	Super::BeginPlay();
	Boat = Cast<ASaltBoat>(GetOwner());
	if (ASaltBoat* B = Boat.Get())
	{
		KnockHandle = B->OnHullKnock.AddUObject(this, &USaltHullDamageComponent::Knock);
	}
	EnsureParts();
	RebuildHoles();
	PushToBoat();
}

void USaltHullDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ASaltBoat* B = Boat.Get())
	{
		B->OnHullKnock.Remove(KnockHandle);
	}
	Super::EndPlay(EndPlayReason);
}

FTransform USaltHullDamageComponent::HoleTransform(float X, int32 Side)
{
	using namespace SaltHull;
	// On the bulwark's inner face, a hand's breadth above the deck, turned with the hull's line there.
	const float Inner = HalfBeamAtSheer(X) - 0.11f;
	const float Slope = (HalfBeamAtSheer(X + 0.05f) - HalfBeamAtSheer(X - 0.05f)) / 0.1f;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan(Slope)) * Side;
	const FVector At(X, Side * Inner, SheerZ(X) + 0.17f);
	// The meshes are built facing -Y (from the starboard side inward); the port side turns them round.
	return FTransform(FRotator(0.f, (Side > 0 ? 0.f : 180.f) + Yaw, 0.f), At * 100.0);
}

bool USaltHullDamageComponent::IsHoleSpotFree(float X, int32 Side, float HalfWidth, float* Protrusion) const
{
	using namespace SaltHull;
	if (Protrusion)
	{
		*Protrusion = 0.f;
	}
	const ASaltBoat* B = Boat.Get();
	if (!B || !B->Hull)
	{
		return true;
	}
	// The bucket stands against the starboard bulwark and has no collision of its own.
	if (Side > 0 && FMath::Abs(X - BucketX) < 0.16f + HalfWidth)
	{
		if (Protrusion)
		{
			*Protrusion = 30.f;
		}
		return false;
	}
	// Rays from 60 cm inboard out to the bulwark's face, across the hole's width (and its splinters) and up to the pin rails'
	// height: the pin rails and casks are part of her meshes, so trace their triangles. Whatever stops a ray more than 8 cm short
	// of the face stands in front of it.
	TInlineComponentArray<UPrimitiveComponent*> Parts(B);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltHoleSpot), true);
	const FTransform& W = B->Hull->GetComponentTransform();
	constexpr float ReachCm = 60.f;
	float Out = 0.f;
	const float Wide = HalfWidth * 1.2f;
	for (const float DX : { -Wide, -Wide * 0.5f, 0.f, Wide * 0.5f, Wide })
	{
		const float At = X + DX;
		const float Face = HalfBeamAtSheer(At) - 0.11f;
		for (const float DZ : { 0.09f, 0.17f, 0.25f, 0.33f, 0.40f })
		{
			const FVector From = W.TransformPosition(FVector(At, Side * (Face - ReachCm / 100.f), SheerZ(At) + DZ) * 100.0);
			const FVector To = W.TransformPosition(FVector(At, Side * (Face + 0.04f), SheerZ(At) + DZ) * 100.0);
			for (UPrimitiveComponent* C : Parts)
			{
				FHitResult Hit;
				if (C && C->IsQueryCollisionEnabled() && C->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block
					&& C->LineTraceComponent(Hit, From, To, Params))
				{
					Out = FMath::Max(Out, ReachCm - Hit.Distance);
				}
			}
		}
	}
	if (Protrusion)
	{
		*Protrusion = Out;
	}
	return Out < 8.f;
}

void USaltHullDamageComponent::EnsureParts()
{
	ASaltBoat* B = Boat.Get();
	if (!B || Bucket || !B->Hull)
	{
		return;
	}
	auto Mesh = [B](const TCHAR* Path, const FTransform& At)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(B);
		M->SetupAttachment(B->Hull);
		M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Path));
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->SetRelativeTransform(At);
		M->RegisterComponent();
		return M;
	};
	auto Volume = [B](ESaltRepairUse Use, const FVector& AtCm, const FVector& Extent, float Hold)
	{
		USaltRepairUseComponent* U = NewObject<USaltRepairUseComponent>(B);
		U->Use = Use;
		U->SetupAttachment(B->Hull);
		U->SetBoxExtent(Extent);
		U->SetRelativeLocation(AtCm);
		U->HoldTime = Hold;
		U->RegisterComponent();
		return U;
	};
	using namespace SaltHull;
	// The bucket stands on the deck against the starboard bulwark, just aft of the gap.
	const float BucketY = HalfBeamAtSheer(BucketX) - 0.11f - 0.17f;
	const FVector BucketAt(BucketX * 100.f, BucketY * 100.f, SheerZ(BucketX) * 100.f + 1.f);
	Bucket = Mesh(*BoatMeshPath(TEXT("SM_Boat_Bucket")), FTransform(FRotator(0.f, 35.f, 0.f), BucketAt));
	BucketUse = Volume(ESaltRepairUse::Bail, BucketAt + FVector(0.f, 0.f, 22.f), FVector(22.f, 22.f, 25.f), BailSeconds);
	// Mending happens at the mast's foot (the yard and the sail come down to the hands there).
	MendUse = Volume(ESaltRepairUse::Mend, FVector((MastX - 0.32f) * 100.f, 0.f, SheerZ(MastX) * 100.f + 95.f), FVector(20.f, 30.f, 40.f), MendSeconds);
	// The water over the deck, from the deck's outline inside the bulwarks.
	DeckWater = Mesh(*BoatMeshPath(TEXT("SM_Boat_BilgeWater")), FTransform(FVector(0.f, 0.f, SheerZ(0.f) * 100.f)));
	DeckWater->SetCastShadow(false);
	DeckWater->SetVisibility(false);
}

void USaltHullDamageComponent::RebuildHoles()
{
	ASaltBoat* B = Boat.Get();
	if (!B || !B->Hull)
	{
		return;
	}
	for (UStaticMeshComponent* M : HoleMeshes) { if (M) { M->DestroyComponent(); } }
	for (UStaticMeshComponent* M : SpurtMeshes) { if (M) { M->DestroyComponent(); } }
	for (USaltRepairUseComponent* U : HoleUses) { if (U) { U->DestroyComponent(); } }
	HoleMeshes.Reset();
	SpurtMeshes.Reset();
	HoleUses.Reset();
	UStaticMesh* HoleMesh = LoadObject<UStaticMesh>(nullptr, *BoatMeshPath(TEXT("SM_Hull_Hole")));
	UStaticMesh* PatchMesh = LoadObject<UStaticMesh>(nullptr, *BoatMeshPath(TEXT("SM_Hull_Patch")));
	UStaticMesh* SpurtMesh = LoadObject<UStaticMesh>(nullptr, *BoatMeshPath(TEXT("SM_Water_Spurt")));
	for (int32 I = 0; I < Holes.Num(); ++I)
	{
		const FSaltHullHole& H = Holes[I];
		const FTransform At = HoleTransform(H.X, H.Side);
		// Bigger holes are bigger breaks; a patch is the same board whatever it covers.
		const float Scale = H.bPatched ? 1.f : 0.75f + 0.25f * H.Size;
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(B);
		M->SetupAttachment(B->Hull);
		M->SetStaticMesh(H.bPatched ? PatchMesh : HoleMesh);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetRelativeTransform(FTransform(At.GetRotation(), At.GetLocation(), FVector(Scale)));
		M->RegisterComponent();
		HoleMeshes.Add(M);
		UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(B);
		S->SetupAttachment(B->Hull);
		S->SetStaticMesh(SpurtMesh);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(false);
		S->SetRelativeTransform(FTransform(At.GetRotation(), At.GetLocation(), FVector(0.6f + 0.25f * H.Size)));
		S->SetVisibility(!H.bPatched);
		S->RegisterComponent();
		SpurtMeshes.Add(S);
		USaltRepairUseComponent* U = NewObject<USaltRepairUseComponent>(B);
		U->Use = ESaltRepairUse::Hole;
		U->HoleIndex = I;
		U->SetupAttachment(B->Hull);
		U->SetBoxExtent(FVector(22.f, 14.f, 16.f));
		U->SetRelativeTransform(FTransform(At.GetRotation(), At.TransformPosition(FVector(0.f, -10.f, 0.f))));
		U->HoldTime = PatchSeconds;
		U->RegisterComponent();
		HoleUses.Add(U);
	}
}

int32 USaltHullDamageComponent::CountOpenHoles() const
{
	int32 N = 0;
	for (const FSaltHullHole& H : Holes)
	{
		N += H.bPatched ? 0 : 1;
	}
	return N;
}

float USaltHullDamageComponent::GetLeakRate() const
{
	float Rate = 0.f;
	for (const FSaltHullHole& H : Holes)
	{
		if (!H.bPatched)
		{
			Rate += (float)LeakLitresPerSecond[FMath::Clamp(H.Size, 1, 3) - 1];
		}
	}
	return Rate;
}

float USaltHullDamageComponent::GetDriveFactor() const
{
	// The tear takes its share of the drive; a boat low in the water drags (a third less at the brink).
	return TearDrive[FMath::Clamp(SailTear, 0, 3)] * (1.f - 0.35f * GetFill());
}

void USaltHullDamageComponent::Knock(const FVector& BodyPointM, float ClosingSpeed)
{
	if (ClosingSpeed < HoleSpeed || KnockCooldown > 0.f || CVarBoatDamage.GetValueOnGameThread() == 0)
	{
		return;
	}
	// A knock lasts a few frames; one hole for it.
	KnockCooldown = 1.2f;
	const int32 Size = ClosingSpeed >= LargeSpeed ? 3 : ClosingSpeed >= MediumSpeed ? 2 : 1;
	const int32 Side = FMath::Abs(BodyPointM.Y) > 0.25f ? (BodyPointM.Y > 0.f ? 1 : -1) : (FMath::RandBool() ? 1 : -1);
	if (AddHole(BodyPointM.X, Side, Size))
	{
		UE_LOG(LogSaltbound, Display, TEXT("SB_REPAIR knock speed=%.2f m/s at (%.2f, %.2f, %.2f) hole size=%d holes=%d"), ClosingSpeed,
			BodyPointM.X, BodyPointM.Y, BodyPointM.Z, Size, Holes.Num());
		Say(Size >= 3 ? NSLOCTEXT("SaltRepair", "HoledBadly", "She is holed badly: the sea is pouring in")
			: NSLOCTEXT("SaltRepair", "Holed", "A plank has sprung: she is taking water"));
	}
}

bool USaltHullDamageComponent::AddHole(float X, int32 Side, int32 Size)
{
	if (Holes.Num() >= MaxHoles)
	{
		return false;
	}
	Side = Side >= 0 ? 1 : -1;
	Size = FMath::Clamp(Size, 1, 3);
	// The nearest spot along the bulwark to where she was hit: never in the boarding gaps (no bulwark there) nor right at the
	// ends, not on top of another hole (0.55 m apart), and not behind a fitting (seen in play: a hole on the pin rail cut through
	// its coils). Only when every spot on that side is behind something does a fitting not count.
	const float Want = FMath::Clamp(X, -4.0f, 3.9f);
	const float Gate = SaltHull::GateHalfWidth + 0.25f;
	const float HalfWidth = 0.335f * (0.75f + 0.25f * Size);
	auto Fits = [this, Side, Gate, HalfWidth](float C, bool bFittings)
	{
		if (C < -4.0f || C > 3.9f || FMath::Abs(C) < Gate
			|| Holes.ContainsByPredicate([C, Side](const FSaltHullHole& H) { return H.Side == Side && FMath::Abs(H.X - C) < 0.55f; }))
		{
			return false;
		}
		return !bFittings || IsHoleSpotFree(C, Side, HalfWidth);
	};
	auto Nearest = [Want](bool bFittings, const TFunctionRef<bool(float, bool)>& Ok, float& Out)
	{
		for (int32 K = 0; K <= 80; ++K)
		{
			const float C = Want + (K % 2 == 1 ? 0.1f : -0.1f) * ((K + 1) / 2);
			if (Ok(C, bFittings))
			{
				Out = C;
				return true;
			}
		}
		return false;
	};
	if (!Nearest(true, Fits, X))
	{
		// Her bulwarks are crowded (pin rails, the chest, benches): with no clear spot left on that side, the knock opens the
		// nearest open hole there wider; only when none can open further does a hole go behind a fitting.
		int32 Widen = INDEX_NONE;
		for (int32 I = 0; I < Holes.Num(); ++I)
		{
			const FSaltHullHole& O = Holes[I];
			if (O.Side == Side && !O.bPatched && O.Size < 3 && (Widen == INDEX_NONE || FMath::Abs(O.X - Want) < FMath::Abs(Holes[Widen].X - Want)))
			{
				Widen = I;
			}
		}
		if (Widen != INDEX_NONE)
		{
			Holes[Widen].Size = FMath::Min(Holes[Widen].Size + Size, 3);
			bSaidTakingWater = true;
			RebuildHoles();
			PushToBoat();
			return true;
		}
		if (!Nearest(false, Fits, X))
		{
			return false;
		}
	}
	FSaltHullHole H;
	H.X = X;
	H.Side = Side;
	H.Size = Size;
	Holes.Add(H);
	bSaidTakingWater = true;
	RebuildHoles();
	PushToBoat();
	return true;
}

bool USaltHullDamageComponent::PatchHole(int32 Index, ASaltPlayerCharacter* Player, bool bFree)
{
	if (!Holes.IsValidIndex(Index) || Holes[Index].bPatched)
	{
		return false;
	}
	if (!bFree)
	{
		USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		if (!Satchel || Satchel->CountOf(Plank) < PatchPlanks || Satchel->CountOf(Nails) < PatchNails)
		{
			Say(NSLOCTEXT("SaltRepair", "NeedPlank", "A patch takes a piece of driftwood and two nails"));
			return false;
		}
		Satchel->Remove(Plank, PatchPlanks);
		Satchel->Remove(Nails, PatchNails);
	}
	Holes[Index].bPatched = true;
	UE_LOG(LogSaltbound, Display, TEXT("SB_REPAIR patched hole=%d open=%d"), Index, CountOpenHoles());
	if (CountOpenHoles() == 0)
	{
		Say(NSLOCTEXT("SaltRepair", "Tight", "The last hole is patched: she is tight again"));
		bSaidTakingWater = false;
	}
	RebuildHoles();
	PushToBoat();
	return true;
}

void USaltHullDamageComponent::Bail(float Litres)
{
	WaterKg = FMath::Max(0.f, WaterKg - Litres);
	PushToBoat();
}

bool USaltHullDamageComponent::MendSail(ASaltPlayerCharacter* Player, bool bFree)
{
	if (SailTear <= 0 || SailTear >= 3)
	{
		return false;
	}
	if (!bFree)
	{
		USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		if (!Satchel || Satchel->CountOf(Cloth) < 1)
		{
			Say(NSLOCTEXT("SaltRepair", "NeedCloth", "Mending the sail takes sailcloth"));
			return false;
		}
		Satchel->Remove(Cloth, 1);
	}
	--SailTear;
	UE_LOG(LogSaltbound, Display, TEXT("SB_REPAIR mended sail tear=%d"), SailTear);
	PushToBoat();
	return true;
}

void USaltHullDamageComponent::TearSail()
{
	if (SailTear >= 3)
	{
		return;
	}
	++SailTear;
	UE_LOG(LogSaltbound, Display, TEXT("SB_REPAIR sail tore tear=%d"), SailTear);
	Say(SailTear >= 3 ? NSLOCTEXT("SaltRepair", "Shreds", "The sail is in shreds")
		: SailTear == 2 ? NSLOCTEXT("SaltRepair", "Rip", "The sail rips further: take it in")
		: NSLOCTEXT("SaltRepair", "Tear", "The sail tears: too much wind on it"));
	PushToBoat();
}

void USaltHullDamageComponent::RepairAll()
{
	Holes.Reset();
	WaterKg = 0.f;
	SailTear = 0;
	TearTimer = 0.f;
	bSaidTakingWater = false;
	bSaidLow = false;
	RebuildHoles();
	PushToBoat();
}

void USaltHullDamageComponent::RestoreState(const TArray<FSaltHullHole>& InHoles, float InWaterKg, int32 InSailTear)
{
	Holes = InHoles;
	WaterKg = FMath::Max(InWaterKg, 0.f);
	SailTear = FMath::Clamp(InSailTear, 0, 3);
	TearTimer = 0.f;
	bSaidTakingWater = CountOpenHoles() > 0;
	EnsureParts();
	RebuildHoles();
	PushToBoat();
}

void USaltHullDamageComponent::PushToBoat()
{
	if (ASaltBoat* B = Boat.Get())
	{
		B->BilgeWaterKg = WaterKg;
		B->SailCondition = GetDriveFactor();
	}
	if (DeckWater)
	{
		// Over the deck past DeckWaterFrom: from just under the boards up to 18 cm deep at the brink.
		const float Over = FMath::Clamp((GetFill() - DeckWaterFrom) / FMath::Max(1.f - DeckWaterFrom, 0.01f), 0.f, 1.f);
		DeckWater->SetVisibility(Over > 0.f);
		DeckWater->SetRelativeLocation(FVector(0.f, 0.f, SaltHull::SheerZ(0.f) * 100.f - 2.f + 20.f * Over));
	}
}

void USaltHullDamageComponent::Say(const FText& Text) const
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowNotice(Text);
	}
}

void USaltHullDamageComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ASaltBoat* B = Boat.Get();
	if (!B)
	{
		return;
	}
	KnockCooldown = FMath::Max(0.f, KnockCooldown - DeltaTime);
	// The sea comes in through the open holes.
	const float Rate = GetLeakRate();
	if (Rate > 0.f)
	{
		WaterKg = FMath::Min(WaterKg + Rate * DeltaTime, FounderKg);
		if (!bSaidLow && GetFill() > DeckWaterFrom)
		{
			bSaidLow = true;
			Say(NSLOCTEXT("SaltRepair", "Low", "She is low in the water: bail, and patch the holes"));
		}
	}
	if (GetFill() < DeckWaterFrom * 0.8f)
	{
		bSaidLow = false;
	}
	// Too much wind on a full sail tears it.
	const float Wind = (float)B->GetTrueWind().Size();
	if (Wind > TearWind && B->GetSailAmount() > SailTearShare && SailTear < 3)
	{
		TearTimer += DeltaTime * (Wind / TearWind);
		if (TearTimer > TearSeconds)
		{
			TearTimer = 0.f;
			TearSail();
		}
	}
	else
	{
		TearTimer = FMath::Max(0.f, TearTimer - DeltaTime);
	}
	PushToBoat();
	// Holding E only patches or mends with what it takes in the satchel; otherwise a press says what is missing.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
	const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	const bool bPatchKit = Satchel && Satchel->CountOf(Plank) >= PatchPlanks && Satchel->CountOf(Nails) >= PatchNails;
	for (USaltRepairUseComponent* U : HoleUses)
	{
		if (U)
		{
			U->HoldTime = bPatchKit ? PatchSeconds : 0.f;
		}
	}
	if (MendUse)
	{
		MendUse->HoldTime = Satchel && Satchel->CountOf(Cloth) > 0 && SailTear < 3 ? MendSeconds : 0.f;
	}
	// The spouts breathe with the sea (they spurt harder as she heels).
	const float Heel = FMath::Abs(B->GetHeelDeg());
	for (int32 I = 0; I < SpurtMeshes.Num() && I < Holes.Num(); ++I)
	{
		if (UStaticMeshComponent* S = SpurtMeshes[I]; S && !Holes[I].bPatched)
		{
			const float Pulse = 0.85f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f + I * 1.7f) + 0.01f * Heel;
			S->SetRelativeScale3D(FVector((0.6f + 0.25f * Holes[I].Size) * Pulse));
		}
	}
	// Foundered: the brink is reached. ASaltBoat's own recovery takes her home (D74: towed back to her mooring).
	if (WaterKg >= FounderKg - 1.f)
	{
		UE_LOG(LogSaltbound, Display, TEXT("SB_REPAIR foundered water=%.0f"), WaterKg);
		Say(NSLOCTEXT("SaltRepair", "Foundered", "She foundered. The Harbour Keeper's men towed her home; she needs the Shipwright."));
		WaterKg = 0.f;
		for (FSaltHullHole& H : Holes)
		{
			H.bPatched = true;
		}
		RebuildHoles();
		B->ResetToStart();
		PushToBoat();
	}
}

// --- What the volumes offer ------------------------------------------------------------------------------------------

bool USaltHullDamageComponent::CanUse(const USaltRepairUseComponent* Use, const ASaltPlayerCharacter* Player) const
{
	switch (Use->Use)
	{
	case ESaltRepairUse::Hole: return Holes.IsValidIndex(Use->HoleIndex) && !Holes[Use->HoleIndex].bPatched;
	case ESaltRepairUse::Bail: return WaterKg > 1.f;
	case ESaltRepairUse::Mend: return SailTear > 0;
	}
	return false;
}

FText USaltHullDamageComponent::GetPrompt(const USaltRepairUseComponent* Use, const ASaltPlayerCharacter* Player) const
{
	const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	switch (Use->Use)
	{
	case ESaltRepairUse::Hole:
	{
		const bool bHave = Satchel && Satchel->CountOf(Plank) >= PatchPlanks && Satchel->CountOf(Nails) >= PatchNails;
		return bHave ? NSLOCTEXT("SaltRepair", "Patch", "Patch the hole") : NSLOCTEXT("SaltRepair", "PatchNeed", "Patch the hole (driftwood, two nails)");
	}
	case ESaltRepairUse::Bail:
		return FText::Format(NSLOCTEXT("SaltRepair", "Bail", "Bail ({0} buckets aboard)"), FText::AsNumber(FMath::CeilToInt(WaterKg / BucketLitres)));
	case ESaltRepairUse::Mend:
		if (SailTear >= 3)
		{
			return NSLOCTEXT("SaltRepair", "MendShreds", "The sail is in shreds: the Shipwright");
		}
		return Satchel && Satchel->CountOf(Cloth) > 0 ? NSLOCTEXT("SaltRepair", "Mend", "Mend the sail")
			: NSLOCTEXT("SaltRepair", "MendNeed", "Mend the sail (sailcloth)");
	}
	return FText::GetEmpty();
}

void USaltHullDamageComponent::UseBy(USaltRepairUseComponent* Use, ASaltPlayerCharacter* Player)
{
	switch (Use->Use)
	{
	case ESaltRepairUse::Hole:
		PatchHole(Use->HoleIndex, Player);
		break;
	case ESaltRepairUse::Bail:
		Bail(BucketLitres);
		break;
	case ESaltRepairUse::Mend:
		if (SailTear >= 3)
		{
			Say(NSLOCTEXT("SaltRepair", "ShredsNeedSlip", "Shreds cannot be mended at sea: the Shipwright must see to it"));
		}
		else
		{
			MendSail(Player);
		}
		break;
	}
}

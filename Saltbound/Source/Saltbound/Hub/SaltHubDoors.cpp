// Saltbound - the hub ship's hidden side doors (D64; see SaltHubDoors.h).

#include "Hub/SaltHubDoors.h"

#include "Boat/SaltBoat.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

namespace SaltHubDoorsLocal
{
	/** SM_Hub_GateRope hangs 27 m down from its pivot (build_hub_doors.py ROPE_LEN). */
	constexpr float RopeMeshCm = 2700.f;
	/** How often the watch looks out, and the swing is checked while the leaves move (s). */
	constexpr float WatchStep = 0.25f;
	/** Clearance kept between a leaf and a boat (cm). */
	constexpr float SwingMarginCm = 40.f;
	/** A boat inside the hall heading for a pair at this speed or more wants out (cm/s). */
	constexpr float LeavingSpeed = 50.f;
	/** The doorway: within this of the doors' middle fore and aft, and this far out from the centreline (cm). */
	constexpr float DoorwayHalfX = 700.f;
	constexpr float DoorwayInY = 1100.f;

	ASaltHubDoors* FindDoors(UWorld* World)
	{
		for (TActorIterator<ASaltHubDoors> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdDoors(
		TEXT("sb.Hub.Doors"),
		TEXT("Work the hub's side doors: open | close | toggle | snapopen | snapclosed | watchon | watchoff; without an argument it prints their state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltHubDoors* Doors = FindDoors(World);
			if (!Doors)
			{
				UE_LOG(LogSaltbound, Warning, TEXT("SB_HUBDOORS none in this world"));
				return;
			}
			const FString Arg = Args.Num() > 0 ? Args[0].ToLower() : FString();
			if (Arg == TEXT("open")) { Doors->Open(); }
			else if (Arg == TEXT("close")) { Doors->Close(); }
			else if (Arg == TEXT("toggle")) { Doors->Toggle(); }
			else if (Arg == TEXT("snapopen")) { Doors->SetOpenAmountNow(1.f); }
			else if (Arg == TEXT("snapclosed")) { Doors->SetOpenAmountNow(0.f); }
			else if (Arg == TEXT("watchon")) { Doors->bWatch = true; }
			else if (Arg == TEXT("watchoff")) { Doors->bWatch = false; }
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUBDOORS amount=%.3f moving=%d blocked=%d watch=%d yaws=%.1f,%.1f,%.1f,%.1f last='%s'"),
				Doors->GetOpenAmount(), Doors->IsMoving() ? 1 : 0, Doors->IsSwingBlocked() ? 1 : 0, Doors->bWatch ? 1 : 0, Doors->GetLeafYaw(0),
				Doors->GetLeafYaw(1), Doors->GetLeafYaw(2), Doors->GetLeafYaw(3), *Doors->GetLastReason());
		}));
}

ASaltHubDoors::ASaltHubDoors()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	for (int32 I = 0; I < MaxLeaves; ++I)
	{
		UStaticMeshComponent* Leaf = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Leaf%d"), I));
		Leaf->SetupAttachment(Root);
		Leaf->SetMobility(EComponentMobility::Movable);
		LeafMeshes.Add(Leaf);
		UStaticMeshComponent* Rope = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Rope%d"), I));
		Rope->SetupAttachment(Root);
		Rope->SetMobility(EComponentMobility::Movable);
		Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ropes.Add(Rope);
	}
	for (int32 I = 0; I < MaxCapstans; ++I)
	{
		UStaticMeshComponent* Capstan = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Capstan%d"), I));
		Capstan->SetupAttachment(Root);
		Capstan->SetMobility(EComponentMobility::Movable);
		Capstans.Add(Capstan);
		USaltInteractableComponent* Use = CreateDefaultSubobject<USaltInteractableComponent>(*FString::Printf(TEXT("CapstanUse%d"), I));
		Use->SetupAttachment(Root);
		Use->SetBoxExtent(FVector(140.f, 140.f, 70.f));         // the bars sweep 1.3 m round the drum
		Use->PromptOffset = FVector(0.0, 0.0, 70.0);
		Handles.Add(Use);
	}
}

void ASaltHubDoors::Resolve()
{
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	for (int32 I = 0; I < LeafMeshes.Num(); ++I)
	{
		const bool bUsed = Leaves.IsValidIndex(I) && !Leaves[I].Mesh.IsNone();
		if (bUsed && !LeafMeshes[I]->GetStaticMesh())
		{
			LeafMeshes[I]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(*Leaves[I].Mesh.ToString())));
		}
		LeafMeshes[I]->SetVisibility(bUsed);
		LeafMeshes[I]->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bUsed && !Ropes[I]->GetStaticMesh())
		{
			Ropes[I]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(TEXT("GateRope"))));
		}
		Ropes[I]->SetVisibility(bUsed);
	}
	for (int32 I = 0; I < Capstans.Num(); ++I)
	{
		if (!Capstans[I]->GetStaticMesh())
		{
			Capstans[I]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(TEXT("Capstan"))));
		}
		const bool bUsed = CapstanSpots.IsValidIndex(I);
		Capstans[I]->SetVisibility(bUsed);
		Capstans[I]->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Handles[I]->bInteractionEnabled = bUsed;
		if (bUsed)
		{
			Capstans[I]->SetRelativeLocation(CapstanSpots[I]);
			Handles[I]->SetRelativeLocation(CapstanSpots[I] + FVector(0.0, 0.0, 100.0));
		}
	}
}

void ASaltHubDoors::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Resolve();
	SetOpenAmountNow(0.f);
}

void ASaltHubDoors::BeginPlay()
{
	Super::BeginPlay();
	Resolve();
	SetOpenAmountNow(0.f);
	for (USaltInteractableComponent* Use : Handles)
	{
		Use->OnInteract.AddDynamic(this, &ASaltHubDoors::HandleInteract);
	}
}

float ASaltHubDoors::LeafFraction(int32 Leaf) const
{
	// The aft leaf (the master) leads by LagDegrees: it opens first and shuts last.
	const float Progress = Amount * (90.f + LagDegrees);
	const bool bFollows = Leaves.IsValidIndex(Leaf) && Leaves[Leaf].bFollows;
	return FMath::Clamp((bFollows ? Progress - LagDegrees : Progress) / 90.f, 0.f, 1.f);
}

float ASaltHubDoors::GetLeafYaw(int32 Leaf) const
{
	return Leaves.IsValidIndex(Leaf) ? Leaves[Leaf].OpenYaw * LeafFraction(Leaf) : 0.f;
}

bool ASaltHubDoors::IsSwingBlocked() const
{
	// Points along each leaf's footprint at angles across the rest of its swing (from where it is to where it goes), tested in
	// every boat's own box.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	TArray<TPair<FTransform, FBox>> Boats;
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		Boats.Emplace(It->GetActorTransform(), It->CalculateComponentsBoundingBoxInLocalSpace().ExpandBy(SaltHubDoorsLocal::SwingMarginCm));
	}
	if (Boats.Num() == 0)
	{
		return false;
	}
	const FTransform Frame = GetActorTransform();
	for (int32 I = 0; I < Leaves.Num() && I < LeafMeshes.Num(); ++I)
	{
		const UStaticMesh* Mesh = LeafMeshes[I]->GetStaticMesh();
		if (!Mesh)
		{
			continue;
		}
		const FBox B = Mesh->GetBoundingBox();
		const float Now = GetLeafYaw(I);
		const float Goal = Leaves[I].OpenYaw * (Target >= 0.5f ? 1.f : 0.f);
		for (int32 K = 0; K <= 6; ++K)
		{
			const FRotator Yaw(0.0, FMath::Lerp(Now, Goal, K / 6.f), 0.0);
			for (int32 U = 0; U <= 5; ++U)
			{
				for (int32 V = 0; V <= 1; ++V)
				{
					const FVector Leaf(FMath::Lerp(B.Min.X, B.Max.X, U / 5.0), V ? B.Max.Y : B.Min.Y, 0.0);
					const FVector Local = Leaves[I].Pivot * FVector(1.0, 1.0, 0.0) + Yaw.RotateVector(Leaf);
					const FVector World3 = Frame.TransformPosition(Local);
					for (const TPair<FTransform, FBox>& Boat : Boats)
					{
						const FVector P = Boat.Key.InverseTransformPosition(World3);
						if (P.X > Boat.Value.Min.X && P.X < Boat.Value.Max.X && P.Y > Boat.Value.Min.Y && P.Y < Boat.Value.Max.Y)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

void ASaltHubDoors::SetOpenAmountNow(float InAmount)
{
	Amount = Target = From = FMath::Clamp(InAmount, 0.f, 1.f);
	Travel = 1.f;
	bByWatch = false;
	QuietSeconds = 0.f;
	Apply();
}

bool ASaltHubDoors::MoveTo(float NewTarget, const TCHAR* Why)
{
	if (FMath::IsNearlyEqual(Target, NewTarget))
	{
		return true;
	}
	const float OldTarget = Target;
	Target = NewTarget;
	if (IsSwingBlocked())
	{
		Target = OldTarget;
		const FString Reason = FString::Printf(TEXT("%s: a boat is in the way"), Why);
		if (Reason != LastReason)
		{
			UE_LOG(LogSaltbound, Log, TEXT("SB_HUBDOORS %s blocked (a boat in the swing)"), NewTarget >= 0.5f ? TEXT("open") : TEXT("close"));
		}
		LastReason = Reason;
		return false;
	}
	From = Amount;
	Travel = 0.f;
	bHeld = false;
	LastReason = Why;
	UE_LOG(LogSaltbound, Log, TEXT("SB_HUBDOORS %s (%s)"), NewTarget >= 0.5f ? TEXT("opening") : TEXT("closing"), Why);
	return true;
}

bool ASaltHubDoors::Open()
{
	bByWatch = false;
	return MoveTo(1.f, TEXT("hand"));
}

bool ASaltHubDoors::Close()
{
	bByWatch = false;
	return MoveTo(0.f, TEXT("hand"));
}

bool ASaltHubDoors::Toggle()
{
	return Target >= 0.5f ? Close() : Open();
}

void ASaltHubDoors::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WatchClock += DeltaSeconds;
	bool bChecked = false;
	if (WatchClock >= SaltHubDoorsLocal::WatchStep)
	{
		Watch(WatchClock);
		WatchClock = 0.f;
		bChecked = true;
	}
	if (Travel >= 1.f)
	{
		return;
	}
	// Never into a boat: a leaf holds where it is while one is in the rest of its swing.
	if (bChecked)
	{
		bHeld = IsSwingBlocked();
	}
	if (bHeld)
	{
		return;
	}
	// Eased start and stop; a reversed move takes the time of the way left.
	const float Duration = FMath::Max(TravelSeconds * FMath::Abs(Target - From), 0.1f);
	Travel = FMath::Min(Travel + DeltaSeconds / Duration, 1.f);
	const float Ease = 0.5f - 0.5f * FMath::Cos(PI * Travel);
	Amount = Travel >= 1.f ? Target : FMath::Lerp(From, Target, Ease);
	Apply();
}

void ASaltHubDoors::Watch(float Seconds)
{
	if (!bWatch || !GetWorld())
	{
		return;
	}
	// A boat comes (outside the hull, near a pair) or wants out (inside, heading for one); the doorway has a boat in it.
	const FTransform Frame = GetActorTransform();
	bool bWanted = false;
	bool bInDoorway = false;
	for (TActorIterator<ASaltBoat> It(GetWorld()); It; ++It)
	{
		const FVector L = Frame.InverseTransformPosition(It->GetActorLocation());
		const FVector V = Frame.InverseTransformVector(It->GetVelocity());
		const float Side = L.Y >= 0.0 ? 1.f : -1.f;
		const FVector Door(0.0, Side * HullHalfCm, L.Z);
		const float Dist = (FVector(L.X, L.Y, L.Z) - Door).Size2D();
		const bool bOutside = FMath::Abs(L.Y) > HullHalfCm;
		const bool bHeading = FVector::DotProduct((Door - L).GetSafeNormal2D(), FVector(V.X, V.Y, 0.0)) > SaltHubDoorsLocal::LeavingSpeed;
		if (Dist < WatchOpenCm && (bOutside || bHeading))
		{
			bWanted = true;
		}
		if (FMath::Abs(L.X) < SaltHubDoorsLocal::DoorwayHalfX && FMath::Abs(L.Y) > SaltHubDoorsLocal::DoorwayInY && Dist < WatchOpenCm)
		{
			bInDoorway = true;
		}
	}
	if (bWanted)
	{
		QuietSeconds = 0.f;
		if (Target < 0.5f)
		{
			const bool bWasShut = Amount <= KINDA_SMALL_NUMBER || bByWatch;
			if (MoveTo(1.f, TEXT("watch")))
			{
				bByWatch = bWasShut;
			}
		}
		return;
	}
	if (bByWatch && IsOpen() && !bInDoorway)
	{
		QuietSeconds += Seconds;
		if (QuietSeconds >= WatchCloseSeconds && MoveTo(0.f, TEXT("watch")))
		{
			QuietSeconds = 0.f;
		}
	}
	else
	{
		QuietSeconds = 0.f;
	}
}

void ASaltHubDoors::Apply()
{
	for (int32 I = 0; I < LeafMeshes.Num(); ++I)
	{
		if (!Leaves.IsValidIndex(I))
		{
			continue;
		}
		const FSaltHubDoorLeaf& Leaf = Leaves[I];
		const FRotator Yaw(0.0, GetLeafYaw(I), 0.0);
		LeafMeshes[I]->SetRelativeLocationAndRotation(Leaf.Pivot, Yaw);
		// The rope from the leaf's eye up to its block (from there down to the capstan is the frame's, fixed).
		const FVector Eye = Leaf.Pivot + Yaw.RotateVector(Leaf.Eye - Leaf.Pivot);
		const FVector Up = Leaf.Block - Eye;
		const float Length = Up.Size();
		Ropes[I]->SetRelativeLocationAndRotation(Leaf.Block, FRotationMatrix::MakeFromZ(Up.GetSafeNormal()).Rotator());
		Ropes[I]->SetRelativeScale3D(FVector(1.0, 1.0, FMath::Max(Length, 1.f) / SaltHubDoorsLocal::RopeMeshCm));
	}
	// The capstans turn with the leaves (the two sides' opposite ways, each winding its own ropes).
	for (int32 I = 0; I < Capstans.Num(); ++I)
	{
		if (CapstanSpots.IsValidIndex(I))
		{
			Capstans[I]->SetRelativeRotation(FRotator(0.0, Amount * CapstanTurns * 360.0 * (I % 2 == 0 ? 1.0 : -1.0), 0.0));
		}
	}
	const bool bMoving = IsMoving();
	for (int32 I = 0; I < Handles.Num(); ++I)
	{
		Handles[I]->PromptText = Target >= 0.5f ? NSLOCTEXT("Saltbound", "HubDoorsShut", "Shut the sea doors")
			: NSLOCTEXT("Saltbound", "HubDoorsOpen", "Open the sea doors");
		Handles[I]->bInteractionEnabled = CapstanSpots.IsValidIndex(I) && !bMoving;
	}
}

void ASaltHubDoors::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	if (IsMoving())
	{
		return;
	}
	if (!Toggle())
	{
		if (ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr)
		{
			PC->ShowNotice(NSLOCTEXT("Saltbound", "HubDoorsBlocked", "A boat is in the doors' way."));
		}
	}
	Apply();
}

// Saltbound - the Well's hoist (D62; see SaltHubHoist.h).

#include "Hub/SaltHubHoist.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Interaction/SaltStationComponent.h"
#include "Saltbound.h"

namespace SaltHoistLocal
{
	/** SM_Hub_GateRope hangs 27 m down from its pivot (build_hub_seagate.py ROPE_LEN). */
	constexpr float RopeMeshCm = 2700.f;

	void HangRope(UStaticMeshComponent* Rope, const FVector& From, float ToZ)
	{
		const float Length = From.Z - ToZ;
		Rope->SetRelativeLocation(From);
		Rope->SetRelativeScale3D(FVector(1.0, 1.0, FMath::Max(Length, 1.f) / RopeMeshCm));
		Rope->SetVisibility(Length > 5.f);
	}
}

ASaltHubHoist::ASaltHubHoist()
{
	PrimaryActorTick.bCanEverTick = true;
	StopHeights = { 0.f, 400.f, 1600.f };
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto Mesh = [this](const TCHAR* Name, bool bCollide)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Movable);
		if (!bCollide)
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return C;
	};
	Cage = Mesh(TEXT("Cage"), true);
	Weight = Mesh(TEXT("Weight"), false);
	CageRope = Mesh(TEXT("CageRope"), false);
	WeightRope = Mesh(TEXT("WeightRope"), false);
	Station = CreateDefaultSubobject<USaltStationComponent>(TEXT("Station"));
	Station->SetupAttachment(Cage);
	Station->SetRelativeLocation(FVector(0.0, 0.0, 110.0));
	Station->SetBoxExtent(FVector(115.f, 115.f, 100.f));
	Station->StandOffset = FVector(20.f, 0.f, -98.f);
	Station->ExitOffset = FVector(-215.f, 0.f, -98.f);
	Station->YawLimit = 180.f;
	Station->PromptText = NSLOCTEXT("Saltbound", "HoistRide", "Ride the hoist");
	Station->LeavePromptText = NSLOCTEXT("Saltbound", "HoistStepOut", "Step out");
	for (int32 I = 0; I < MaxStops; ++I)
	{
		USaltInteractableComponent* Call = CreateDefaultSubobject<USaltInteractableComponent>(*FString::Printf(TEXT("Call%d"), I));
		Call->SetupAttachment(Root);
		Call->SetBoxExtent(FVector(30.f, 110.f, 80.f));
		Call->PromptText = NSLOCTEXT("Saltbound", "HoistCall", "Call the hoist");
		Calls.Add(Call);
		UBoxComponent* Guard = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Guard%d"), I));
		Guard->SetupAttachment(Root);
		Guard->SetCollisionProfileName(TEXT("BlockAll"));
		Guards.Add(Guard);
		GuardRopes.Add(Mesh(*FString::Printf(TEXT("GuardRope%d"), I), false));
	}
}

void ASaltHubHoist::Resolve()
{
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	auto Set = [](UStaticMeshComponent* C, const TCHAR* Part)
	{
		if (C && !C->GetStaticMesh())
		{
			C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(Part)));
		}
	};
	Set(Cage, TEXT("HoistCage"));
	Set(Weight, TEXT("HoistWeight"));
	Set(CageRope, TEXT("GateRope"));
	Set(WeightRope, TEXT("GateRope"));
	for (UStaticMeshComponent* R : GuardRopes)
	{
		Set(R, TEXT("GateRope"));
	}
	for (int32 I = 0; I < MaxStops; ++I)
	{
		const float Z = StopZ(I);
		Calls[I]->SetRelativeLocation(FVector(GapCentre.X - 35.0, 0.0, Z + 100.0));
		Guards[I]->SetRelativeLocation(GapCentre + FVector(0.0, 0.0, Z));
		Guards[I]->SetBoxExtent(GapExtent);
		// A rope across the gap: the rope mesh hangs down -Z from its pivot; turned to run along +Y.
		GuardRopes[I]->SetRelativeLocation(FVector(GapCentre.X, -GapExtent.Y, Z + 95.0));
		GuardRopes[I]->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector(0.0, -1.0, 0.0)).Rotator());
		GuardRopes[I]->SetRelativeScale3D(FVector(1.0, 1.0, GapExtent.Y * 2.0 / SaltHoistLocal::RopeMeshCm));
	}
}

void ASaltHubHoist::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Resolve();
	Apply();
}

void ASaltHubHoist::BeginPlay()
{
	Super::BeginPlay();
	Resolve();
	for (USaltInteractableComponent* Call : Calls)
	{
		Call->OnInteract.AddDynamic(this, &ASaltHubHoist::HandleCall);
	}
	SetStopNow(0);
}

FText ASaltHubHoist::StopName(int32 Stop) const
{
	if (StopNames.IsValidIndex(Stop))
	{
		return StopNames[Stop];
	}
	static const FText Defaults[] = { NSLOCTEXT("Saltbound", "HoistQuay", "Keelgate"), NSLOCTEXT("Saltbound", "HoistLower", "lower deck"),
		NSLOCTEXT("Saltbound", "HoistMain", "main deck") };
	return Stop >= 0 && Stop < 3 ? Defaults[Stop] : FText::GetEmpty();
}

bool ASaltHubHoist::GoTo(int32 Stop)
{
	if (!StopHeights.IsValidIndex(Stop) || IsMoving())
	{
		return false;
	}
	Target = Stop;
	if (Stop != At)
	{
		UE_LOG(LogSaltbound, Display, TEXT("SB_HOIST %d -> %d"), At, Stop);
	}
	return true;
}

void ASaltHubHoist::SetStopNow(int32 Stop)
{
	At = Target = FMath::Clamp(Stop, 0, FMath::Max(StopHeights.Num() - 1, 0));
	Height = StopZ(At);
	Speed = 0.f;
	Apply();
}

void ASaltHubHoist::HandleCall(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	const int32 Stop = Calls.IndexOfByKey(Interactable);
	if (Stop != INDEX_NONE && !Station->GetOccupant())
	{
		GoTo(Stop);
	}
}

void ASaltHubHoist::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The rider's keys: W up, S down, one stop per press.
	const FVector2D In = Station->GetControlInput();
	if (Station->GetOccupant() && !IsMoving() && !bInputLatch && FMath::Abs(In.Y) > 0.5f)
	{
		bInputLatch = true;
		const int32 Next = FMath::Clamp(At + (In.Y > 0.f ? 1 : -1), 0, StopHeights.Num() - 1);
		if (Next != At)
		{
			GoTo(Next);
		}
	}
	if (FMath::Abs(In.Y) < 0.2f)
	{
		bInputLatch = false;
	}
	// Eased travel: accelerate, run, brake into the stop.
	const float Goal = StopZ(Target);
	const float Remaining = Goal - Height;
	if (FMath::Abs(Remaining) > 0.5f || Speed != 0.f)
	{
		const float MaxV = HoistSpeed * 100.f;
		const float A = HoistAccel * 100.f;
		const float Want = FMath::Sign(Remaining) * FMath::Min(MaxV, FMath::Sqrt(2.f * A * FMath::Abs(Remaining)) + 2.f);
		Speed = FMath::FInterpConstantTo(Speed, Want, DeltaSeconds, A);
		Height += Speed * DeltaSeconds;
		if ((Goal - Height) * Remaining <= 0.f || FMath::Abs(Goal - Height) < 0.5f)
		{
			Height = Goal;
			Speed = 0.f;
			At = Target;
		}
	}
	Apply();
}

void ASaltHubHoist::Apply()
{
	Cage->SetRelativeLocation(FVector(0.0, 0.0, Height));
	// The counterweight runs the other way: down in its bay when the cage is up at the main deck.
	const float Top = StopZ(StopHeights.Num() - 1);
	const FVector WeightPos = WeightLow + FVector(0.0, 0.0, Top - Height);
	Weight->SetRelativeLocation(WeightPos);
	SaltHoistLocal::HangRope(CageRope, CageSheave, Height + YokeHeight);
	SaltHoistLocal::HangRope(WeightRope, WeightSheave, WeightPos.Z + WeightHeight);
	const bool bMoving = IsMoving();
	Station->bLeaveAllowed = !bMoving;
	Station->LeavePromptText = bMoving ? NSLOCTEXT("Saltbound", "HoistHold", "Hold on") : NSLOCTEXT("Saltbound", "HoistStepOut", "Step out");
	if (Station->Controls.Num() == 0)
	{
		Station->Controls = { { { TEXT("W") }, NSLOCTEXT("Saltbound", "HoistUp", "Up") }, { { TEXT("S") }, NSLOCTEXT("Saltbound", "HoistDown", "Down") } };
	}
	Station->ControlsHintText = bMoving ? FText::Format(NSLOCTEXT("Saltbound", "HoistTo", "to the {0}"), StopName(Target))
		: FText::Format(NSLOCTEXT("Saltbound", "HoistAt", "at the {0}"), StopName(At));
	for (int32 I = 0; I < MaxStops; ++I)
	{
		const bool bHere = !bMoving && At == I;
		Calls[I]->bInteractionEnabled = StopHeights.IsValidIndex(I) && !bHere && !Station->GetOccupant();
		const bool bGuard = I > 0 && StopHeights.IsValidIndex(I) && !bHere;
		Guards[I]->SetCollisionEnabled(bGuard ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		GuardRopes[I]->SetVisibility(bGuard);
	}
}

// Saltbound - the hub ship's sea gate (D62; see SaltSeaGate.h).

#include "Hub/SaltSeaGate.h"

#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Saltbound.h"

namespace SaltSeaGateLocal
{
	/** SM_Hub_GateRope hangs 27 m down from its pivot (build_hub_seagate.py ROPE_LEN). */
	constexpr float RopeMeshCm = 2700.f;

	ASaltSeaGate* FindGate(UWorld* World)
	{
		for (TActorIterator<ASaltSeaGate> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdSeaGate(
		TEXT("sb.Hub.SeaGate"),
		TEXT("Work the hub's sea gate: open | close | toggle | snapopen | snapclosed; without an argument it prints its state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltSeaGate* Gate = FindGate(World);
			if (!Gate)
			{
				UE_LOG(LogSaltbound, Warning, TEXT("SB_SEAGATE none in this world"));
				return;
			}
			const FString Arg = Args.Num() > 0 ? Args[0].ToLower() : FString();
			if (Arg == TEXT("open")) { Gate->Open(); }
			else if (Arg == TEXT("close")) { Gate->Close(); }
			else if (Arg == TEXT("toggle")) { Gate->Toggle(); }
			else if (Arg == TEXT("snapopen")) { Gate->SetOpenAmountNow(1.f); }
			else if (Arg == TEXT("snapclosed")) { Gate->SetOpenAmountNow(0.f); }
			UE_LOG(LogSaltbound, Display, TEXT("SB_SEAGATE amount=%.3f lift=%.0f moving=%d"), Gate->GetOpenAmount(), Gate->GetLiftCm(),
				Gate->IsMoving() ? 1 : 0);
		}));
}

ASaltSeaGate::ASaltSeaGate()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Gate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gate"));
	Gate->SetupAttachment(Root);
	Gate->SetMobility(EComponentMobility::Movable);
	for (int32 I = 0; I < MaxRopes; ++I)
	{
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

void ASaltSeaGate::Resolve()
{
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	if (!Gate->GetStaticMesh())
	{
		Gate->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(TEXT("SeaGate"))));
	}
	for (int32 I = 0; I < Ropes.Num(); ++I)
	{
		if (!Ropes[I]->GetStaticMesh())
		{
			Ropes[I]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(TEXT("GateRope"))));
		}
		Ropes[I]->SetVisibility(RopeTops.IsValidIndex(I));
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

void ASaltSeaGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Resolve();
	Amount = Target = From = bStartOpen ? 1.f : 0.f;
	Travel = 1.f;
	Apply();
}

void ASaltSeaGate::BeginPlay()
{
	Super::BeginPlay();
	Resolve();
	SetOpenAmountNow(bStartOpen ? 1.f : 0.f);
	for (USaltInteractableComponent* Use : Handles)
	{
		Use->OnInteract.AddDynamic(this, &ASaltSeaGate::HandleInteract);
	}
}

float ASaltSeaGate::GetLiftCm() const
{
	return RaiseCm * Amount;
}

void ASaltSeaGate::SetOpenAmountNow(float InAmount)
{
	Amount = Target = From = FMath::Clamp(InAmount, 0.f, 1.f);
	Travel = 1.f;
	Apply();
}

void ASaltSeaGate::Open()
{
	if (Target >= 1.f)
	{
		return;
	}
	From = Amount;
	Target = 1.f;
	Travel = 0.f;
}

void ASaltSeaGate::Close()
{
	if (Target <= 0.f)
	{
		return;
	}
	From = Amount;
	Target = 0.f;
	Travel = 0.f;
}

void ASaltSeaGate::Toggle()
{
	if (Target >= 0.5f)
	{
		Close();
	}
	else
	{
		Open();
	}
}

void ASaltSeaGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Travel >= 1.f)
	{
		return;
	}
	// Eased start and stop; a reversed move takes the time of the way left.
	const float Duration = FMath::Max(TravelSeconds * FMath::Abs(Target - From), 0.1f);
	Travel = FMath::Min(Travel + DeltaSeconds / Duration, 1.f);
	const float Ease = 0.5f - 0.5f * FMath::Cos(PI * Travel);
	Amount = FMath::Lerp(From, Target, Ease);
	if (Travel >= 1.f)
	{
		Amount = Target;
	}
	Apply();
}

void ASaltSeaGate::Apply()
{
	const float Lift = GetLiftCm();
	Gate->SetRelativeLocation(FVector(0.0, 0.0, Lift));
	// Each rope hangs from under its sheave to the gate's head; it pays in as the gate rises (gone into the gatehouse when
	// the head is up there).
	const float Head = Lift + GateHeightCm;
	for (int32 I = 0; I < Ropes.Num(); ++I)
	{
		if (!RopeTops.IsValidIndex(I))
		{
			continue;
		}
		const float Length = RopeTops[I].Z - Head;
		Ropes[I]->SetRelativeLocation(RopeTops[I]);
		Ropes[I]->SetRelativeScale3D(FVector(1.0, 1.0, FMath::Max(Length, 1.f) / SaltSeaGateLocal::RopeMeshCm));
		Ropes[I]->SetVisibility(Length > 5.f);
	}
	// The capstans turn with the gate (the two quays' turn opposite ways, each winding its own ropes).
	for (int32 I = 0; I < Capstans.Num(); ++I)
	{
		if (CapstanSpots.IsValidIndex(I))
		{
			Capstans[I]->SetRelativeRotation(FRotator(0.0, Amount * CapstanTurns * 360.0 * (I % 2 == 0 ? 1.0 : -1.0), 0.0));
		}
	}
	const bool bMoving = IsMoving();
	for (USaltInteractableComponent* Use : Handles)
	{
		Use->PromptText = Target >= 0.5f ? NSLOCTEXT("Saltbound", "SeaGateLower", "Lower the sea gate")
			: NSLOCTEXT("Saltbound", "SeaGateRaise", "Raise the sea gate");
	}
	for (int32 I = 0; I < Handles.Num(); ++I)
	{
		Handles[I]->bInteractionEnabled = CapstanSpots.IsValidIndex(I) && !bMoving;
	}
}

void ASaltSeaGate::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	if (!IsMoving())
	{
		Toggle();
		UE_LOG(LogSaltbound, Log, TEXT("SB_SEAGATE %s"), Target >= 0.5f ? TEXT("raising") : TEXT("lowering"));
		Apply();
	}
}

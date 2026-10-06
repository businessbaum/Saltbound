// Saltbound - a lifting bridge of the hub ship's harbour (D70; see SaltHubBridge.h).

#include "Hub/SaltHubBridge.h"

#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

ASaltHubBridge::ASaltHubBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	AftLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AftLeaf"));
	AftLeaf->SetupAttachment(Root);
	AftLeaf->SetMobility(EComponentMobility::Movable);
	ForeLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForeLeaf"));
	ForeLeaf->SetupAttachment(Root);
	ForeLeaf->SetMobility(EComponentMobility::Movable);
}

ASaltHubBridge* ASaltHubBridge::Find(const UWorld* World, int32 InStation, int32 InSide)
{
	for (TActorIterator<ASaltHubBridge> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Station == InStation && It->Side == InSide)
		{
			return *It;
		}
	}
	return nullptr;
}

void ASaltHubBridge::Resolve()
{
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	for (UStaticMeshComponent* Leaf : {AftLeaf.Get(), ForeLeaf.Get()})
	{
		if (!Leaf->GetStaticMesh())
		{
			Leaf->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::HubMesh(TEXT("BridgeLeaf"))));
		}
	}
}

void ASaltHubBridge::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Resolve();
	SetOpenNow(0.f);
}

void ASaltHubBridge::BeginPlay()
{
	Super::BeginPlay();
	Resolve();
	SetOpenNow(0.f);
}

void ASaltHubBridge::SetOpenNow(float InAmount)
{
	Amount = Target = FMath::Clamp(InAmount, 0.f, 1.f);
	Apply();
}

void ASaltHubBridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (FMath::IsNearlyEqual(Amount, Target))
	{
		return;
	}
	Amount = FMath::FInterpConstantTo(Amount, Target, DeltaSeconds, 1.f / FMath::Max(TravelSeconds, 0.5f));
	Apply();
}

void ASaltHubBridge::Apply()
{
	// Eased: the leaves start and stop gently; each pitches up about its own hinge (the fore one is turned round).
	const float Ease = 0.5f - 0.5f * FMath::Cos(PI * Amount);
	const float Pitch = OpenDegrees * Ease;
	AftLeaf->SetRelativeLocationAndRotation(FVector(-HalfSpanCm, 0.0, 0.0), FRotator(Pitch, 0.f, 0.f));
	ForeLeaf->SetRelativeLocationAndRotation(FVector(HalfSpanCm, 0.0, 0.0), FRotator(Pitch, 180.f, 0.f));
}

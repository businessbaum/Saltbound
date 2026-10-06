// Saltbound - the hub ship's hanging ropes and her lanterns' flicker (D69).

#include "Hub/SaltHubRopes.h"

#include "CableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/PointLight.h"
#include "Materials/MaterialInterface.h"

namespace SaltHubRopesLocal
{
	int32 SpansHash(const TArray<FSaltRopeSpan>& Spans, int32 NumMaterials)
	{
		uint32 H = GetTypeHash(Spans.Num()) ^ GetTypeHash(NumMaterials);
		for (const FSaltRopeSpan& S : Spans)
		{
			H = HashCombine(H, GetTypeHash(S.Start));
			H = HashCombine(H, GetTypeHash(S.End));
			H = HashCombine(H, GetTypeHash(S.Slack));
			H = HashCombine(H, GetTypeHash(S.Width));
		}
		return (int32)H;
	}
}

ASaltHubRopes::ASaltHubRopes()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASaltHubRopes::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildCables();
}

void ASaltHubRopes::BeginPlay()
{
	Super::BeginPlay();
	BuildCables();
	FindLanterns();
}

void ASaltHubRopes::BuildCables()
{
	const int32 Hash = SaltHubRopesLocal::SpansHash(Spans, Materials.Num());
	bool bAllValid = Cables.Num() == Spans.Num();
	for (UCableComponent* C : Cables)
	{
		bAllValid &= IsValid(C) && C->IsRegistered();
	}
	if (bAllValid && Hash == BuiltHash)
	{
		return;
	}
	for (UCableComponent* C : Cables)
	{
		if (IsValid(C))
		{
			C->DestroyComponent();
		}
	}
	Cables.Reset();
	Phases.Reset();
	// Transient: built from the spans every time, never saved with the level.
	for (int32 i = 0; i < Spans.Num(); ++i)
	{
		const FSaltRopeSpan& S = Spans[i];
		const float Dist = (float)FVector::Dist(S.Start, S.End);
		if (Dist < 5.f)
		{
			continue;
		}
		UCableComponent* C = NewObject<UCableComponent>(this, NAME_None, RF_Transient);
		C->SetupAttachment(GetRootComponent());
		C->SetRelativeLocation(S.Start);
		C->bAttachStart = true;
		C->bAttachEnd = true;
		C->EndLocation = S.End - S.Start;
		C->CableLength = Dist * FMath::Max(S.Slack, 1.f);
		C->NumSegments = FMath::Clamp(FMath::RoundToInt(Dist / 120.f), 4, 28);
		C->SolverIterations = 3;
		C->CableWidth = S.Width;
		C->NumSides = 6;
		C->TileMaterial = FMath::Max(C->CableLength / 100.f, 0.5f);
		C->CableGravityScale = 1.f;
		C->bResetAfterTeleport = true;
		C->TeleportDistanceThreshold = 2000.f;
		C->SetCastShadow(true);
		if (Materials.IsValidIndex(S.Material) && Materials[S.Material])
		{
			C->SetMaterial(0, Materials[S.Material]);
		}
		C->RegisterComponent();
		Cables.Add(C);
		Phases.Add(FMath::Frac(i * 0.6180339f) * 40.f);
	}
	BuiltHash = Hash;
}

void ASaltHubRopes::FindLanterns()
{
	Lamps.Reset();
	ULevel* Level = GetLevel();
	if (!Level)
	{
		return;
	}
	for (AActor* A : Level->Actors)
	{
		if (!A || !A->ActorHasTag(TEXT("HubLantern")))
		{
			continue;
		}
		if (UPointLightComponent* L = A->FindComponentByClass<UPointLightComponent>())
		{
			FLamp Lamp;
			Lamp.Light = L;
			Lamp.Base = L->Intensity;
			Lamp.Seed = FMath::Frac(Lamps.Num() * 0.7548776f) * 100.f;
			Lamps.Add(Lamp);
		}
	}
}

void ASaltHubRopes::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Cables.Num() != Spans.Num() || (Cables.Num() > 0 && !IsValid(Cables[0])))
	{
		BuildCables();
	}
	Time += DeltaSeconds;
	const FVector Wind = GetActorTransform().TransformVectorNoScale(WindDirection.GetSafeNormal());
	const FVector Side = FVector::CrossProduct(Wind, FVector::UpVector).GetSafeNormal();
	for (int32 i = 0; i < Cables.Num(); ++i)
	{
		UCableComponent* C = Cables[i];
		if (!IsValid(C) || !Spans.IsValidIndex(i))
		{
			continue;
		}
		const float P = Phases.IsValidIndex(i) ? Phases[i] : 0.f;
		// A gust front that runs along the ship (the ropes' own phase), a slow swell of the wind and a quick flutter across.
		const float Gust = FMath::PerlinNoise1D(Time * GustRate + P * 0.05f) * 0.7f + FMath::PerlinNoise1D(Time * GustRate * 3.1f + P) * 0.3f;
		const float Flutter = FMath::PerlinNoise1D(Time * 1.7f + P * 1.3f);
		const float Push = WindAccel * Spans[i].Wind * (1.f + GustAmount * Gust);
		C->CableForce = Wind * Push + Side * (WindAccel * 0.35f * Spans[i].Wind * Flutter);
	}
	if (FlickerAmount > 0.f && GetWorld() && GetWorld()->IsGameWorld())
	{
		for (FLamp& L : Lamps)
		{
			if (UPointLightComponent* Light = L.Light.Get())
			{
				const float Slow = FMath::PerlinNoise1D(Time * 0.9f + L.Seed);
				const float Fast = FMath::PerlinNoise1D(Time * 7.3f + L.Seed * 2.f);
				Light->SetIntensity(L.Base * (1.f + FlickerAmount * (0.7f * Slow + 0.3f * Fast)));
			}
		}
	}
}

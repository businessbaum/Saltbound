// Saltbound - seagulls soaring over an area (ambient life).

#include "World/SaltBirds.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/StaticMesh.h"

namespace
{
	/** Shoulder positions of the wings on the body mesh (cm, at BirdScale 1). */
	constexpr float ShoulderX = 4.f;
	constexpr float ShoulderY = 5.5f;
	constexpr float ShoulderZ = 3.f;

	UInstancedStaticMeshComponent* MakeIsm(AActor* Owner, FName Name, USceneComponent* Parent)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(true);
		C->bAffectDistanceFieldLighting = false;
		C->NumCustomDataFloats = 0;
		return C;
	}
}

ASaltBirds::ASaltBirds()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
	Bodies = MakeIsm(this, TEXT("Bodies"), Root);
	WingsLeft = MakeIsm(this, TEXT("WingsLeft"), Root);
	WingsRight = MakeIsm(this, TEXT("WingsRight"), Root);
}

void ASaltBirds::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveContent();
	InitBirds();
	UpdateBirds(0.f);
}

void ASaltBirds::BeginPlay()
{
	Super::BeginPlay();
	ResolveContent();
	InitBirds();
	UpdateBirds(0.f);
}

void ASaltBirds::ResolveContent()
{
	auto Resolve = [](UInstancedStaticMeshComponent* C, const TCHAR* Part)
	{
		if (C && !C->GetStaticMesh())
		{
			C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::BirdMesh(Part)));
		}
	};
	Resolve(Bodies, TEXT("Body"));
	Resolve(WingsLeft, TEXT("WingL"));
	Resolve(WingsRight, TEXT("WingR"));
}

void ASaltBirds::InitBirds()
{
	FRandomStream Rng(Seed);
	Birds.Reset();
	for (int32 I = 0; I < Count; ++I)
	{
		FBird B;
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = AreaRadius * FMath::Sqrt(Rng.FRand());
		B.Centre = FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
		const float D = Rng.FRandRange(0.f, 2.f * PI);
		B.Drift = FVector2D(FMath::Cos(D), FMath::Sin(D)) * Rng.FRandRange(30.f, 90.f);
		B.Radius = Rng.FRandRange(CircleRadius.X, CircleRadius.Y);
		B.Angle = Rng.FRandRange(0.f, 2.f * PI);
		B.Dir = Rng.FRand() < 0.5f ? -1.f : 1.f;
		B.BaseHeight = Rng.FRandRange(HeightRange.X, HeightRange.Y);
		B.HeightPhase = Rng.FRandRange(0.f, 2.f * PI);
		B.SpeedScale = Rng.FRandRange(0.85f, 1.15f);
		B.FlapPhase = Rng.FRandRange(0.f, 2.f * PI);
		B.FlapTimer = Rng.FRandRange(0.5f, 6.f);
		B.bFlapping = Rng.FRand() < 0.3f;
		Birds.Add(B);
	}
	for (UInstancedStaticMeshComponent* C : { Bodies.Get(), WingsLeft.Get(), WingsRight.Get() })
	{
		C->ClearInstances();
		for (int32 I = 0; I < Count; ++I)
		{
			C->AddInstance(FTransform::Identity, false);
		}
	}
}

void ASaltBirds::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBirds(FMath::Min(DeltaSeconds, 0.1f));
}

void ASaltBirds::UpdateBirds(float DeltaSeconds)
{
	if (Birds.Num() != Bodies->GetInstanceCount())
	{
		return;
	}
	Time += DeltaSeconds;
	FRandomStream Rng(Seed + (int32)(Time * 10.0));
	TArray<FTransform> BodyXf, LeftXf, RightXf;
	BodyXf.Reserve(Birds.Num());
	LeftXf.Reserve(Birds.Num());
	RightXf.Reserve(Birds.Num());
	const FVector Origin = GetActorLocation();
	for (FBird& B : Birds)
	{
		// Soaring circle around a slowly drifting centre (turned back towards the area when it strays).
		B.Centre += B.Drift * DeltaSeconds;
		if (B.Centre.Size() > AreaRadius)
		{
			B.Drift = -B.Centre.GetSafeNormal() * B.Drift.Size();
		}
		const float V = Speed * B.SpeedScale * (B.bFlapping ? 1.1f : 1.f);
		B.Angle += B.Dir * V / FMath::Max(B.Radius, 100.f) * DeltaSeconds;
		const FVector2D Radial(FMath::Cos(B.Angle), FMath::Sin(B.Angle));
		const FVector2D P = B.Centre + Radial * B.Radius;
		const float Height = B.BaseHeight + 400.f * FMath::Sin((float)Time * 0.11f + B.HeightPhase);
		const float Climb = 400.f * 0.11f * FMath::Cos((float)Time * 0.11f + B.HeightPhase);

		// Heading along the circle; bank into the turn (tan(bank) = v^2 / (r g)), a little pitch when climbing.
		const FVector2D Vel = FVector2D(-Radial.Y, Radial.X) * B.Dir;
		const float Yaw = FMath::Atan2(Vel.Y, Vel.X);
		const float Bank = FMath::Atan(V * V / (FMath::Max(B.Radius, 100.f) * 980.f));
		const float Pitch = FMath::Atan2(Climb, V);
		const FQuat Q = FQuat(FVector::UpVector, Yaw) * FQuat(FVector::RightVector, -Pitch) * FQuat(FVector::ForwardVector, -B.Dir * Bank);

		// Long glides with a slight dihedral, short bursts of flapping (about 3 beats a second).
		B.FlapTimer -= DeltaSeconds;
		if (B.FlapTimer <= 0.f)
		{
			B.bFlapping = !B.bFlapping;
			B.FlapTimer = B.bFlapping ? Rng.FRandRange(1.2f, 2.8f) : Rng.FRandRange(4.f, 10.f);
		}
		B.FlapBlend = FMath::FInterpTo(B.FlapBlend, B.bFlapping ? 1.f : 0.f, DeltaSeconds, 4.f);
		B.FlapPhase += DeltaSeconds * 2.f * PI * 2.9f;
		const float Flap = FMath::DegreesToRadians(10.f + B.FlapBlend * 38.f * FMath::Sin(B.FlapPhase)
			+ (1.f - B.FlapBlend) * 2.f * FMath::Sin((float)Time * 0.7f + B.HeightPhase));

		const FTransform Body(Q, Origin + FVector(P.X, P.Y, Height), FVector(BirdScale));
		// Left wing tip at -Y rises with a negative rotation about the forward axis, the right one with a positive.
		const FTransform Left(FQuat(FVector::ForwardVector, -Flap), FVector(ShoulderX, -ShoulderY, ShoulderZ));
		const FTransform Right(FQuat(FVector::ForwardVector, Flap), FVector(ShoulderX, ShoulderY, ShoulderZ));
		BodyXf.Add(Body);
		LeftXf.Add(Left * Body);
		RightXf.Add(Right * Body);
	}
	Bodies->BatchUpdateInstancesTransforms(0, BodyXf, true, true, false);
	WingsLeft->BatchUpdateInstancesTransforms(0, LeftXf, true, true, false);
	WingsRight->BatchUpdateInstancesTransforms(0, RightXf, true, true, false);
}

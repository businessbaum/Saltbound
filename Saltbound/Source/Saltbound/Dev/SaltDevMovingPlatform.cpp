// Saltbound - development: kinematic test deck that heaves, pitches, rolls and can be driven from a helm.

#include "Dev/SaltDevMovingPlatform.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/SaltStationComponent.h"
#include "UObject/ConstructorHelpers.h"

ASaltDevMovingPlatform::ASaltDevMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeFinder.Object;
	UStaticMesh* Cylinder = CylinderFinder.Object;

	// Unscaled pivot root; every part is a child sized in centimetres.
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Pivot);

	const float Top = DeckThickness * 0.5f;
	Deck = AddBlock(TEXT("Deck"), Cube, FVector::ZeroVector, FVector(800.f, 300.f, DeckThickness));
	// Bulwarks: both sides and the bow; the stern stays open for boarding tests.
	AddBlock(TEXT("BulwarkPort"), Cube, FVector(50.f, -147.f, Top + 45.f), FVector(700.f, 6.f, 90.f));
	AddBlock(TEXT("BulwarkStarboard"), Cube, FVector(50.f, 147.f, Top + 45.f), FVector(700.f, 6.f, 90.f));
	AddBlock(TEXT("BulwarkBow"), Cube, FVector(397.f, 0.f, Top + 45.f), FVector(6.f, 300.f, 90.f));
	// 140 cm wide so the side lanes (74 cm) fit the 68 cm player capsule.
	AddBlock(TEXT("Deckhouse"), Cube, FVector(170.f, 0.f, Top + 95.f), FVector(180.f, 140.f, 190.f));
	AddBlock(TEXT("Mast"), Cylinder, FVector(310.f, 0.f, Top + 300.f), FVector(16.f, 16.f, 600.f));
	AddBlock(TEXT("Step"), Cube, FVector(330.f, -105.f, Top + 9.f), FVector(50.f, 50.f, 18.f));
	AddBlock(TEXT("HelmPost"), Cylinder, FVector(-250.f, 0.f, Top + 50.f), FVector(12.f, 12.f, 100.f));

	Helm = CreateDefaultSubobject<USaltStationComponent>(TEXT("Helm"));
	Helm->SetupAttachment(Pivot);
	Helm->SetRelativeLocation(FVector(-250.f, 0.f, Top + 105.f));
	Helm->SetBoxExtent(FVector(20.f, 35.f, 25.f));
	Helm->StandOffset = FVector(-60.f, 0.f, -105.f);
	Helm->ExitOffset = FVector(-110.f, 0.f, -105.f);
	Helm->PromptText = NSLOCTEXT("Saltbound", "DevHelmUse", "Take the helm");
	Helm->LeavePromptText = NSLOCTEXT("Saltbound", "DevHelmLeave", "Leave the helm");
	Helm->Controls = { { { TEXT("W"), TEXT("S") }, NSLOCTEXT("Saltbound", "DevHelmSpeed", "Speed") },
		{ { TEXT("A"), TEXT("D") }, NSLOCTEXT("Saltbound", "DevHelmSteer", "Steer") } };
}

UStaticMeshComponent* ASaltDevMovingPlatform::AddBlock(FName Name, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm)
{
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Pivot);
	C->SetRelativeLocation(Center);
	C->SetRelativeScale3D(SizeCm / 100.f);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	return C;
}

void ASaltDevMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
	const FVector L = GetActorLocation();
	PlanarPosition = FVector2D(L.X, L.Y);
	BaseZ = L.Z;
	Heading = GetActorRotation().Yaw;
}

void ASaltDevMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DeltaSeconds <= 0.f)
	{
		return;
	}
	Time += DeltaSeconds;

	float TurnInput = 0.f;
	if (bUseForcedDrive)
	{
		Speed = ForcedSpeed;
		Heading += ForcedTurnRate * DeltaSeconds;
	}
	else
	{
		const FVector2D In = Helm ? Helm->GetControlInput() : FVector2D::ZeroVector;
		Speed = FMath::Clamp(Speed + In.Y * Acceleration * DeltaSeconds, -150.f, MaxSpeed);
		if (FMath::IsNearlyZero(In.Y))
		{
			Speed = FMath::FInterpTo(Speed, 0.f, DeltaSeconds, 0.15f);
		}
		TurnInput = In.X;
		const float Steerage = FMath::Clamp(FMath::Abs(Speed) / 150.f, 0.25f, 1.f);
		Heading += TurnInput * TurnRate * Steerage * DeltaSeconds;
	}

	const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(Heading)), FMath::Sin(FMath::DegreesToRadians(Heading)));
	PlanarPosition += Dir * Speed * DeltaSeconds;

	const double TwoPi = 2.0 * UE_DOUBLE_PI;
	const float Heave = MotionScale * HeaveAmplitude * FMath::Sin(TwoPi * Time / HeavePeriod);
	const float Pitch = MotionScale * PitchAmplitude * FMath::Sin(TwoPi * Time / PitchPeriod + 1.1);
	const float Roll = MotionScale * RollAmplitude * FMath::Sin(TwoPi * Time / RollPeriod + 0.4);

	const FVector OldLocation = GetActorLocation();
	const FQuat OldRotation = GetActorQuat();
	const FVector NewLocation(PlanarPosition.X, PlanarPosition.Y, BaseZ + Heave);
	SetActorLocationAndRotation(NewLocation, FRotator(Pitch, Heading, Roll), false, nullptr, ETeleportType::None);
	AngularVelocity = AngularVelocityFromDelta(OldRotation, GetActorQuat(), DeltaSeconds);

	// Let CharacterMovement know how fast the deck moves (used when jumping off / impart velocity).
	const FVector Velocity = (NewLocation - OldLocation) / DeltaSeconds;
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* P : Prims)
	{
		P->ComponentVelocity = Velocity;
	}
}

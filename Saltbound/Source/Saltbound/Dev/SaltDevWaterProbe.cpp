// Saltbound - development: marker that sits on the CPU-evaluated water surface.

#include "Dev/SaltDevWaterProbe.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Ocean/SaltOcean.h"
#include "Ocean/SaltOceanSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ASaltDevWaterProbe::ASaltDevWaterProbe()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetStaticMesh(Sphere.Object);
	Marker->SetRelativeScale3D(FVector(0.3f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Marker);
}

void ASaltDevWaterProbe::BeginPlay()
{
	Super::BeginPlay();
	bAnchored = false;
}

void ASaltDevWaterProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAnchorAtPlacement && !bAnchored)
	{
		AnchorXY = FVector2D(GetActorLocation().X, GetActorLocation().Y);
		bAnchored = true;
	}
	if (const USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>())
	{
		const FSaltWaterSample S = Sub->SampleWater(AnchorXY);
		SetActorLocation(S.Location, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

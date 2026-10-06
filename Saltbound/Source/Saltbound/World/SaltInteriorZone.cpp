// Saltbound - an enclosed space (D69).

#include "World/SaltInteriorZone.h"

#include "Components/SceneComponent.h"

ASaltInteriorZone::ASaltInteriorZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

float ASaltInteriorZone::WeightAt(const FVector& WorldLocation) const
{
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	const float Depth = (float)FMath::Min3(Extent.X - FMath::Abs(L.X), Extent.Y - FMath::Abs(L.Y), Extent.Z - FMath::Abs(L.Z));
	const float T = FMath::Clamp(Depth / FMath::Max(BlendCm, 1.f), 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}

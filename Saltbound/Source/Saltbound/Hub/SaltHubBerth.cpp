// Saltbound - a berth of the hub ship (D62).

#include "Hub/SaltHubBerth.h"

#include "Components/SceneComponent.h"

ASaltHubBerth::ASaltHubBerth()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

TArray<FSaltCalmZone> ASaltHubBerth::GetWorldCalmZones() const
{
	TArray<FSaltCalmZone> Out;
	const FTransform T = GetActorTransform();
	for (FSaltCalmZone Z : CalmZones)
	{
		const FVector W = T.TransformPosition(FVector(Z.Center.X, Z.Center.Y, 0.0));
		Z.Center = FVector2D(W.X, W.Y);
		Out.Add(Z);
	}
	return Out;
}

FSaltShelterBox ASaltHubBerth::GetWorldShelter() const
{
	FSaltShelterBox Out = Shelter;
	const FVector W = GetActorTransform().TransformPosition(FVector(Shelter.Center.X, Shelter.Center.Y, 0.0));
	Out.Center = FVector2D(W.X, W.Y);
	Out.Yaw = (float)GetActorRotation().Yaw + Shelter.Yaw;
	return Out;
}

FVector ASaltHubBerth::ShipToWorld(const FVector& ShipM) const
{
	return GetActorTransform().TransformPosition(FVector((ShipM.X - PivotX) * 100.0, ShipM.Y * 100.0, ShipM.Z * 100.0));
}

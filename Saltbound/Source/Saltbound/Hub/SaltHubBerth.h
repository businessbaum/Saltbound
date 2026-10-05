// Saltbound - a berth of the hub ship (D62, phase 3 of the hub prompt): where she lies off a harbour, and her calm water there.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ocean/SaltWaveTypes.h"
#include "SaltHubBerth.generated.h"

/**
 * The actor's transform is the ship's: her pivot (x 80 of the ship's frame on the waterline) and her heading. While she lies
 * here her calm zones are added to the ocean (centres in this actor's frame, cm). The player has "sailed to" its harbour once
 * within HarbourRadiusM of it.
 */
UCLASS()
class SALTBOUND_API ASaltHubBerth : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubBerth();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName BerthId;
	/** Where she lies at the start of a game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") bool bDefault = false;
	/** The harbour round the berth (m): the player has sailed to it within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "m")) float HarbourRadiusM = 1200.f;
	/** Her calm zones while she lies here; centres in this actor's frame (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FSaltCalmZone> CalmZones;

	TArray<FSaltCalmZone> GetWorldCalmZones() const;
	/** A point of the ship's frame (metres: x from the stern face, y starboard, z up) in the world (cm) at this berth. */
	FVector ShipToWorld(const FVector& ShipM) const;
	float ShipYawToWorld(float FaceDeg) const { return (float)GetActorRotation().Yaw + FaceDeg; }

	/** The ship's pivot along her frame (hub_layout.PIVOT_X). */
	static constexpr float PivotX = 80.f;
};

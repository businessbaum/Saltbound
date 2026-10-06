// Saltbound - an enclosed space (D69): while the camera is inside, the sea haze stays outside.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltInteriorZone.generated.h"

/**
 * A box in the actor's frame (half extents in cm). The height fog is one for the whole world and walls do not stop it,
 * so inside the hub's hull it filled the halls with the sea's haze (and with the eye opened for the dark, it glowed). While
 * the camera is inside a zone, ASaltLightingRig pushes the fog's start out beyond the space (FogStartCm) and scales its
 * density, blending over BlendCm at the box's faces (doorways, the side doors, the Well's shaft). Several zones may
 * overlap: the strongest wins.
 */
UCLASS()
class SALTBOUND_API ASaltInteriorZone : public AActor
{
	GENERATED_BODY()

public:
	ASaltInteriorZone();

	/** Half the box's size along the actor's axes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior") FVector Extent = FVector(1000.f, 1000.f, 300.f);
	/** How far inside the faces the interior is fully applied (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (Units = "cm", ClampMin = "1")) float BlendCm = 250.f;
	/** The fog starts this far from the camera inside (cm): beyond the longest view in the space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (Units = "cm")) float FogStartCm = 16000.f;
	/** The fog's density inside, relative to outside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (ClampMin = "0", ClampMax = "1")) float FogDensityScale = 0.5f;

	/** 0 outside, 1 deeper than BlendCm inside (smooth). */
	float WeightAt(const FVector& WorldLocation) const;
};

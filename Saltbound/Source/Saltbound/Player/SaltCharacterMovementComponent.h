// Saltbound - CharacterMovement with support for kinematic (code-driven) moving bases and swimming in the
// ocean (whose surface moves with the waves, so it cannot be a static water volume).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SaltCharacterMovementComponent.generated.h"

class APhysicsVolume;

UCLASS()
class SALTBOUND_API USaltCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USaltCharacterMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Adds the tangential velocity of rotating kinematic bases (see ISaltKinematicBase). */
	virtual FVector GetImpartedMovementBaseVelocity() const override;

	/** In water: the ocean surface at the character is above StartSwimImmersion of the capsule (with
	 *  hysteresis down to StopSwimImmersion, so wading out of the sea does not flicker). */
	virtual bool IsInWater() const override;
	/** Share of the capsule below the ocean surface (0 = dry, 1 = fully submerged); drives buoyancy. */
	virtual float ImmersionDepth() const override;

	/** Ocean surface height at the character this frame (very low when there is no ocean). */
	float GetWaterSurfaceZ() const { return WaterSurfaceZ; }

	/** Leave the water now (e.g. climbing out); swimming cannot restart for Seconds. */
	void SuppressWater(float Seconds);

	/** Start swimming when the water reaches this share of the capsule (about chest height)... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", ClampMax = "1"))
	float StartSwimImmersion = 0.6f;

	/** ...and stop again below this share (wading out on a beach or sandbank). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", ClampMax = "1"))
	float StopSwimImmersion = 0.45f;

	/** Buoyancy while the head is at the surface (the body floats with the eyes just above the water). Fully under
	 *  water the swimmer is neutral and hovers where they stop: no drifting up or down without input (owner, D48). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "1"))
	float SurfaceBuoyancy = 1.2f;

private:
	void UpdateWater(float DeltaTime);

	/** Brushless water volume assigned to the capsule while in the sea (CharacterMovement swims while its
	 *  physics volume is a water volume); the world's default volume otherwise. */
	UPROPERTY(Transient)
	TObjectPtr<APhysicsVolume> SeaVolume;

	float WaterSurfaceZ = -1.0e6f;
	float Immersion = 0.f;
	bool bInWater = false;
	float SuppressTime = 0.f;
};

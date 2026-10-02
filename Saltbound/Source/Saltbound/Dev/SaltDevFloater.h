// Saltbound - development: static mesh that rides the ocean surface (simple kinematic buoyancy preview).

#pragma once

#include "CoreMinimal.h"
#include "Core/SaltKinematicBase.h"
#include "GameFramework/Actor.h"
#include "SaltDevFloater.generated.h"

class UStaticMeshComponent;

/**
 * Samples the water at four hull points, fits heave/pitch/roll and follows them through a damped
 * spring. Kinematic (walkable via CharacterMovement based movement). A preview of the M4 boat motion.
 */
UCLASS()
class SALTBOUND_API ASaltDevFloater : public AActor, public ISaltKinematicBase
{
	GENERATED_BODY()

public:
	ASaltDevFloater();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual FVector GetKinematicAngularVelocity() const override { return AngularVelocity; }
	virtual FVector GetKinematicPivot() const override { return GetActorLocation(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Deck (pivot) height above the local water surface. */
	UPROPERTY(EditAnywhere, Category = "Float", meta = (Units = "cm")) float Freeboard = 75.f;
	/** Half length / half beam of the sampling footprint. */
	UPROPERTY(EditAnywhere, Category = "Float", meta = (Units = "cm")) FVector2D HalfExtent = FVector2D(180.f, 140.f);
	/** Response: natural frequency (Hz) and damping ratio of the heave/attitude spring. */
	UPROPERTY(EditAnywhere, Category = "Float") float Frequency = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Float") float Damping = 0.7f;

private:
	FVector BaseXY = FVector::ZeroVector;
	float Yaw = 0.f;
	FVector State = FVector::ZeroVector;     // heave (cm), pitch (deg), roll (deg)
	FVector StateVel = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
	bool bInitialised = false;
};

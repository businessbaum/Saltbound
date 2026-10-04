// Saltbound - something hanging from a pivot (the boat's bell and its clapper, the lantern) that swings like a damped
// pendulum, driven by its pivot's real motion: the boat's roll, pitch, heave and turns, and whoever bumps into it (D53).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SaltSwingComponent.generated.h"

/**
 * Sits at the pivot; attach the hanging meshes to it. Its -Z points from the pivot to the hanging body. At rest it
 * hangs straight down; it never swings further than MaxAngle from its parent's down (it strikes its bracket, or the
 * clapper strikes the bell). It ticks after physics so it sees where the boat moved this frame.
 */
UCLASS(ClassGroup = (Saltbound), meta = (BlueprintSpawnableComponent))
class SALTBOUND_API USaltSwingComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USaltSwingComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pivot to the body's centre of mass: sets the period (about 2 pi sqrt(L / g), 0.75 s for 14 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (Units = "cm", ClampMin = "1"))
	float Length = 14.f;

	/** Fraction of critical damping (air and the pivot's friction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (ClampMin = "0", ClampMax = "1"))
	float Damping = 0.05f;

	/** Largest angle from the parent's down; the body strikes there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (Units = "deg", ClampMin = "1", ClampMax = "170"))
	float MaxAngle = 40.f;

	/** Fraction of the speed into the limit that bounces back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (ClampMin = "0", ClampMax = "1"))
	float Restitution = 0.4f;

	/** The player pushes the body when its capsule comes this close to it (cm beyond the capsule's radius; 0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (Units = "cm", ClampMin = "0"))
	float PushRadius = 0.f;

	/** Adds a velocity (cm/s, world) to the hanging body: a hand, a gust. */
	UFUNCTION(BlueprintCallable, Category = "Swing")
	void Kick(const FVector& Velocity);

	/** Hangs straight down at rest (after a teleport or on start). */
	UFUNCTION(BlueprintCallable, Category = "Swing")
	void Settle();

	/** Angle from hanging straight down in the parent's frame (deg). */
	UFUNCTION(BlueprintPure, Category = "Swing")
	float GetSwingAngle() const;

	/** How often the body has struck its limit (bracket, bell) since play began. */
	int32 GetStrikeCount() const { return Strikes; }

	/** Speed (cm/s) of the last strike. */
	float GetLastStrikeSpeed() const { return LastStrikeSpeed; }

private:
	FVector ParentDown() const;
	FQuat ParentQuat() const;
	void ApplyPose();

	FVector Bob = FVector::ZeroVector;          // world, cm
	FVector BobVel = FVector::ZeroVector;       // world, cm/s
	FVector LastPivot = FVector::ZeroVector;
	FVector LastPawn = FVector::ZeroVector;
	bool bHasPawn = false;
	bool bStarted = false;
	int32 Strikes = 0;
	float LastStrikeSpeed = 0.f;
	float StrikeCooldown = 0.f;
};

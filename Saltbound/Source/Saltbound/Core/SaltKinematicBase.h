// Saltbound - interface for code-driven (kinematic) actors that characters can stand on.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaltKinematicBase.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USaltKinematicBase : public UInterface
{
	GENERATED_BODY()
};

/**
 * Kinematic movers (boat, test deck) have no physics angular velocity, so CharacterMovement cannot
 * impart the tangential part of the deck motion when the player jumps or walks off. Implementers report
 * their current world angular velocity; USaltCharacterMovementComponent adds the missing term.
 */
class SALTBOUND_API ISaltKinematicBase
{
	GENERATED_BODY()

public:
	/** World-space angular velocity in radians per second (axis * rate). */
	virtual FVector GetKinematicAngularVelocity() const = 0;
	/** World-space point the angular velocity is measured around (usually the actor location). */
	virtual FVector GetKinematicPivot() const = 0;

	/** Angular velocity from two orientations one step apart. */
	static FVector AngularVelocityFromDelta(const FQuat& Previous, const FQuat& Current, float DeltaSeconds)
	{
		if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}
		FQuat Delta = Current * Previous.Inverse();
		Delta.EnforceShortestArcWith(FQuat::Identity);
		FVector Axis;
		float Angle;
		Delta.ToAxisAndAngle(Axis, Angle);
		return Axis * (Angle / DeltaSeconds);
	}
};

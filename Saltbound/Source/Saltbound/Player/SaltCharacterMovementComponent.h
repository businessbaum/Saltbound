// Saltbound - CharacterMovement with support for kinematic (code-driven) moving bases.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SaltCharacterMovementComponent.generated.h"

UCLASS()
class SALTBOUND_API USaltCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** Adds the tangential velocity of rotating kinematic bases (see ISaltKinematicBase). */
	virtual FVector GetImpartedMovementBaseVelocity() const override;
};

// Saltbound - CharacterMovement with support for kinematic (code-driven) moving bases.

#include "Player/SaltCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/SaltKinematicBase.h"
#include "GameFramework/Character.h"

FVector USaltCharacterMovementComponent::GetImpartedMovementBaseVelocity() const
{
	FVector Result = Super::GetImpartedMovementBaseVelocity();
	if (!bImpartBaseAngularVelocity || !CharacterOwner || !UpdatedComponent)
	{
		return Result;
	}
	const UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(GetMovementBaseObject());
	const ISaltKinematicBase* Kinematic = Base ? Cast<ISaltKinematicBase>(Base->GetOwner()) : nullptr;
	if (!Kinematic)
	{
		return Result;
	}
	const FVector Omega = Kinematic->GetKinematicAngularVelocity();
	if (Omega.IsNearlyZero())
	{
		return Result;
	}
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, HalfHeight);
	const FVector Tangential = FVector::CrossProduct(Omega, Feet - Kinematic->GetKinematicPivot());
	Result.X += bImpartBaseVelocityX ? Tangential.X : 0.f;
	Result.Y += bImpartBaseVelocityY ? Tangential.Y : 0.f;
	Result.Z += bImpartBaseVelocityZ ? Tangential.Z : 0.f;
	return Result;
}

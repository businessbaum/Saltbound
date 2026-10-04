// Saltbound - CharacterMovement with support for kinematic (code-driven) moving bases and swimming in the
// ocean.

#include "Player/SaltCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/SaltKinematicBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PhysicsVolume.h"
#include "Ocean/SaltOcean.h"
#include "Ocean/SaltOceanSubsystem.h"

USaltCharacterMovementComponent::USaltCharacterMovementComponent()
{
	// Swimming: at the surface the eyes float just above the water (equilibrium immersion = 1 / Buoyancy ~ 0.83 of
	// the capsule) and the body rides the waves; fully under water the swimmer is neutral and hovers (D48).
	MaxSwimSpeed = 300.f;
	Buoyancy = SurfaceBuoyancy;
	// No constant braking: it would cancel the buoyant lift at the surface every frame. The sea volume's fluid
	// friction damps instead (proportional to speed): the swimmer stops and hovers, or settles at the surface.
	BrakingDecelerationSwimming = 0.f;
	GetNavAgentPropertiesRef().bCanSwim = true;
}

void USaltCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || !UpdatedComponent)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SeaVolume = World->SpawnActor<APhysicsVolume>(APhysicsVolume::StaticClass(), FTransform::Identity, Params);
	if (SeaVolume)
	{
		SeaVolume->bWaterVolume = true;
		SeaVolume->FluidFriction = 2.0f;
		SeaVolume->TerminalVelocity = 2000.f;
	}
	// The sea assigns the physics volume itself (the surface moves); overlaps must not reset it.
	UpdatedComponent->SetShouldUpdatePhysicsVolume(false);
}

void USaltCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	UpdateWater(DeltaTime);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USaltCharacterMovementComponent::UpdateWater(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !UpdatedComponent || !CharacterOwner || !SeaVolume)
	{
		return;
	}
	SuppressTime = FMath::Max(0.f, SuppressTime - DeltaTime);
	const USaltOceanSubsystem* Sub = World->GetSubsystem<USaltOceanSubsystem>();
	const ASaltOcean* Ocean = Sub ? Sub->GetOcean() : nullptr;
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const float Half = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Ocean)
	{
		WaterSurfaceZ = Sub->SampleWater(FVector2D(Location.X, Location.Y)).Location.Z;
		Immersion = FMath::Clamp((WaterSurfaceZ - (Location.Z - Half)) / (2.f * Half), 0.f, 1.f);
	}
	else
	{
		WaterSurfaceZ = -1.0e6f;
		Immersion = 0.f;
	}
	// Neutral once fully under water (no auto rise: the swimmer hovers), buoyant as the head breaks the surface.
	Buoyancy = FMath::Lerp(SurfaceBuoyancy, 1.f, FMath::SmoothStep(0.97f, 1.f, Immersion));
	const bool bWant = SuppressTime <= 0.f && (bInWater ? Immersion > StopSwimImmersion : Immersion > StartSwimImmersion);
	if (bWant != bInWater)
	{
		bInWater = bWant;
		// SetPhysicsVolume notifies CharacterMovement (PhysicsVolumeChanged): it starts swimming on entering a
		// water volume and falls on leaving one.
		UpdatedComponent->SetPhysicsVolume(bInWater ? SeaVolume.Get() : World->GetDefaultPhysicsVolume(), true);
	}
	else if (bInWater && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling))
	{
		// Still in the water after a teleport or a landing that reset the mode.
		SetMovementMode(MOVE_Swimming);
	}
}

void USaltCharacterMovementComponent::SuppressWater(float Seconds)
{
	SuppressTime = FMath::Max(SuppressTime, Seconds);
	if (bInWater && UpdatedComponent && GetWorld())
	{
		bInWater = false;
		UpdatedComponent->SetPhysicsVolume(GetWorld()->GetDefaultPhysicsVolume(), true);
	}
}

bool USaltCharacterMovementComponent::IsInWater() const
{
	return bInWater;
}

float USaltCharacterMovementComponent::ImmersionDepth() const
{
	return Immersion;
}

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

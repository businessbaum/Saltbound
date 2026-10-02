// Saltbound - development: kinematic test deck that heaves, pitches, rolls and can be driven from a helm.

#pragma once

#include "CoreMinimal.h"
#include "Core/SaltKinematicBase.h"
#include "GameFramework/Actor.h"
#include "SaltDevMovingPlatform.generated.h"

class UStaticMeshComponent;
class USaltStationComponent;

/**
 * Stand-in for the boat while the player and station systems are developed (Milestone 2).
 * Moves kinematically in TG_PrePhysics exactly like the real boat will: the transform is set by code,
 * the deck's ComponentVelocity is updated for CharacterMovement's base-velocity handling.
 * Deck is 8 x 3 m; open stern; a deckhouse block, mast and a helm station.
 */
UCLASS()
class SALTBOUND_API ASaltDevMovingPlatform : public AActor, public ISaltKinematicBase
{
	GENERATED_BODY()

public:
	ASaltDevMovingPlatform();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UStaticMeshComponent* GetDeck() const { return Deck; }
	USaltStationComponent* GetHelm() const { return Helm; }
	/** Deck surface height in actor space. */
	float GetDeckTopZ() const { return DeckThickness * 0.5f; }

	/** Scales heave/pitch/roll amplitudes (0 = flat calm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float MotionScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "Motion", meta = (Units = "cm")) float HeaveAmplitude = 25.f;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (Units = "s")) float HeavePeriod = 6.5f;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (Units = "deg")) float PitchAmplitude = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (Units = "s")) float PitchPeriod = 7.7f;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (Units = "deg")) float RollAmplitude = 4.f;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (Units = "s")) float RollPeriod = 5.3f;

	/** Helm drive: forward input accelerates, side input turns. */
	UPROPERTY(EditAnywhere, Category = "Drive", meta = (Units = "cm/s")) float MaxSpeed = 500.f;
	UPROPERTY(EditAnywhere, Category = "Drive") float Acceleration = 120.f;
	UPROPERTY(EditAnywhere, Category = "Drive", meta = (Units = "deg/s")) float TurnRate = 12.f;

	/** Development override: constant speed / turn rate regardless of the helm (sb.Dev.Platform). */
	UPROPERTY(EditAnywhere, Category = "Drive") bool bUseForcedDrive = false;
	UPROPERTY(EditAnywhere, Category = "Drive", meta = (Units = "cm/s")) float ForcedSpeed = 0.f;
	UPROPERTY(EditAnywhere, Category = "Drive", meta = (Units = "deg/s")) float ForcedTurnRate = 0.f;

	float GetSpeed() const { return Speed; }

	//~ ISaltKinematicBase
	virtual FVector GetKinematicAngularVelocity() const override { return AngularVelocity; }
	virtual FVector GetKinematicPivot() const override { return GetActorLocation(); }

private:
	UStaticMeshComponent* AddBlock(FName Name, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Deck;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltStationComponent> Helm;

	static constexpr float DeckThickness = 20.f;

	double Time = 0.0;
	FVector2D PlanarPosition = FVector2D::ZeroVector;
	float BaseZ = 0.f;
	float Heading = 0.f;
	float Speed = 0.f;
	FVector AngularVelocity = FVector::ZeroVector;
};

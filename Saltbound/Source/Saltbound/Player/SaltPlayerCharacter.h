// Saltbound - first-person player character.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SaltPlayerCharacter.generated.h"

class UCameraComponent;
class USaltInteractableComponent;
class USaltStationComponent;
struct FInputActionValue;

/**
 * First-person character without a visible body.
 * - Walks on moving decks via CharacterMovement based movement (the boat/platform must move before the
 *   character ticks; CharacterMovement adds that tick dependency when it bases on a component).
 * - Looks at interactables through a short sphere probe on the Interaction channel, with an occlusion check.
 * - Steps into control stations: the capsule is held at the station's stand point (in station space, so
 *   it follows the moving deck exactly) and movement input is routed to the station.
 * The camera never rolls; pitch and roll of the deck only move the eye point.
 */
UCLASS()
class SALTBOUND_API ASaltPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ASaltPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UCameraComponent* GetCamera() const { return Camera; }

	// Interaction -------------------------------------------------------------
	USaltInteractableComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }
	/** Prompt for the HUD (empty when there is nothing to do). */
	FText GetPromptText() const;
	/** Secondary hint for the HUD, e.g. station controls. */
	FText GetHintText() const;

	// Stations ----------------------------------------------------------------
	void EnterStation(USaltStationComponent* Station);
	void LeaveStation();
	bool IsAtStation() const { return ActiveStation != nullptr; }
	USaltStationComponent* GetActiveStation() const { return ActiveStation; }

	// Development / tests -----------------------------------------------------
	/** Places the player (feet) at a world location on foot, facing Yaw. */
	void TeleportFeetTo(const FVector& FeetLocation, float Yaw);
	/** Simulated movement input for automated tests. Zero vector disables it. */
	void SetDebugMoveInput(const FVector2D& Input) { DebugMoveInput = Input; }
	/** Programmatic interaction for automated tests (same path as the key press). */
	void DebugInteract() { HandleInteract(); }
	/** Programmatic jump press/release for automated tests. */
	void DebugJump(bool bPressed) { bPressed ? HandleJumpStarted() : HandleJumpCompleted(); }
	/** Programmatic dive press/release for automated tests. */
	void DebugDive(bool bPressed) { bPressed ? HandleDiveStarted() : HandleDiveCompleted(); }

	// Tunables ----------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float WalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", Units = "cm/s"))
	float SwimSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", Units = "cm/s"))
	float SwimSprintSpeed = 460.f;

	/** Highest ledge above the feet that the player can climb onto from the water (pier deck, boat side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", Units = "cm"))
	float ClimbOutReach = 300.f;

	/** True while swimming in the sea (surface or under water). */
	bool IsSwimming() const;

	/** Scripted climb along a path (capsule centre points in Base's space, cm, so it follows a moving boat). Stepped
	 *  segments go rung by rung with a dip of the head on each pull; the others glide. bEndInWater: the climb ends
	 *  in the sea (down a ladder), otherwise standing on the top. */
	void StartScriptedClimb(USceneComponent* Base, const TArray<FVector>& LocalPoints, const TArray<bool>& Stepped, bool bEndInWater);
	bool IsClimbing() const { return bClimbing; }
	float GetCapsuleHalfHeightForClimb() const;

	/** Height of one climbing step (ladder rung) and the time it takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "cm"))
	float ClimbStepHeight = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "s"))
	float ClimbStepTime = 0.34f;

	/** Camera height above the feet (capsule height is 150 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Camera", meta = (ClampMin = "100", ClampMax = "180", Units = "cm"))
	float EyeHeight = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Camera", meta = (ClampMin = "60", ClampMax = "110"))
	float FieldOfView = 80.f;

	/** Degrees of rotation per engine-scaled mouse count. Console: sb.MouseSensitivity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Input", meta = (ClampMin = "0.05", ClampMax = "10"))
	float MouseSensitivity = 1.0f;

	/** Gamepad turn rate at full stick deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Input", meta = (Units = "deg/s"))
	float GamepadLookRate = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Input")
	bool bInvertLookY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Interaction", meta = (Units = "cm"))
	float InteractionRange = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Interaction", meta = (Units = "cm"))
	float InteractionProbeRadius = 5.f;

	/** Time to step into a station's stand point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Interaction", meta = (Units = "s"))
	float StationBlendTime = 0.45f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleLookMouse(const FInputActionValue& Value);
	void HandleLookGamepad(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpCompleted();
	void HandleSprintStarted();
	void HandleSprintCompleted();
	void HandleDiveStarted();
	void HandleDiveCompleted();
	void HandleInteract();
	/** Starts climbing onto a walkable ledge in front when swimming at the surface. */
	bool TryClimbOutOfWater();
	void TickClimb(float DeltaSeconds);

	void ApplyMoveInput(const FVector2D& Input);
	void ApplyLook(float YawDelta, float PitchDelta);
	void UpdateInteractionFocus();
	void TickStation(float DeltaSeconds);
	bool IsCapsuleSpotFree(const FVector& CapsuleCenter) const;
	float GetStationYaw() const;

	TWeakObjectPtr<USaltInteractableComponent> FocusedInteractable;

	UPROPERTY(Transient)
	TObjectPtr<USaltStationComponent> ActiveStation;

	/** Feet position at the moment of entering, in station space (blend start and exit fallback). */
	FVector StationEntryLocal = FVector::ZeroVector;
	float StationBlendAlpha = 0.f;
	float StationTime = 0.f;
	float StationLookYaw = 0.f;
	float StationLookPitch = 0.f;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector2D DebugMoveInput = FVector2D::ZeroVector;
	bool bSprinting = false;
	bool bSwimUpHeld = false;
	bool bSwimDownHeld = false;

	/** Climbs (out of the water, ladders): capsule centre path in the space of the ledge's component (boats move). */
	bool bClimbing = false;
	bool bClimbEndInWater = false;
	TArray<FVector> ClimbPoints;
	TArray<bool> ClimbStepped;
	int32 ClimbSegment = 0;
	float ClimbSegmentTime = 0.f;
	TWeakObjectPtr<USceneComponent> ClimbBase;
	float SegmentDuration(int32 Segment) const;
};

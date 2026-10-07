// Saltbound - first-person player character.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/SaltStationComponent.h"
#include "SaltPlayerCharacter.generated.h"

class UCameraComponent;
class USaltInventoryComponent;
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
/** A ladder to hold on to (D54): the capsule centre's line from Bottom to Top in Base's space (cm, so it follows a moving
 *  boat), the yaw in Base's space that faces the rungs, the rung spacing, and where the top lets you off on deck. */
struct FSaltLadderLine
{
	TWeakObjectPtr<USceneComponent> Base;
	FVector Bottom = FVector::ZeroVector;
	FVector Top = FVector::ZeroVector;
	FVector TopExit = FVector::ZeroVector;
	float FacingYaw = 0.f;
	float RungStep = 30.f;
};

UCLASS()
class SALTBOUND_API ASaltPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ASaltPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UCameraComponent* GetCamera() const { return Camera; }

	// Interaction -------------------------------------------------------------
	USaltInteractableComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }
	/** Prompt for the HUD (empty when there is nothing to do). */
	FText GetPromptText() const;
	/** The controls in use for the HUD (D58: shown in the key-scrap prompt): groups of keys and what they do (at a station,
	 *  on a ladder), and a status line (the boat's). False when there are none. */
	bool GetControlsHint(TArray<FSaltControlHint>& OutControls, FText& OutStatus) const;

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
	/** Programmatic release of the interact key for automated tests (ends a hold). */
	void DebugInteractRelease() { HandleInteractReleased(); }
	/** D80: the left mouse button pressed (true) or let go, for automated tests. */
	void DebugUse(bool bDown) { if (bDown) { HandleUseStarted(); } else { HandleUseCompleted(); } }
	/** D80: the focused object is used with what is in hand (the prompt shows the left mouse button, not E). */
	bool IsPromptForUse() const;

	// Hold interactions (D55) ----------------------------------------------------
	/** 0..1 while a hold action fills (and drains after letting go early). */
	float GetHoldProgress() const { return HoldProgress; }
	/** The interactable being held, or null. */
	USaltInteractableComponent* GetHoldTarget() const { return HoldTarget.Get(); }
	/** Seconds since a hold action fired (the prompt hides for a moment then). */
	float GetTimeSinceHoldFired() const { return TimeSinceHoldFired; }
	/** Programmatic jump press/release for automated tests. */
	void DebugJump(bool bPressed) { bPressed ? HandleJumpStarted() : HandleJumpCompleted(); }
	/** Programmatic dive press/release for automated tests. */
	void DebugDive(bool bPressed) { bPressed ? HandleDiveStarted() : HandleDiveCompleted(); }

	// Tunables ----------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float WalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", Units = "cm/s"))
	float SwimSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (ClampMin = "0", Units = "cm/s"))
	float SwimSprintSpeed = 460.f;

	/** True while swimming in the sea (surface or under water). */
	bool IsSwimming() const;

	/** Scripted climb along a path (capsule centre points in Base's space, cm, so it follows a moving boat). Stepped
	 *  segments go rung by rung with a dip of the head on each pull; the others glide. bEndInWater: the climb ends
	 *  in the sea (down a ladder), otherwise standing on the top. */
	void StartScriptedClimb(USceneComponent* Base, const TArray<FVector>& LocalPoints, const TArray<bool>& Stepped, bool bEndInWater, float GlideTime = 0.f);
	bool IsClimbing() const { return bClimbing; }

	/** Takes hold of a ladder (D54): from the deck at its top, from the water where the body is. W and S climb rung by
	 *  rung, E lets go (into the sea); at the bottom you are in the sea, at the top you step on board. */
	void MountLadder(const FSaltLadderLine& Line, bool bFromTop);
	bool IsOnLadder() const { return bOnLadder; }
	/** 0 at the bottom .. 1 at the top. */
	float GetLadderAlong() const { return LadderAlong; }
	/** Lets go of the ladder: falls (into the sea), pushed a little off the hull. */
	void LetGoOfLadder();

	/** The satchel (D50). */
	USaltInventoryComponent* GetInventory() const { return Inventory; }

	/** The tool in hand (D55): Compass, Spyglass or Lantern; None is empty hands. */
	FName GetHeldTool() const { return HeldTool; }
	void SetHeldTool(FName Tool);
	/** Tools the player has (the compass, the spyglass and the lantern from the start, owner). */
	bool IsToolOwned(FName Tool) const { return OwnedTools.Contains(Tool); }
	/** D73: the tools owned (saved with the game). */
	TArray<FName> GetOwnedToolList() const { return OwnedTools.Array(); }
	void SetOwnedTools(const TArray<FName>& Tools) { OwnedTools = TSet<FName>(Tools); if (!HeldTool.IsNone() && !OwnedTools.Contains(HeldTool)) { SetHeldTool(NAME_None); } }
	void SetToolOwned(FName Tool, bool bOwned);
	/** FH1 (D77): waking on the beach: out of black, the view rises from the sand over Seconds (lying, sitting up, standing);
	 *  walking and looking come back at the end. */
	void StartWakeUp(float Seconds);
	bool IsWakingUp() const { return WakeSeconds > 0.f; }
	/** Shows the held tool (compass, spyglass view, lantern). */
	class USaltHeldToolsComponent* GetHeldTools() const { return HeldTools; }
	/** D80: the arms that hold them. */
	class USaltArmsComponent* GetArms() const { return Arms; }

	/** Applies the look settings (field of view, mouse sensitivity, invert look) from USaltGameUserSettings. */
	void ApplyUserSettings();
	/** Forgets held movement keys (the pause menu takes the input away while they are down). */
	void ResetHeldInput();
	float GetCapsuleHalfHeightForClimb() const;

	/** Height of one climbing step (ladder rung) and the time it takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "cm"))
	float ClimbStepHeight = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "s"))
	float ClimbStepTime = 0.34f;
	/** Climbing speed on a ladder, the same up and down, eased in and out (D59: smooth, no rung-by-rung steps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "cm/s"))
	float LadderSpeed = 190.f;
	/** D59: the move on to a ladder (cm/s) and over its top on to the deck (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "cm/s"))
	float LadderMountSpeed = 320.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Swimming", meta = (Units = "s"))
	float LadderTopTime = 0.35f;

	/** Camera height above the feet (the capsule's height and the eyes come from the stats table, SaltStats.h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltbound|Camera", meta = (ClampMin = "100", ClampMax = "180", Units = "cm"))
	float EyeHeight = 150.f;   // D79: 1.5 m (owner: "still to small"; D55 had 1.2 m)

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

	/** D56: how far the knees take up a heaving deck (cm the eyes are below (+) or above (-) where they would be): a share
	 *  of the swell about its recent level (D58: 20 %), softly at most 24 cm. */
	float GetDeckEyeOffset() const { return 24.f * FMath::Tanh(DeckEyeRate * (DeckEyeTarget - DeckEyeZ) / 24.f); }
	/** D58: the view's tilt with the deck (degrees from level), and how far a sudden pop of the body against the deck is
	 *  still being smoothed out of the view (cm). */
	float GetViewTiltDeg() const;
	float GetEyeStepOffset() const { return EyeStepOffset; }

	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;

	// A close look (D58: the chart on the mast) -------------------------------------
	/** The eyes glide to a view fixed in Base's space (cm; X looks ahead) in BlendTime, smoothly, and stay there while the
	 *  base moves; the body stays where it stands. Held tools are lowered meanwhile. */
	void FocusView(USceneComponent* Base, const FTransform& ViewInBase, float BlendTime);
	/** Back to the eyes in BlendTime. */
	void ReleaseView(float BlendTime);
	bool IsViewFocused() const { return bViewFocus; }
	/** 0 at the eyes .. 1 at the focused view (eased as the camera moves). */
	float GetViewFocusAlpha() const { return ViewFocusAlpha; }
	/** The scene component the held tools hang from: it follows the view as rendered (tilt and knees included). */
	USceneComponent* GetViewAnchor() const { return ViewAnchor; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USaltInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USaltHeldToolsComponent> HeldTools;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USaltArmsComponent> Arms;

	/** Follows the view as rendered (set in CalcCamera); the held tools hang from it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ViewAnchor;

private:
	float WakeT = 0.f;
	float WakeSeconds = 0.f;
	FTimerHandle WakeTimer;
	/** The deck the player is on, at its controls or climbing (it moves the view), or null. */
	const USceneComponent* GetDeck() const;

	/** The eyes on a moving deck: its level over a few seconds (world Z, cm), the share of the swell taken up, the last
	 *  eye height before that. */
	float DeckEyeZ = 0.f;
	float DeckEyeRate = 0.f;
	float DeckEyeTarget = 0.f;
	bool bDeckEyes = false;

	/** D58: the view takes a share of the deck's tilt (its up, smoothed) and smooths sudden pops of the body against the
	 *  deck (an edge under the feet as the deck heaves): the eye's height in the deck's frame as last shown. */
	FVector ViewUp = FVector::UpVector;
	float TiltShare = 0.f;
	TWeakObjectPtr<const USceneComponent> EyeDeck;
	float EyeStepOffset = 0.f;
	float EyeStepRate = 0.f;
	float LastEyeDeckZ = 0.f;
	/** In the air for real (a jump or a fall, not a moment's slip off an edge): the view follows it as it is. */
	bool bEyeAirborne = false;

	/** D58: a close look (FocusView): the view in its base's space, the blend's progress and its time. */
	TWeakObjectPtr<USceneComponent> FocusBase;
	FTransform FocusInBase = FTransform::Identity;
	bool bViewFocus = false;
	float ViewFocusLinear = 0.f;
	float ViewFocusAlpha = 0.f;
	float ViewFocusBlend = 0.4f;

	FName HeldTool;
	TSet<FName> OwnedTools;

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
	void HandleInteractReleased();
	void HandleUseStarted();
	void HandleUseCompleted();
	void TickHold(float DeltaSeconds);
	void TickClimb(float DeltaSeconds);
	void TickLadder(float DeltaSeconds, float Input);
	/** Off the bottom of the ladder into the sea. */
	void LeaveLadderIntoWater();
	FVector LadderPoint(float Along) const { return FMath::Lerp(Ladder.Bottom, Ladder.Top, Along); }

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

	TWeakObjectPtr<USaltInteractableComponent> HoldTarget;
	bool bInteractHeld = false;
	/** D80: the hold was started by the left mouse button (E's release does not end it). */
	bool bHoldByUse = false;
	float HoldProgress = 0.f;
	float TimeSinceHoldFired = 100.f;

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

	/** On a ladder (D54): where along it (0 bottom .. 1 top), and the short glide from where the hold was taken. */
	bool bOnLadder = false;
	FSaltLadderLine Ladder;
	float LadderAlong = 0.f;
	float LadderTime = 0.f;
	float LadderGlide = 1.f;
	float LadderGlideTime = 0.3f;
	/** Climbing speed along the ladder now (cm/s, + up), eased towards the keys' wish. */
	float LadderVelocity = 0.f;
	/** A scripted climb's glide time per segment (s; <= 0: from the distance). */
	float ClimbGlideTime = 0.f;
	FVector LadderGlideFrom = FVector::ZeroVector;
};

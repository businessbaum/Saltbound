// Saltbound - first-person player character.

#include "Player/SaltPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltGameUserSettings.h"
#include "Core/SaltStats.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Core/SaltTypes.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputActionValue.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Interaction/SaltStationComponent.h"
#include "Player/SaltCharacterMovementComponent.h"
#include "Player/SaltHeldTools.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

namespace
{
	TAutoConsoleVariable<float> CVarMouseSensitivity(
		TEXT("sb.MouseSensitivity"), 1.0f,
		TEXT("Multiplier applied on top of the character's MouseSensitivity."),
		ECVF_Default);

	/** D56: the knees take part of a heaving deck's swell (ASaltPlayerCharacter::CalcCamera); 0 turns it off (A/B). */
	TAutoConsoleVariable<int32> CVarDeckKnees(TEXT("sb.Camera.DeckKnees"), 1,
		TEXT("1: on a heaving deck the eyes take only part of the swell (default); 0: they ride the deck rigidly."));

	/** D58 (owner: "feel the rocking"): the share of the deck's roll and pitch the view takes. */
	TAutoConsoleVariable<float> CVarDeckTilt(TEXT("sb.Camera.DeckTilt"), 0.3f,
		TEXT("Share of a deck's roll and pitch the view takes (0 = always level; default 0.3)."));

	/** D58: sudden pops of the body against the deck (an edge under the feet as the deck heaves) are smoothed out of the
	 *  view; 0 turns it off (A/B). */
	TAutoConsoleVariable<int32> CVarEyeSteps(TEXT("sb.Camera.StepSmooth"), 1,
		TEXT("1: smooth sudden vertical pops of the body against the deck out of the view (default); 0: off."));

	/** D58: the knees' share of the swell (D56: 0.45; the owner wants to feel the ship move, so less). */
	constexpr float KneeShare = 0.2f;
	/** D58: the time constant of the spring that eases a pop against the deck out of the view (s; settled in ~0.3 s). */
	constexpr float EyeStepTime = 0.07f;

	/** Smoother step (zero first and second derivative at both ends). */
	float SmootherStep(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * T * (T * (T * 6.f - 15.f) + 10.f);
	}

	TAutoConsoleVariable<bool> CVarShowInteractionVolumes(
		TEXT("sb.Dev.ShowInteractionVolumes"), false,
		TEXT("Draw interaction focus probe hits."),
		ECVF_Cheat);

	// 1.4 m and slimmer (owner, D54: too big for the boat; earlier 1.76 m, 1.6 m and 1.5 m with a 68 cm wide capsule).
	constexpr float CapsuleRadius = 26.f;
	constexpr float CapsuleHalfHeight = 70.f;
}

ASaltPlayerCharacter::ASaltPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USaltCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	// The tools the player starts with (D55, owner).
	OwnedTools = { TEXT("Compass"), TEXT("Spyglass"), TEXT("Lantern") };

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Inventory = CreateDefaultSubobject<USaltInventoryComponent>(TEXT("Inventory"));
	HeldTools = CreateDefaultSubobject<USaltHeldToolsComponent>(TEXT("HeldTools"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - CapsuleHalfHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(FieldOfView);
	ViewAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ViewAnchor"));
	ViewAnchor->SetupAttachment(Camera);

	// No visible body for now.
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// D59: the player's numbers come from the stats table (Core/SaltStats, Docs/Stats.md). Owner: "the movement doesn't
	// feel responsive and clean": up to speed and to a stop in about a tenth of a second (was 0.17 s and more), a quicker
	// jump of the same height (stronger gravity, faster take-off), a little more steering in the air.
	const FSaltPlayerStats& Stats = SaltStats::Player();
	WalkSpeed = Stats.WalkSpeed;
	SprintSpeed = Stats.SprintSpeed;
	SwimSpeed = Stats.SwimSpeed;
	SwimSprintSpeed = Stats.SwimSprintSpeed;
	EyeHeight = Stats.EyeHeight;
	InteractionRange = Stats.InteractionRange;
	LadderSpeed = Stats.LadderSpeed;
	LadderMountSpeed = Stats.LadderMountSpeed;
	LadderTopTime = Stats.LadderTopTime;
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - CapsuleHalfHeight));
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = Stats.Acceleration;
	Move->BrakingDecelerationWalking = Stats.Braking;
	Move->GroundFriction = Stats.GroundFriction;
	Move->JumpZVelocity = Stats.JumpSpeed;
	Move->GravityScale = Stats.GravityScale;
	Move->AirControl = Stats.AirControl;
	Move->MaxStepHeight = Stats.MaxStepHeight;
	Move->SetWalkableFloorAngle(50.f);
	Move->bOrientRotationToMovement = false;
	Move->GetNavAgentPropertiesRef().bCanCrouch = false;
	// Moving decks: inherit base motion, and keep its velocity when jumping off.
	Move->bIgnoreBaseRotation = false;
	Move->bImpartBaseVelocityX = true;
	Move->bImpartBaseVelocityY = true;
	Move->bImpartBaseVelocityZ = true;
	Move->bImpartBaseAngularVelocity = true;
	// Jumping on the deck keeps the player in the deck's frame (lands where they jumped); leaving the
	// deck (no floor of the same object below) releases the base and imparts its velocity.
	Move->bStayBasedInAir = true;
	Move->StayBasedInAirHeight = 300.f;
	// D58 (owner: the camera "teleporting up and down really fast" by the edge of something on a heaving deck): next to a
	// low step (a chest, a rail's foot) the capsule's round bottom rode up and down the step's edge as the deck rolled under
	// it, 15-25 cm several times a second (sb.Dev.EdgeTest). A flat base stands on the step or beside it.
	Move->bUseFlatBaseForFloorChecks = true;
}

void ASaltPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	Camera->SetFieldOfView(FieldOfView);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ASaltPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyUserSettings();
}

void ASaltPlayerCharacter::ApplyUserSettings()
{
	if (const USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		FieldOfView = S->FieldOfView;
		MouseSensitivity = S->MouseSensitivity;
		bInvertLookY = S->bInvertLookY;
		Camera->SetFieldOfView(FieldOfView);
	}
}

void ASaltPlayerCharacter::ResetHeldInput()
{
	MoveInput = FVector2D::ZeroVector;
	bSprinting = false;
	bSwimUpHeld = false;
	bSwimDownHeld = false;
	StopJumping();
}

void ASaltPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetController());
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !Input)
	{
		UE_LOG(LogSaltbound, Error, TEXT("SaltPlayerCharacter requires ASaltPlayerController and Enhanced Input."));
		return;
	}

	const USaltInputConfig* Cfg = PC->GetInputConfig();
	Input->BindAction(Cfg->Move, ETriggerEvent::Triggered, this, &ASaltPlayerCharacter::HandleMove);
	Input->BindAction(Cfg->Move, ETriggerEvent::Completed, this, &ASaltPlayerCharacter::HandleMoveCompleted);
	Input->BindAction(Cfg->LookMouse, ETriggerEvent::Triggered, this, &ASaltPlayerCharacter::HandleLookMouse);
	Input->BindAction(Cfg->LookGamepad, ETriggerEvent::Triggered, this, &ASaltPlayerCharacter::HandleLookGamepad);
	Input->BindAction(Cfg->Jump, ETriggerEvent::Started, this, &ASaltPlayerCharacter::HandleJumpStarted);
	Input->BindAction(Cfg->Jump, ETriggerEvent::Completed, this, &ASaltPlayerCharacter::HandleJumpCompleted);
	Input->BindAction(Cfg->Sprint, ETriggerEvent::Started, this, &ASaltPlayerCharacter::HandleSprintStarted);
	Input->BindAction(Cfg->Sprint, ETriggerEvent::Completed, this, &ASaltPlayerCharacter::HandleSprintCompleted);
	Input->BindAction(Cfg->Dive, ETriggerEvent::Started, this, &ASaltPlayerCharacter::HandleDiveStarted);
	Input->BindAction(Cfg->Dive, ETriggerEvent::Completed, this, &ASaltPlayerCharacter::HandleDiveCompleted);
	Input->BindAction(Cfg->Interact, ETriggerEvent::Started, this, &ASaltPlayerCharacter::HandleInteract);
	Input->BindAction(Cfg->Interact, ETriggerEvent::Completed, this, &ASaltPlayerCharacter::HandleInteractReleased);
}

void ASaltPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector2D EffectiveMove = DebugMoveInput.IsNearlyZero() ? MoveInput : DebugMoveInput;
	if (bOnLadder)
	{
		TickLadder(DeltaSeconds, EffectiveMove.Y);
	}
	else if (bClimbing)
	{
		TickClimb(DeltaSeconds);
	}
	else if (ActiveStation)
	{
		ActiveStation->SetControlInput(EffectiveMove);
		TickStation(DeltaSeconds);
	}
	else
	{
		if (!DebugMoveInput.IsNearlyZero())
		{
			ApplyMoveInput(DebugMoveInput);
		}
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
		Move->MaxSwimSpeed = bSprinting ? SwimSprintSpeed : SwimSpeed;
		if (IsSwimming())
		{
			// Space rises, Dive sinks; under water the swimmer hovers, at the surface floats. D59 (owner: "remove the climbing
			// system for objects when you run against them"): no more climbing out on to any ledge; out of the sea by the
			// beach or up the boat's ladders.
			AddMovementInput(FVector::UpVector, (bSwimUpHeld ? 1.f : 0.f) - (bSwimDownHeld ? 1.f : 0.f));
		}
	}

	// D58: a close look glides there and back smoothly (zero speed at both ends).
	ViewFocusLinear = FMath::Clamp(ViewFocusLinear + (bViewFocus ? 1.f : -1.f) * DeltaSeconds / ViewFocusBlend, 0.f, 1.f);
	ViewFocusAlpha = SmootherStep(ViewFocusLinear);
	if (!bViewFocus && ViewFocusLinear <= 0.f)
	{
		FocusBase = nullptr;
	}

	UpdateInteractionFocus();
	TickHold(DeltaSeconds);
}

// --- Input -------------------------------------------------------------------

void ASaltPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
	if (!ActiveStation && !bOnLadder)
	{
		ApplyMoveInput(MoveInput);
	}
}

void ASaltPlayerCharacter::HandleMoveCompleted(const FInputActionValue& Value)
{
	MoveInput = FVector2D::ZeroVector;
}

void ASaltPlayerCharacter::ApplyMoveInput(const FVector2D& Input)
{
	if (!Controller)
	{
		return;
	}
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix M(YawRot);
	// Swimming follows the view, including its pitch (look down and swim forward to dive).
	const FVector Forward = IsSwimming() ? Controller->GetControlRotation().Vector() : M.GetUnitAxis(EAxis::X);
	AddMovementInput(Forward, Input.Y);
	AddMovementInput(M.GetUnitAxis(EAxis::Y), Input.X);
}

bool ASaltPlayerCharacter::IsSwimming() const
{
	return GetCharacterMovement() && GetCharacterMovement()->IsSwimming();
}

const USceneComponent* ASaltPlayerCharacter::GetDeck() const
{
	if (IsSwimming())
	{
		return nullptr;
	}
	const USceneComponent* Deck = ActiveStation ? static_cast<const USceneComponent*>(ActiveStation.Get())
		: bOnLadder ? Ladder.Base.Get() : bClimbing ? ClimbBase.Get() : Cast<UPrimitiveComponent>(GetMovementBaseObject());
	return Deck && Deck->Mobility == EComponentMobility::Movable ? Deck : nullptr;
}

float ASaltPlayerCharacter::GetViewTiltDeg() const
{
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)ViewUp.Z, -1.f, 1.f)));
}

void ASaltPlayerCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	const USceneComponent* Deck = GetDeck();

	// D58: a sudden pop of the body against the deck (faster than 1.5 m/s: a step up or down, the body pushed onto a rail's
	// foot as the deck rolls) is taken out of the view and eased back in about 0.3 s by a critically damped spring (no jump,
	// no overshoot); slower changes (a ramp) pass as they are. Only while standing or walking on the deck (and in the first
	// moment of a slip): a jump, a real fall, a ladder and a climb move the view as they are.
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bGround = Move && Move->IsMovingOnGround();
	bEyeAirborne = !bGround && Move && (bEyeAirborne || FMath::Abs(Move->Velocity.Z) >= 120.f);
	const bool bOnFoot = !ActiveStation && !bOnLadder && !bClimbing && Move && (bGround || (Move->IsFalling() && !bEyeAirborne));
	if (Deck && Deck == EyeDeck.Get() && DeltaTime > 0.f)
	{
		const float EyeDeckZ = (float)Deck->GetComponentTransform().InverseTransformPosition(OutResult.Location).Z;
		const float Step = EyeDeckZ - LastEyeDeckZ;
		if (bOnFoot && FMath::Abs(Step) > 150.f * DeltaTime && FMath::Abs(Step) < 60.f && CVarEyeSteps.GetValueOnGameThread() != 0)
		{
			EyeStepOffset -= Step;
		}
		const float W = 1.f / EyeStepTime;
		const float Decay = FMath::Exp(-W * DeltaTime);
		const float Push = (EyeStepRate + W * EyeStepOffset) * DeltaTime;
		EyeStepOffset = (EyeStepOffset + Push) * Decay;
		EyeStepRate = (EyeStepRate - W * Push) * Decay;
		LastEyeDeckZ = EyeDeckZ;
	}
	else
	{
		EyeDeck = Deck;
		EyeStepOffset = 0.f;
		EyeStepRate = 0.f;
		LastEyeDeckZ = Deck ? (float)Deck->GetComponentTransform().InverseTransformPosition(OutResult.Location).Z : 0.f;
	}
	OutResult.Location.Z += EyeStepOffset;

	// D56: on a deck in big waves (owner: "the camera is bugging up and down a lot") the eyes ride its average level but
	// not all of each swell, as a sailor's knees take part of it: the level over the last few seconds is followed fully,
	// of the motion about it the knees take a share (D58: 20 %, the owner wants to feel the ship), softly at most 24 cm.
	// Smooth everywhere: no limits that kink the view, no lag at a steady rise. Off the deck it fades out; a teleport
	// starts it afresh.
	const float Target = (float)OutResult.Location.Z;
	const bool bDeck = Deck && CVarDeckKnees.GetValueOnGameThread() != 0;
	if (!bDeckEyes || FMath::Abs(Target - DeckEyeTarget) > 300.f || DeltaTime <= 0.f)
	{
		DeckEyeZ = Target;
		DeckEyeRate = 0.f;
		bDeckEyes = true;
	}
	else
	{
		// DeckEyeZ: the deck's level over the last ~2.5 s; DeckEyeRate: how much of the swell the knees take.
		DeckEyeZ += (Target - DeckEyeZ) * (1.f - FMath::Exp(-DeltaTime / 2.5f));
		DeckEyeRate = FMath::FInterpConstantTo(DeckEyeRate, bDeck ? KneeShare : 0.f, DeltaTime, 1.f / 0.5f);
	}
	DeckEyeTarget = Target;
	OutResult.Location.Z = Target - GetDeckEyeOffset();

	// D58 (owner: "my character seems to be not influenced by the moving ship"): the view leans with the deck, a share of
	// its roll and pitch (as the body standing on it would), eased so a slam does not jolt it. Level off the deck.
	// The boat's up, not the floor part's (the gangway is a ramp).
	const AActor* Vessel = Deck ? Deck->GetOwner() : nullptr;
	const FVector DeckUp = Vessel ? Vessel->GetActorUpVector() : Deck ? Deck->GetUpVector() : FVector::UpVector;
	TiltShare = FMath::FInterpConstantTo(TiltShare, Deck ? FMath::Clamp(CVarDeckTilt.GetValueOnGameThread(), 0.f, 1.f) : 0.f, DeltaTime, 1.f / 0.6f);
	const FVector WantUp = FMath::Lerp(FVector::UpVector, DeckUp, TiltShare).GetSafeNormal();
	ViewUp = DeltaTime > 0.f ? FMath::Lerp(ViewUp, WantUp, 1.f - FMath::Exp(-DeltaTime / 0.08f)).GetSafeNormal() : WantUp;
	OutResult.Rotation = (FQuat::FindBetweenNormals(FVector::UpVector, ViewUp) * OutResult.Rotation.Quaternion()).Rotator();

	// The held tools hang from the view as it is seen.
	if (ViewAnchor)
	{
		ViewAnchor->SetWorldLocationAndRotation(OutResult.Location, OutResult.Rotation);
	}

	// D58: a close look glides from the eyes to the focused view (the chart on the mast) and back.
	if (ViewFocusAlpha > 0.f)
	{
		if (const USceneComponent* Base = FocusBase.Get())
		{
			const FTransform Focus = FocusInBase * Base->GetComponentTransform();
			OutResult.Location = FMath::Lerp(OutResult.Location, Focus.GetLocation(), (double)ViewFocusAlpha);
			OutResult.Rotation = FQuat::Slerp(OutResult.Rotation.Quaternion(), Focus.GetRotation(), ViewFocusAlpha).Rotator();
		}
	}
}

void ASaltPlayerCharacter::FocusView(USceneComponent* Base, const FTransform& ViewInBase, float BlendTime)
{
	FocusBase = Base;
	FocusInBase = ViewInBase;
	ViewFocusBlend = FMath::Max(BlendTime, 0.01f);
	bViewFocus = Base != nullptr;
	FocusedInteractable = nullptr;
	if (HeldTools)
	{
		HeldTools->SetSuppressed(bViewFocus);
	}
}

void ASaltPlayerCharacter::ReleaseView(float BlendTime)
{
	bViewFocus = false;
	ViewFocusBlend = FMath::Max(BlendTime, 0.01f);
	if (HeldTools)
	{
		HeldTools->SetSuppressed(false);
	}
}

void ASaltPlayerCharacter::SetHeldTool(FName Tool)
{
	if (Tool != HeldTool)
	{
		HeldTool = Tool;
		UE_LOG(LogSaltbound, Display, TEXT("SB_TOOL held=%s"), *Tool.ToString());
		if (HeldTools)
		{
			HeldTools->SetTool(Tool);
		}
	}
}

void ASaltPlayerCharacter::SetToolOwned(FName Tool, bool bOwned)
{
	if (bOwned)
	{
		OwnedTools.Add(Tool);
	}
	else
	{
		OwnedTools.Remove(Tool);
		if (HeldTool == Tool)
		{
			SetHeldTool(NAME_None);
		}
	}
}

void ASaltPlayerCharacter::HandleLookMouse(const FInputActionValue& Value)
{
	// While the tool wheel is open, the mouse aims at a tool instead of turning (D55).
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(Controller); PC && PC->IsToolWheelOpen())
	{
		PC->AimToolWheel(Value.Get<FVector2D>(), false);
		return;
	}
	// Through the spyglass the view turns slower, as narrow as it is.
	const float Zoom = HeldTools ? HeldTools->GetLookScale() : 1.f;
	const FVector2D Delta = Value.Get<FVector2D>() * (MouseSensitivity * CVarMouseSensitivity.GetValueOnGameThread() * Zoom);
	ApplyLook(Delta.X, bInvertLookY ? -Delta.Y : Delta.Y);
}

void ASaltPlayerCharacter::HandleLookGamepad(const FInputActionValue& Value)
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(Controller); PC && PC->IsToolWheelOpen())
	{
		PC->AimToolWheel(Value.Get<FVector2D>(), true);
		return;
	}
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const FVector2D Delta = Value.Get<FVector2D>() * (GamepadLookRate * Dt * (HeldTools ? HeldTools->GetLookScale() : 1.f));
	ApplyLook(Delta.X, bInvertLookY ? -Delta.Y : Delta.Y);
}

void ASaltPlayerCharacter::ApplyLook(float YawDelta, float PitchDelta)
{
	if (ActiveStation)
	{
		StationLookYaw = FMath::Clamp(StationLookYaw + YawDelta, -ActiveStation->YawLimit, ActiveStation->YawLimit);
		StationLookPitch = FMath::Clamp(StationLookPitch + PitchDelta, ActiveStation->PitchMin, ActiveStation->PitchMax);
		return;
	}
	// Positive pitch input looks up (legacy input scales are disabled in DefaultInput.ini).
	AddControllerYawInput(YawDelta);
	AddControllerPitchInput(PitchDelta);
}

void ASaltPlayerCharacter::HandleJumpStarted()
{
	bSwimUpHeld = true;
	if (ActiveStation)
	{
		ActiveStation->TriggerAction(0);
	}
	else if (!IsSwimming() && !bOnLadder)
	{
		Jump();
	}
}

void ASaltPlayerCharacter::HandleJumpCompleted()
{
	bSwimUpHeld = false;
	StopJumping();
}

void ASaltPlayerCharacter::HandleDiveStarted()
{
	// B on a gamepad cancels the tool wheel (D55).
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(Controller); PC && PC->IsToolWheelOpen())
	{
		PC->CloseToolWheel(false);
		return;
	}
	bSwimDownHeld = true;
	if (ActiveStation)
	{
		ActiveStation->TriggerAction(1);
	}
}

void ASaltPlayerCharacter::HandleDiveCompleted()
{
	bSwimDownHeld = false;
}

float ASaltPlayerCharacter::GetCapsuleHalfHeightForClimb() const
{
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

float ASaltPlayerCharacter::SegmentDuration(int32 Segment) const
{
	if (!ClimbPoints.IsValidIndex(Segment + 1))
	{
		return 0.f;
	}
	const FVector D = ClimbPoints[Segment + 1] - ClimbPoints[Segment];
	if (ClimbStepped.IsValidIndex(Segment) && ClimbStepped[Segment])
	{
		return FMath::Max(1, FMath::RoundToInt(FMath::Abs(D.Z) / FMath::Max(ClimbStepHeight, 5.f))) * ClimbStepTime;
	}
	return ClimbGlideTime > 0.f ? ClimbGlideTime : FMath::Clamp((float)D.Size() / 140.f, 0.25f, 1.2f);
}

void ASaltPlayerCharacter::StartScriptedClimb(USceneComponent* Base, const TArray<FVector>& LocalPoints, const TArray<bool>& Stepped, bool bEndInWater,
	float GlideTime)
{
	ClimbGlideTime = GlideTime;
	USaltCharacterMovementComponent* Move = Cast<USaltCharacterMovementComponent>(GetCharacterMovement());
	if (!Move || LocalPoints.Num() < 2)
	{
		return;
	}
	if (ActiveStation)
	{
		LeaveStation();
	}
	ClimbBase = Base;
	ClimbPoints = LocalPoints;
	ClimbStepped = Stepped;
	ClimbStepped.SetNum(ClimbPoints.Num() - 1);
	ClimbSegment = 0;
	ClimbSegmentTime = 0.f;
	bClimbEndInWater = bEndInWater;
	bClimbing = true;
	float Total = 0.f;
	for (int32 I = 0; I + 1 < ClimbPoints.Num(); ++I)
	{
		Total += SegmentDuration(I);
	}
	// No swimming while climbing; after climbing down, the sea takes over again at once.
	Move->SuppressWater(Total + (bEndInWater ? 0.f : 0.4f));
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_None);
}

void ASaltPlayerCharacter::TickClimb(float DeltaSeconds)
{
	const FTransform BaseXf = ClimbBase.IsValid() ? ClimbBase->GetComponentTransform() : FTransform::Identity;
	const float RestZ = EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	float Bob = 0.f;
	ClimbSegmentTime += DeltaSeconds;
	float Duration = SegmentDuration(ClimbSegment);
	while (ClimbSegmentTime >= Duration && ClimbPoints.IsValidIndex(ClimbSegment + 2))
	{
		ClimbSegmentTime -= Duration;
		++ClimbSegment;
		Duration = SegmentDuration(ClimbSegment);
	}
	const float Alpha = Duration > 0.f ? FMath::Min(ClimbSegmentTime / Duration, 1.f) : 1.f;
	const FVector A = ClimbPoints[ClimbSegment];
	const FVector B = ClimbPoints[ClimbSegment + 1];
	FVector Local;
	if (ClimbStepped.IsValidIndex(ClimbSegment) && ClimbStepped[ClimbSegment])
	{
		// Rung by rung: each step eases in and out (pull, settle) and dips the head a little.
		const int32 Steps = FMath::Max(1, FMath::RoundToInt(FMath::Abs(B.Z - A.Z) / FMath::Max(ClimbStepHeight, 5.f)));
		const float P = Alpha * Steps;
		const int32 K = FMath::Min((int32)P, Steps - 1);
		const float Frac = P - K;
		const float Eased = (K + FMath::InterpEaseInOut(0.f, 1.f, FMath::Min(Frac, 1.f), 2.2f)) / Steps;
		Local = FMath::Lerp(A, B, Alpha >= 1.f ? 1.f : Eased);
		Bob = -4.f * FMath::Sin(PI * FMath::Min(Frac, 1.f)) * (Alpha >= 1.f ? 0.f : 1.f);
	}
	else
	{
		Local = FMath::Lerp(A, B, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f));
	}
	SetActorLocation(BaseXf.TransformPosition(Local), false, nullptr, ETeleportType::None);
	Camera->SetRelativeLocation(FVector(0.f, 0.f, RestZ + Bob));
	if (Alpha >= 1.f && !ClimbPoints.IsValidIndex(ClimbSegment + 2))
	{
		Camera->SetRelativeLocation(FVector(0.f, 0.f, RestZ));
		bClimbing = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		UE_LOG(LogSaltbound, Log, TEXT("Climb finished (%s)"), bClimbEndInWater ? TEXT("in the water") : TEXT("on top"));
	}
}

// --- Ladders (D54) ---------------------------------------------------------------

void ASaltPlayerCharacter::MountLadder(const FSaltLadderLine& Line, bool bFromTop)
{
	USaltCharacterMovementComponent* Move = Cast<USaltCharacterMovementComponent>(GetCharacterMovement());
	USceneComponent* Base = Line.Base.Get();
	if (!Move || !Base || bOnLadder || bClimbing)
	{
		return;
	}
	if (ActiveStation)
	{
		LeaveStation();
	}
	Ladder = Line;
	const FTransform BaseXf = Base->GetComponentTransform();
	LadderGlideFrom = BaseXf.InverseTransformPosition(GetActorLocation());
	if (bFromTop)
	{
		LadderAlong = 1.f;
	}
	else
	{
		// From the water: take hold at the body's height, on a rung.
		const float Length = FMath::Max((float)(Ladder.Top - Ladder.Bottom).Size(), 1.f);
		const float Along = FMath::Clamp((float)((LadderGlideFrom.Z - Ladder.Bottom.Z) / (Ladder.Top.Z - Ladder.Bottom.Z)), 0.f, 1.f);
		LadderAlong = FMath::Clamp(FMath::RoundToFloat(Along * Length / Ladder.RungStep) * Ladder.RungStep / Length, 0.f, 1.f);
	}
	// D59 (owner: "make the going to the ladder animation faster"): on to it at 3.2 m/s, eased, in 0.12 to 0.35 s.
	LadderGlideTime = FMath::Clamp((float)(LadderPoint(LadderAlong) - LadderGlideFrom).Size() / FMath::Max(LadderMountSpeed, 1.f), 0.12f, 0.35f);
	LadderGlide = 0.f;
	LadderVelocity = 0.f;
	LadderTime = 0.f;
	bOnLadder = true;
	Move->SuppressWater(0.5f);
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_None);
	// Face the rungs; the view stays free to look around.
	if (Controller)
	{
		const float Yaw = BaseXf.GetRotation().Rotator().Yaw + Ladder.FacingYaw;
		Controller->SetControlRotation(FRotator(FMath::Clamp(Controller->GetControlRotation().GetNormalized().Pitch, -30.f, 30.f), Yaw, 0.f));
	}
	UE_LOG(LogSaltbound, Log, TEXT("Took hold of the ladder (%s, along %.2f)"), bFromTop ? TEXT("from the top") : TEXT("from the water"), LadderAlong);
}

void ASaltPlayerCharacter::TickLadder(float DeltaSeconds, float Input)
{
	USaltCharacterMovementComponent* Move = Cast<USaltCharacterMovementComponent>(GetCharacterMovement());
	USceneComponent* Base = Ladder.Base.Get();
	if (!Move || !Base)
	{
		bOnLadder = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		return;
	}
	LadderTime += DeltaSeconds;
	// No swimming while holding on (the lower rungs are in the sea).
	Move->SuppressWater(0.2f);
	const FTransform BaseXf = Base->GetComponentTransform();
	const float RestZ = EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (LadderGlide < 1.f)
	{
		LadderGlide = FMath::Min(LadderGlide + DeltaSeconds / LadderGlideTime, 1.f);
		const FVector Local = FMath::Lerp(LadderGlideFrom, LadderPoint(LadderAlong), FMath::InterpEaseInOut(0.f, 1.f, LadderGlide, 2.f));
		SetActorLocation(BaseXf.TransformPosition(Local), false, nullptr, ETeleportType::None);
		Camera->SetRelativeLocation(FVector(0.f, 0.f, RestZ));
		return;
	}
	const float Length = FMath::Max((float)(Ladder.Top - Ladder.Bottom).Size(), 1.f);
	const float Dir = FMath::Abs(Input) > 0.2f ? FMath::Sign(Input) : 0.f;
	// D59 (owner: "make smooth upwards movement when climbing", "you can climb down way faster than up"): one smooth
	// glide, the same speed up and down, eased in and out (no rung-by-rung steps, no dip of the head); it stops where
	// the keys are let go.
	LadderVelocity = FMath::FInterpConstantTo(LadderVelocity, Dir * LadderSpeed, DeltaSeconds, LadderSpeed / 0.15f);
	LadderAlong += LadderVelocity * DeltaSeconds / Length;
	const float Bob = 0.f;
	if (LadderAlong >= 1.f && Dir > 0.f)
	{
		// The top: over on to the deck in one quick move.
		bOnLadder = false;
		Camera->SetRelativeLocation(FVector(0.f, 0.f, RestZ));
		StartScriptedClimb(Base, { Ladder.Top, Ladder.TopExit }, { false }, false, LadderTopTime);
		UE_LOG(LogSaltbound, Log, TEXT("Off the ladder on to the deck"));
		return;
	}
	if (LadderAlong <= 0.f && Dir < 0.f)
	{
		LeaveLadderIntoWater();
		return;
	}
	LadderAlong = FMath::Clamp(LadderAlong, 0.f, 1.f);
	SetActorLocation(BaseXf.TransformPosition(LadderPoint(LadderAlong)), false, nullptr, ETeleportType::None);
	Camera->SetRelativeLocation(FVector(0.f, 0.f, RestZ + Bob));
}

void ASaltPlayerCharacter::LeaveLadderIntoWater()
{
	bOnLadder = false;
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	if (USaltCharacterMovementComponent* Move = Cast<USaltCharacterMovementComponent>(GetCharacterMovement()))
	{
		Move->ClearWaterSuppression();
		Move->SetMovementMode(MOVE_Falling);
	}
	UE_LOG(LogSaltbound, Log, TEXT("Off the bottom of the ladder into the sea"));
}

void ASaltPlayerCharacter::LetGoOfLadder()
{
	if (!bOnLadder)
	{
		return;
	}
	bOnLadder = false;
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	if (USaltCharacterMovementComponent* Move = Cast<USaltCharacterMovementComponent>(GetCharacterMovement()))
	{
		Move->ClearWaterSuppression();
		Move->SetMovementMode(MOVE_Falling);
		// Push off the hull a little, so the body falls clear of the ladder.
		const FVector Facing = FRotator(0.f, (Ladder.Base.IsValid() ? Ladder.Base->GetComponentRotation().Yaw : 0.f) + Ladder.FacingYaw, 0.f).Vector();
		Move->Velocity = -Facing * 120.f;
	}
	UE_LOG(LogSaltbound, Log, TEXT("Let go of the ladder (along %.2f)"), LadderAlong);
}

void ASaltPlayerCharacter::HandleSprintStarted()
{
	bSprinting = true;
}

void ASaltPlayerCharacter::HandleSprintCompleted()
{
	bSprinting = false;
}

void ASaltPlayerCharacter::HandleInteract()
{
	if (bClimbing)
	{
		return;
	}
	if (bOnLadder)
	{
		// E lets go (not the press that may still be bouncing from taking hold).
		if (LadderTime > 0.25f)
		{
			LetGoOfLadder();
		}
		return;
	}
	if (ActiveStation)
	{
		// Ignore the press that may still be bouncing from entering; some stations hold the player for a while (the hoist).
		if (StationTime > 0.2f && ActiveStation->bLeaveAllowed)
		{
			LeaveStation();
		}
		return;
	}
	if (USaltInteractableComponent* Focus = FocusedInteractable.Get())
	{
		if (Focus->CanInteract(this))
		{
			if (Focus->IsHold())
			{
				// A hold: the ink fills while the key stays down (TickHold); the action runs when it is full.
				HoldTarget = Focus;
				bInteractHeld = true;
				return;
			}
			Focus->Interact(this);
		}
	}
}

void ASaltPlayerCharacter::HandleInteractReleased()
{
	bInteractHeld = false;
}

void ASaltPlayerCharacter::TickHold(float DeltaSeconds)
{
	TimeSinceHoldFired += DeltaSeconds;
	USaltInteractableComponent* Target = HoldTarget.Get();
	if (!Target)
	{
		HoldProgress = 0.f;
		return;
	}
	const bool bStillHolding = bInteractHeld && FocusedInteractable.Get() == Target && Target->IsHold() && Target->CanInteract(this);
	if (bStillHolding)
	{
		HoldProgress += DeltaSeconds / FMath::Max(Target->HoldTime, 0.05f);
		if (HoldProgress >= 1.f)
		{
			HoldProgress = 0.f;
			HoldTarget = nullptr;
			bInteractHeld = false;
			TimeSinceHoldFired = 0.f;
			Target->Interact(this);
		}
		return;
	}
	// Let go early (or looked away): the ink drains back in about 0.25 s.
	bInteractHeld = false;
	HoldProgress = FMath::Max(HoldProgress - DeltaSeconds / 0.25f, 0.f);
	if (HoldProgress <= 0.f)
	{
		HoldTarget = nullptr;
	}
}

// --- Interaction ---------------------------------------------------------------

void ASaltPlayerCharacter::UpdateInteractionFocus()
{
	USaltInteractableComponent* NewFocus = nullptr;
	UWorld* World = GetWorld();
	if (!ActiveStation && !bOnLadder && !bViewFocus && ViewFocusAlpha <= 0.f && Controller && World)
	{
		// Along the view as seen (the control rotation leaning with the deck, D58), so the dot is where the probe goes.
		const FVector Start = Camera->GetComponentLocation();
		const FVector Ahead = FQuat::FindBetweenNormals(FVector::UpVector, ViewUp).RotateVector(Controller->GetControlRotation().Vector());
		const FVector End = Start + Ahead * InteractionRange;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltInteractionProbe), false, this);

		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, SALT_TRACE_INTERACTION,
			FCollisionShape::MakeSphere(InteractionProbeRadius), Params))
		{
			USaltInteractableComponent* Candidate = Cast<USaltInteractableComponent>(Hit.GetComponent());
			if (Candidate && Candidate->CanInteract(this))
			{
				// Reject if solid geometry sits between the eye and the focus volume.
				FHitResult Block;
				const FVector Target = Hit.bStartPenetrating ? Start : Hit.ImpactPoint;
				const bool bBlocked = !Hit.bStartPenetrating &&
					World->LineTraceSingleByChannel(Block, Start, Target, ECC_Visibility, Params) &&
					Block.Distance < Hit.Distance - 2.f;
				if (!bBlocked)
				{
					NewFocus = Candidate;
				}
			}
			if (CVarShowInteractionVolumes.GetValueOnGameThread())
			{
				DrawDebugSphere(World, Hit.ImpactPoint, 4.f, 8, NewFocus ? FColor::Green : FColor::Red, false, 0.f);
			}
		}
	}
	FocusedInteractable = NewFocus;
}

FText ASaltPlayerCharacter::GetPromptText() const
{
	if (bOnLadder)
	{
		return NSLOCTEXT("Saltbound", "LadderLetGo", "Let go of the ladder");
	}
	if (ActiveStation)
	{
		return ActiveStation->LeavePromptText;
	}
	if (const USaltInteractableComponent* Focus = FocusedInteractable.Get())
	{
		return Focus->GetPromptText(this);
	}
	return FText::GetEmpty();
}

bool ASaltPlayerCharacter::GetControlsHint(TArray<FSaltControlHint>& OutControls, FText& OutStatus) const
{
	OutControls.Reset();
	OutStatus = FText::GetEmpty();
	if (bOnLadder)
	{
		OutControls.Add({ { TEXT("W"), TEXT("S") }, NSLOCTEXT("Saltbound", "LadderHint", "Climb up and down") });
	}
	else if (ActiveStation)
	{
		OutControls = ActiveStation->Controls;
		OutStatus = ActiveStation->ControlsHintText;
	}
	return OutControls.Num() > 0 || !OutStatus.IsEmpty();
}

// --- Stations ------------------------------------------------------------------

float ASaltPlayerCharacter::GetStationYaw() const
{
	return ActiveStation ? ActiveStation->GetComponentRotation().Yaw : 0.f;
}

void ASaltPlayerCharacter::EnterStation(USaltStationComponent* Station)
{
	if (!Station || ActiveStation)
	{
		return;
	}
	ActiveStation = Station;
	StationTime = 0.f;
	StationBlendAlpha = 0.f;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	StationEntryLocal = Station->GetComponentTransform().InverseTransformPosition(GetActorLocation() - FVector(0.f, 0.f, HalfHeight));

	const FRotator ControlRot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	StationLookYaw = FMath::Clamp(FRotator::NormalizeAxis(ControlRot.Yaw - GetStationYaw()), -Station->YawLimit, Station->YawLimit);
	StationLookPitch = FMath::Clamp(FRotator::NormalizeAxis(ControlRot.Pitch), Station->PitchMin, Station->PitchMax);

	// Follow the station after its owner (boat) has moved this frame.
	if (AActor* StationOwner = Station->GetOwner())
	{
		AddTickPrerequisiteActor(StationOwner);
	}
	FocusedInteractable = nullptr;
	Station->NotifyEntered(this);
	UE_LOG(LogSaltbound, Log, TEXT("Entered station %s on %s"), *Station->GetName(), *GetNameSafe(Station->GetOwner()));
}

void ASaltPlayerCharacter::LeaveStation()
{
	USaltStationComponent* Station = ActiveStation;
	if (!Station)
	{
		return;
	}
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Lift(0.f, 0.f, HalfHeight + 2.f);
	FVector Center = Station->GetExitLocation() + Lift;
	if (!IsCapsuleSpotFree(Center))
	{
		Center = Station->GetComponentTransform().TransformPosition(StationEntryLocal) + Lift;
	}

	if (AActor* StationOwner = Station->GetOwner())
	{
		RemoveTickPrerequisiteActor(StationOwner);
	}
	ActiveStation = nullptr;

	const float Yaw = Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
	SetActorLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Station->NotifyLeft(this);
	UE_LOG(LogSaltbound, Log, TEXT("Left station %s"), *Station->GetName());
}

void ASaltPlayerCharacter::TickStation(float DeltaSeconds)
{
	StationTime += DeltaSeconds;
	StationBlendAlpha = StationBlendTime > 0.f ? FMath::Min(1.f, StationBlendAlpha + DeltaSeconds / StationBlendTime) : 1.f;
	const float A = FMath::InterpEaseInOut(0.f, 1.f, StationBlendAlpha, 2.f);

	// Blend the feet position in station space so the motion stays glued to a moving deck.
	const FTransform StationXf = ActiveStation->GetComponentTransform();
	const FVector FeetLocal = FMath::Lerp(StationEntryLocal, ActiveStation->StandOffset, A);
	const FVector FeetWorld = StationXf.TransformPosition(FeetLocal);
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float ViewYaw = GetStationYaw() + StationLookYaw;
	SetActorLocationAndRotation(FeetWorld + FVector(0.f, 0.f, HalfHeight), FRotator(0.f, ViewYaw, 0.f), false, nullptr, ETeleportType::None);
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(StationLookPitch, ViewYaw, 0.f));
	}
}

bool ASaltPlayerCharacter::IsCapsuleSpotFree(const FVector& CapsuleCenter) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltStationExit), false, this);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 1.f, Capsule->GetScaledCapsuleHalfHeight() - 1.f);
	return !GetWorld()->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_Pawn, Shape, Params);
}

// --- Development -----------------------------------------------------------------

void ASaltPlayerCharacter::TeleportFeetTo(const FVector& FeetLocation, float Yaw)
{
	if (ActiveStation)
	{
		LeaveStation();
	}
	bClimbing = false;
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocationAndRotation(FeetLocation + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(0.f, Yaw, 0.f));
	}
}

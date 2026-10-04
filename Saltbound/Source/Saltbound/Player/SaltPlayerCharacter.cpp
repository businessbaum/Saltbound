// Saltbound - first-person player character.

#include "Player/SaltPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

namespace
{
	TAutoConsoleVariable<float> CVarMouseSensitivity(
		TEXT("sb.MouseSensitivity"), 1.0f,
		TEXT("Multiplier applied on top of the character's MouseSensitivity."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarShowInteractionVolumes(
		TEXT("sb.Dev.ShowInteractionVolumes"), false,
		TEXT("Draw interaction focus probe hits."),
		ECVF_Cheat);

	constexpr float CapsuleRadius = 34.f;
	constexpr float CapsuleHalfHeight = 75.f;   // 1.5 m: owner feedback (1.76 m, then 1.6 m felt too tall)
}

ASaltPlayerCharacter::ASaltPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USaltCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - CapsuleHalfHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(FieldOfView);

	// No visible body for now.
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = 1500.f;
	Move->BrakingDecelerationWalking = 1800.f;
	Move->GroundFriction = 8.f;
	Move->JumpZVelocity = 340.f;
	Move->AirControl = 0.25f;
	Move->MaxStepHeight = 25.f;
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
}

void ASaltPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	Camera->SetFieldOfView(FieldOfView);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
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
}

void ASaltPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector2D EffectiveMove = DebugMoveInput.IsNearlyZero() ? MoveInput : DebugMoveInput;
	if (bClimbing)
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
			// Space rises (or climbs out at a ledge), Dive sinks; buoyancy holds the head at the surface.
			if (bSwimUpHeld && TryClimbOutOfWater())
			{
				bSwimUpHeld = false;
			}
			else
			{
				AddMovementInput(FVector::UpVector, (bSwimUpHeld ? 1.f : 0.f) - (bSwimDownHeld ? 1.f : 0.f));
			}
		}
	}

	UpdateInteractionFocus();
}

// --- Input -------------------------------------------------------------------

void ASaltPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
	if (!ActiveStation)
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

void ASaltPlayerCharacter::HandleLookMouse(const FInputActionValue& Value)
{
	const FVector2D Delta = Value.Get<FVector2D>() * (MouseSensitivity * CVarMouseSensitivity.GetValueOnGameThread());
	ApplyLook(Delta.X, bInvertLookY ? -Delta.Y : Delta.Y);
}

void ASaltPlayerCharacter::HandleLookGamepad(const FInputActionValue& Value)
{
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const FVector2D Delta = Value.Get<FVector2D>() * (GamepadLookRate * Dt);
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
	else if (!IsSwimming())
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

bool ASaltPlayerCharacter::TryClimbOutOfWater()
{
	// A walkable top within reach just ahead (pier deck, boat side, flat rock), with room to stand. Piers are
	// open frames, so this probes for the top directly instead of requiring a wall in front.
	USaltCharacterMovementComponent* Move = Cast<USaltCharacterMovementComponent>(GetCharacterMovement());
	UWorld* World = GetWorld();
	if (!Move || !Move->IsSwimming() || !Controller || !World)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Half = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Center = GetActorLocation();
	const float Feet = Center.Z - Half;
	// Only at the surface (head out of the water).
	if (Move->GetWaterSurfaceZ() > Center.Z + Half - 15.f)
	{
		return false;
	}
	const FVector Forward = FRotationMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltClimbOut), false, this);
	const FVector Side(-Forward.Y, Forward.X, 0.f);
	for (const float Ahead : { Radius + 25.f, Radius + 50.f, Radius + 75.f })
	for (const float Lateral : { 0.f, -35.f, 35.f })
	{
		// Straight ahead first, then a little to either side (pier posts, rails).
		const FVector Probe = Center + Forward * Ahead + Side * Lateral;
		const FVector From(Probe.X, Probe.Y, Feet + ClimbOutReach + 40.f);
		FHitResult Top;
		if (!World->LineTraceSingleByChannel(Top, From, FVector(Probe.X, Probe.Y, Feet), ECC_Visibility, Params)
			|| Top.bStartPenetrating || Top.ImpactNormal.Z < 0.7f)
		{
			continue;
		}
		const float Rise = Top.ImpactPoint.Z - Feet;
		if (Rise > ClimbOutReach || Top.ImpactPoint.Z < Move->GetWaterSurfaceZ() + 20.f)
		{
			continue;
		}
		// Stand a little further in than the edge.
		const FVector End = FVector(Top.ImpactPoint.X, Top.ImpactPoint.Y, Top.ImpactPoint.Z + Half + 2.f) + Forward * 15.f;
		const FVector Raised(Center.X, Center.Y, End.Z);   // the climb rises here first
		if (!IsCapsuleSpotFree(End) || !IsCapsuleSpotFree(Raised))
		{
			continue;
		}
		USceneComponent* Base = Top.GetComponent();
		const FTransform BaseXf = Base ? Base->GetComponentTransform() : FTransform::Identity;
		ClimbBase = Base;
		ClimbStartLocal = BaseXf.InverseTransformPosition(Center);
		ClimbEndLocal = BaseXf.InverseTransformPosition(End);
		ClimbAlpha = 0.f;
		bClimbing = true;
		Move->SuppressWater(ClimbOutTime + 0.4f);
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_None);
		UE_LOG(LogSaltbound, Log, TEXT("Climbing out of the water onto %s (%.0f cm)"), *GetNameSafe(Top.GetActor()), Rise);
		return true;
	}
	return false;
}

void ASaltPlayerCharacter::TickClimb(float DeltaSeconds)
{
	ClimbAlpha = FMath::Min(1.f, ClimbAlpha + DeltaSeconds / FMath::Max(ClimbOutTime, 0.1f));
	const FTransform BaseXf = ClimbBase.IsValid() ? ClimbBase->GetComponentTransform() : FTransform::Identity;
	const FVector Start = BaseXf.TransformPosition(ClimbStartLocal);
	const FVector End = BaseXf.TransformPosition(ClimbEndLocal);
	// Up first (head and shoulders over the edge), then forward onto the ledge.
	const float Up = FMath::InterpEaseInOut(0.f, 1.f, FMath::Min(ClimbAlpha / 0.65f, 1.f), 2.f);
	const float Over = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((ClimbAlpha - 0.35f) / 0.65f, 0.f, 1.f), 2.f);
	const FVector P(FMath::Lerp(Start.X, End.X, Over), FMath::Lerp(Start.Y, End.Y, Over), FMath::Lerp(Start.Z, End.Z, Up));
	SetActorLocation(P, false, nullptr, ETeleportType::None);
	if (ClimbAlpha >= 1.f)
	{
		bClimbing = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
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
	if (ActiveStation)
	{
		// Ignore the press that may still be bouncing from entering.
		if (StationTime > 0.2f)
		{
			LeaveStation();
		}
		return;
	}
	if (USaltInteractableComponent* Focus = FocusedInteractable.Get())
	{
		if (Focus->CanInteract(this))
		{
			Focus->Interact(this);
		}
	}
}

// --- Interaction ---------------------------------------------------------------

void ASaltPlayerCharacter::UpdateInteractionFocus()
{
	USaltInteractableComponent* NewFocus = nullptr;
	UWorld* World = GetWorld();
	if (!ActiveStation && Controller && World)
	{
		const FVector Start = Camera->GetComponentLocation();
		const FVector End = Start + Controller->GetControlRotation().Vector() * InteractionRange;
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

FText ASaltPlayerCharacter::GetHintText() const
{
	return ActiveStation ? ActiveStation->ControlsHintText : FText::GetEmpty();
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
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocationAndRotation(FeetLocation + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(0.f, Yaw, 0.f));
	}
}

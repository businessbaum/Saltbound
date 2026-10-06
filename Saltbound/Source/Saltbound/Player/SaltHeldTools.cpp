// Saltbound - the tools in the player's hands (D55).

#include "Player/SaltHeldTools.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Props/SaltSwingComponent.h"

namespace
{
	const TCHAR* CompassMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_Compass.SM_Tool_Compass");
	const TCHAR* NeedleMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_CompassNeedle.SM_Tool_CompassNeedle");
	const TCHAR* LanternMeshPath = TEXT("/Game/Saltbound/Boat/SM_Boat_Lantern.SM_Boat_Lantern");
	const TCHAR* ShovelMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_Shovel.SM_Tool_Shovel");

	// Where the tools are held, in the camera's frame (cm: forward, right, up), and how far they sink when put away.
	const FVector CompassAt(30.f, 12.f, -13.f);
	const FRotator CompassTilt(55.f, -14.f, 0.f);   // the dial turned up towards the eye, its bow away
	const FVector LanternAt(58.f, -30.f, -16.f);  // out at arm's length, low on the left: it lights the way, not the view
	// D73: the shovel's grip low on the right, its blade down and ahead (the mesh runs along +X from the grip).
	const FVector ShovelAt(26.f, 24.f, -30.f);
	const FRotator ShovelTilt(-42.f, -12.f, 8.f);
	constexpr float SinkDepth = 30.f;
	constexpr float RaiseTime = 0.25f;
	constexpr float SpyglassTime = 0.35f;

	// The needle: a damped swing to north (period 0.9 s, damping ratio 0.25).
	constexpr float NeedlePeriod = 0.9f;
	constexpr float NeedleDamping = 0.25f;
}

USaltHeldToolsComponent::USaltHeldToolsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USaltHeldToolsComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureParts();
}

void USaltHeldToolsComponent::EnsureParts()
{
	AActor* Owner = GetOwner();
	if (Compass || !Owner)
	{
		return;
	}
	Camera = Owner->FindComponentByClass<UCameraComponent>();
	if (!Camera)
	{
		return;
	}
	auto MeshPart = [Owner](const TCHAR* Name, const TCHAR* Path, USceneComponent* Parent)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Owner, Name);
		M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Path));
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->SetupAttachment(Parent);
		M->RegisterComponent();
		return M;
	};
	// The hands hang from the view as it is seen (D58: it leans with the deck), else from the camera.
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(Owner);
	USceneComponent* View = Player && Player->GetViewAnchor() ? Player->GetViewAnchor() : Camera.Get();
	auto Hand = [Owner, View](const TCHAR* Name)
	{
		USceneComponent* H = NewObject<USceneComponent>(Owner, Name);
		H->SetupAttachment(View);
		H->RegisterComponent();
		return H;
	};
	// The compass, low on the right; its needle just above the dial (the dial's face is 0.15 cm up).
	CompassHand = Hand(TEXT("CompassHand"));
	CompassHand->SetRelativeRotation(CompassTilt);
	Compass = MeshPart(TEXT("HeldCompass"), CompassMeshPath, CompassHand);
	Needle = MeshPart(TEXT("HeldCompassNeedle"), NeedleMeshPath, Compass);
	Needle->SetRelativeLocation(FVector(0.f, 0.f, 0.2f));
	// The lantern hangs from the left hand: the hand follows the camera but keeps the world's up, so the lantern swings
	// under it as the player turns and walks.
	LanternHand = Hand(TEXT("LanternHand"));
	LanternHand->SetUsingAbsoluteRotation(true);
	LanternHand->SetWorldRotation(FRotator::ZeroRotator);
	LanternSwing = NewObject<USaltSwingComponent>(Owner, TEXT("HeldLanternSwing"));
	LanternSwing->Length = 18.f;
	LanternSwing->Damping = 0.08f;
	LanternSwing->MaxAngle = 35.f;
	LanternSwing->PushRadius = 0.f;
	LanternSwing->SetupAttachment(LanternHand);
	LanternSwing->RegisterComponent();
	Lantern = MeshPart(TEXT("HeldLantern"), LanternMeshPath, LanternSwing);
	const int32 GlassSlot = Lantern->GetMaterialIndex(TEXT("MI_LanternGlass"));
	LanternGlass = GlassSlot != INDEX_NONE ? Lantern->CreateAndSetMaterialInstanceDynamic(GlassSlot) : nullptr;
	// The flame's light, as the boat's lantern (2100 K).
	LanternLight = NewObject<UPointLightComponent>(Owner, TEXT("HeldLanternLight"));
	LanternLight->SetupAttachment(Lantern);
	LanternLight->SetRelativeLocation(FVector(0.f, 0.f, -21.5f));
	LanternLight->SetIntensityUnits(ELightUnits::Candelas);
	LanternLight->SetIntensity(28.f);
	LanternLight->SetUseTemperature(true);
	LanternLight->SetTemperature(2100.f);
	LanternLight->SetAttenuationRadius(900.f);
	LanternLight->SetSourceRadius(3.f);
	LanternLight->SetSoftSourceRadius(4.f);
	LanternLight->SetCastShadows(true);
	LanternLight->RegisterComponent();
	// D73: the shovel, held low on the right.
	ShovelHand = Hand(TEXT("ShovelHand"));
	ShovelHand->SetRelativeRotation(ShovelTilt);
	Shovel = MeshPart(TEXT("HeldShovel"), ShovelMeshPath, ShovelHand);
	CompassHand->SetVisibility(false, true);
	LanternHand->SetVisibility(false, true);
	ShovelHand->SetVisibility(false, true);
}

void USaltHeldToolsComponent::SetTool(FName InTool)
{
	Tool = InTool;
}

float USaltHeldToolsComponent::GetFieldOfView() const
{
	return FMath::Lerp(BaseFOV, SpyglassFOV, FMath::SmoothStep(0.f, 1.f, Spyglass));
}

float USaltHeldToolsComponent::GetLookScale() const
{
	const float Fov = GetFieldOfView();
	return FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)) / FMath::Max(FMath::Tan(FMath::DegreesToRadians(BaseFOV * 0.5f)), 0.01f);
}

FVector USaltHeldToolsComponent::GetNeedleNorth() const
{
	return Needle ? Needle->GetForwardVector().GetSafeNormal2D() : FVector::ZeroVector;
}

void USaltHeldToolsComponent::UpdateCompass(float DeltaTime)
{
	if (!Compass || !Needle)
	{
		return;
	}
	// Where north (+X) lies in the dial's plane; the needle swings there like a damped pendulum.
	const FVector North = Compass->GetComponentTransform().InverseTransformVectorNoScale(FVector::ForwardVector);
	const float Target = FMath::RadiansToDegrees(FMath::Atan2(North.Y, North.X));
	const float W = TWO_PI / NeedlePeriod;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime * 120.f), 1, 12);
	const float H = DeltaTime / Steps;
	for (int32 I = 0; I < Steps; ++I)
	{
		const float Diff = FMath::FindDeltaAngleDegrees(NeedleYaw, Target);
		NeedleRate += (W * W * Diff - 2.f * NeedleDamping * W * NeedleRate) * H;
		NeedleYaw = FRotator::NormalizeAxis(NeedleYaw + NeedleRate * H);
	}
	Needle->SetRelativeRotation(FRotator(0.f, NeedleYaw, 0.f));
}

void USaltHeldToolsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EnsureParts();
	if (!Camera || !Compass)
	{
		return;
	}
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwner()))
	{
		BaseFOV = Player->FieldOfView;
	}
	// The shown tool sinks out before the next one rises in (and while the eyes are elsewhere).
	const FName Wanted = bSuppressed ? FName() : Tool;
	if (Shown != Wanted)
	{
		Raise = FMath::Max(0.f, Raise - DeltaTime / RaiseTime);
		if (Raise <= 0.f)
		{
			Shown = Wanted;
			if (Shown == TEXT("Compass"))
			{
				// The needle starts where it lay (pointing along the case) and swings to north.
				NeedleYaw = 0.f;
				NeedleRate = 0.f;
			}
		}
	}
	else
	{
		Raise = FMath::Min(1.f, Raise + DeltaTime / RaiseTime);
	}
	const float Up = 1.f - FMath::Square(1.f - Raise);
	const bool bCompass = Shown == TEXT("Compass");
	const bool bLantern = Shown == TEXT("Lantern");
	CompassHand->SetVisibility(bCompass, true);
	LanternHand->SetVisibility(bLantern, true);
	CompassHand->SetRelativeLocation(CompassAt - FVector(0.f, 0.f, SinkDepth * (1.f - Up)));
	LanternHand->SetRelativeLocation(LanternAt - FVector(0.f, 0.f, SinkDepth * (1.f - Up)));
	LanternHand->SetWorldRotation(FRotator::ZeroRotator);
	// D73: the shovel; three strokes over a dig's hold (a thrust down and forward, back up).
	const bool bShovel = Shown == TEXT("Shovel");
	ShovelHand->SetVisibility(bShovel, true);
	float Stroke = 0.f;
	if (const ASaltPlayerCharacter* Holder = Cast<ASaltPlayerCharacter>(GetOwner()); bShovel && Holder && Holder->GetHoldTarget()
		&& Holder->GetHoldTarget()->ComponentHasTag(TEXT("Dig")))
	{
		Stroke = FMath::Square(FMath::Sin(PI * 3.f * Holder->GetHoldProgress()));
	}
	DigStroke = FMath::FInterpTo(DigStroke, Stroke, DeltaTime, 22.f);
	ShovelHand->SetRelativeLocation(ShovelAt + FVector(16.f, -4.f, -14.f) * DigStroke - FVector(0.f, 0.f, SinkDepth * (1.f - Up)));
	ShovelHand->SetRelativeRotation(ShovelTilt + FRotator(-22.f, 0.f, 0.f) * DigStroke);
	if (bCompass)
	{
		UpdateCompass(DeltaTime);
	}
	if (bLantern)
	{
		// The flame's flicker: a slow breath and a small fast flutter (as the boat's lantern).
		Flicker += DeltaTime;
		const float Factor = 1.f + 0.08f * (0.7f * FMath::PerlinNoise1D(Flicker * 0.9f + 5.f) + 0.3f * FMath::PerlinNoise1D(Flicker * 7.3f + 29.f));
		LanternLight->SetIntensity(28.f * Factor * Up);
		if (LanternGlass)
		{
			LanternGlass->SetScalarParameterValue(TEXT("GlowIntensity"), 80.f * Factor);
		}
	}
	// The spyglass narrows the view (in 0.35 s) and widens it again when put away.
	const bool bSpyglass = Wanted == TEXT("Spyglass");
	Spyglass = FMath::Clamp(Spyglass + (bSpyglass ? 1.f : -1.f) * DeltaTime / SpyglassTime, 0.f, 1.f);
	Camera->SetFieldOfView(GetFieldOfView());
}

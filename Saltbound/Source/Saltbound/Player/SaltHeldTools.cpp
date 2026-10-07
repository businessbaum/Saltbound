// Saltbound - the tools in the player's hands (D55; D80: in the hands, used with the left mouse button).

#include "Player/SaltHeldTools.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Player/SaltArmsComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Props/SaltSwingComponent.h"

namespace
{
	const TCHAR* CompassMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_Compass.SM_Tool_Compass");
	const TCHAR* NeedleMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_CompassNeedle.SM_Tool_CompassNeedle");
	const TCHAR* LanternMeshPath = TEXT("/Game/Saltbound/Boat/SM_Boat_Lantern.SM_Boat_Lantern");
	const TCHAR* ShovelMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_Shovel.SM_Tool_Shovel");
	const TCHAR* ShovelLoadMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_ShovelLoad.SM_Tool_ShovelLoad");
	const TCHAR* SpyglassMeshPath = TEXT("/Game/Saltbound/Props/Tools/SM_Tool_Spyglass.SM_Tool_Spyglass");
	const TCHAR* SpyglassDrawPaths[3] = {
		TEXT("/Game/Saltbound/Props/Tools/SM_Tool_SpyglassDraw1.SM_Tool_SpyglassDraw1"),
		TEXT("/Game/Saltbound/Props/Tools/SM_Tool_SpyglassDraw2.SM_Tool_SpyglassDraw2"),
		TEXT("/Game/Saltbound/Props/Tools/SM_Tool_SpyglassDraw3.SM_Tool_SpyglassDraw3"),
	};

	TAutoConsoleVariable<int32> CVarArmsWorld(
		TEXT("sb.Arms.World"), 0,
		TEXT("D80: draw the arms and the held tool in the world instead of as first-person parts, to look at them from outside ")
		TEXT("(sb.Dev.Camera)."),
		ECVF_Cheat);
	TAutoConsoleVariable<float> CVarDigPhase(
		TEXT("sb.Tools.DigPhase"), -1.f,
		TEXT("D80: hold the spade at a stroke's phase (0..1) for captures; -1 off."),
		ECVF_Cheat);

	// Where the tools are held, in the view's frame (cm: forward, right, up). D80: the hands hold them at their grips, within
	// the arms' reach and clear of the eye (first-person parts are squeezed towards it, everything nearer than 20 cm is cut).
	// The compass lies in the right palm, its dial tilted to the eye, the bow ahead and a little left; used, close and steep.
	const FVector CompassAt(34.f, 10.f, -14.f);
	const FRotator CompassTilt(32.f, -18.f, 0.f);
	const FVector CompassClose(27.f, 4.f, -8.f);
	const FRotator CompassCloseTilt(58.f, -8.f, 0.f);
	// The hold under the case (its frame: the dial up, the bow ahead), the fingers towards the bow's left, the palm up into it.
	const FVector CompassHold(-0.6f, 0.9f, -1.6f);
	const FVector CompassFingers(0.8f, -0.6f, 0.f);
	// The lantern hangs from the left fist round its ring, a little smaller than the boat's; used, lifted high (both within the
	// left arm's reach: USaltArmsComponent).
	const FVector LanternAt(36.f, -22.f, -6.f);
	const FVector LanternLifted(40.f, -16.f, 4.f);
	constexpr float LanternScale = 0.75f;
	// The spyglass closed in the right fist round its leather, low and pointing ahead and left; used, at the eye, drawn out.
	const FVector SpyglassAt(24.f, 14.f, -11.f);
	const FRotator SpyglassTilt(-12.f, -25.f, 0.f);
	const FVector SpyglassAtEye(24.f, 0.f, -0.5f);
	constexpr float SpyglassFist = 9.f;         // the right fist, cm from the barrel's rear face
	// The drawtubes' rear ends (cm from the barrel's rear face), closed and drawn (build_tools.py); the third's is the eyecup.
	const float DrawClosed[3] = { -1.2f, -2.4f, -4.4f };
	const float DrawOpen[3] = { -7.5f, -15.f, -22.5f };
	// The spade carried: the right fist round the shaft at its whipping (CarryHold cm from the crossbar), low on the right, the
	// shaft ahead and a little down and across to the left, the blade in sight; digging, the right hand slides up to the D-handle
	// and the left takes the whipping (as far down the shaft as it reaches).
	const FVector ShovelCarryAt(30.f, 20.f, -14.f);
	const FRotator ShovelReady(-10.f, -50.f, 0.f);
	constexpr float ShovelCarryHold = 30.f;
	constexpr float ShovelLength = 113.f;
	constexpr float ShovelLowerMin = 12.f;
	constexpr float ShovelLowerMax = 50.f;
	constexpr float SinkDepth = 30.f;
	constexpr float RaiseTime = 0.25f;
	constexpr float UseTime = 0.3f;
	constexpr float SpyglassUseTime = 0.4f;

	// D80 (owner: "create the digging animation when digging in"): one stroke of the spade, in the view's frame (looking at the
	// ground being dug): aim (the hands up and back, the blade pointing down), drive it in along the shaft, lever back (the
	// handle down and back), lift the spadeful, toss it to the right turning the blade over, back to the ready pose.
	struct FStrokeKey
	{
		float T;
		FVector Offset;
		FRotator Turn;
	};
	const FStrokeKey StrokeKeys[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f) },
		{ 0.20f, FVector(-5.f, -3.f, 9.f), FRotator(-20.f, 4.f, 0.f) },
		{ 0.40f, FVector(16.f, -6.f, -12.f), FRotator(-22.f, 4.f, 0.f) },
		{ 0.55f, FVector(6.f, -2.f, -20.f), FRotator(-6.f, 2.f, 0.f) },
		{ 0.72f, FVector(4.f, 3.f, -6.f), FRotator(8.f, 6.f, 0.f) },
		{ 0.86f, FVector(4.f, 12.f, -2.f), FRotator(4.f, 26.f, 60.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f) },
	};
	constexpr float StrokeSeconds = 0.8f;
	// D80: the stroke into a spot on the ground: the crossbar's place from the eyes (cm: ahead, right, up, turned with the look
	// but kept level: the hands stay at the waist whether the eyes look down or not) and the blade's tip from the spot (cm: on
	// towards it from the player, to the right, up), the blade's roll about the shaft (the toss turns it over). Between the
	// carrying pose at 0 and at 1: aim above the spot, drive in, lever back, lift the spadeful, toss it to the right.
	struct FDigKey
	{
		float T;
		FVector Grip;
		FVector Tip;
		float Roll;
	};
	const FDigKey DigKeys[] = {
		{ 0.20f, FVector(24.f, 26.f, -30.f), FVector(-8.f, 6.f, 34.f), 0.f },
		{ 0.40f, FVector(40.f, 18.f, -58.f), FVector(4.f, 2.f, -18.f), 0.f },
		{ 0.55f, FVector(20.f, 24.f, -60.f), FVector(-2.f, 2.f, -12.f), 0.f },
		{ 0.72f, FVector(24.f, 28.f, -38.f), FVector(-14.f, 10.f, 30.f), 0.f },
		{ 0.86f, FVector(26.f, 36.f, -30.f), FVector(-6.f, 50.f, 38.f), 70.f },
	};
	// The farthest the blade goes in, in front of the feet (level, cm), and how far the look finds the ground.
	constexpr float DigReach = 65.f;
	constexpr float DigLook = 320.f;

	/** The spade's frame from its grip to its tip, the blade's face (+Z) turned back towards the eyes and up, rolled about the
	 *  shaft. */
	FTransform SpadeFrame(const FVector& Grip, const FVector& Tip, float RollDeg)
	{
		const FVector X = (Tip - Grip).GetSafeNormal();
		const FVector Ref(-1.f, 0.f, 0.35f);
		const FVector Z = (Ref - X * FVector::DotProduct(Ref, X)).GetSafeNormal();
		const FQuat Q(FMatrix(X, FVector::CrossProduct(Z, X), Z, FVector::ZeroVector));
		return FTransform(FQuat(X, FMath::DegreesToRadians(RollDeg)) * Q, Grip);
	}
	// The blade is in the ground (the spade drawn in the world) and the spadeful on it.
	constexpr float InGroundFrom = 0.3f;
	constexpr float InGroundTo = 0.6f;
	constexpr float LoadFrom = 0.5f;
	constexpr float LoadTo = 0.84f;

	// The needle: a damped swing to north (period 0.9 s, damping ratio 0.25).
	constexpr float NeedlePeriod = 0.9f;
	constexpr float NeedleDamping = 0.25f;

	// Where a grip's hold lies in the rest hand's frame (cm from the wrist: along the fingers, across, up from the back).
	const FVector FistGrip(7.4f, 0.f, -2.6f);
	const FVector CradleGrip(6.f, 0.f, -2.9f);

	FVector Mirror(const FVector& V, int32 Side)
	{
		return Side == 0 ? V : FVector(V.X, -V.Y, V.Z);
	}

	FSaltHandTarget HandAt(const FVector& Hold, const FQuat& Turn, const FVector& GripLocal, const FSaltGrip& Grip, float Weight)
	{
		FSaltHandTarget T;
		T.Turn = Turn;
		T.Wrist = Hold - Turn.RotateVector(GripLocal);
		T.Grip = Grip;
		T.Weight = Weight;
		return T;
	}

	/** A fist round a shaft along Axis, the palm towards Palm (view frame). */
	FQuat FistOn(const FVector& Axis, const FVector& Palm, int32 Side)
	{
		const FVector A = Axis.GetSafeNormal();
		const FVector P = (Palm - A * FVector::DotProduct(Palm, A)).GetSafeNormal();
		const FVector Fingers = Side == 0 ? FVector::CrossProduct(A, P) : FVector::CrossProduct(P, A);
		return USaltArmsComponent::HandTurn(Fingers, P, Side);
	}

	/** A fist round a tool's shaft: the hold on the shaft's axis (tool frame, cm), the shaft along the tool's ShaftAxis, the palm
	 *  facing PalmLocal (tool frame). */
	FSaltHandTarget FistOnTool(const FTransform& ToolInView, const FVector& HoldLocal, const FVector& ShaftAxis, const FVector& PalmLocal,
		int32 Side, float Tight, float Weight)
	{
		const FQuat Turn = FistOn(ToolInView.TransformVectorNoScale(ShaftAxis), ToolInView.TransformVectorNoScale(PalmLocal), Side);
		return HandAt(ToolInView.TransformPosition(HoldLocal), Turn, Mirror(FistGrip, Side), FSaltGrip::Fist(Tight), Weight);
	}

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return 0.5f * ((2.f * P1) + (P2 - P0) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (3.f * P1 - P0 - 3.f * P2 + P3) * T3);
	}
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

void USaltHeldToolsComponent::SetFirstPerson(UPrimitiveComponent* Part, bool bFirstPerson)
{
	const EFirstPersonPrimitiveType Want = bFirstPerson && !bWorldDebug ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None;
	if (Part && Part->FirstPersonPrimitiveType != Want)
	{
		Part->SetFirstPersonPrimitiveType(Want);
	}
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
	Arms = Owner->FindComponentByClass<USaltArmsComponent>();
	auto MeshPart = [Owner](const TCHAR* Name, const TCHAR* Path, USceneComponent* Parent)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Owner, Name);
		M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Path));
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		M->SetupAttachment(Parent);
		M->RegisterComponent();
		return M;
	};
	// The hands hang from the view as it is seen (D58: it leans with the deck), else from the camera.
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(Owner);
	View = Player && Player->GetViewAnchor() ? Player->GetViewAnchor() : Camera.Get();
	USceneComponent* ViewRef = View;
	auto Hand = [Owner, ViewRef](const TCHAR* Name)
	{
		USceneComponent* H = NewObject<USceneComponent>(Owner, Name);
		H->SetupAttachment(ViewRef);
		H->RegisterComponent();
		return H;
	};
	// The compass; its needle just above the dial (the dial's face is 0.15 cm up).
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
	LanternSwing->Length = 11.f;
	LanternSwing->Damping = 0.08f;
	LanternSwing->MaxAngle = 35.f;
	LanternSwing->PushRadius = 0.f;
	LanternSwing->SetupAttachment(LanternHand);
	LanternSwing->RegisterComponent();
	Lantern = MeshPart(TEXT("HeldLantern"), LanternMeshPath, LanternSwing);
	Lantern->SetRelativeScale3D(FVector(LanternScale));
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
	// The spade and its spadeful.
	ShovelHand = Hand(TEXT("ShovelHand"));
	Shovel = MeshPart(TEXT("HeldShovel"), ShovelMeshPath, ShovelHand);
	ShovelLoad = MeshPart(TEXT("HeldShovelLoad"), ShovelLoadMeshPath, Shovel);
	ShovelLoad->SetVisibility(false);
	// The thrown clods: a few lumps of the spadeful's sand flying in the world (absolute, not first-person).
	Clods = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("HeldShovelClods"));
	Clods->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ShovelLoadMeshPath));
	Clods->SetUsingAbsoluteLocation(true);
	Clods->SetUsingAbsoluteRotation(true);
	Clods->SetUsingAbsoluteScale(true);
	Clods->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Clods->SetCastShadow(true);
	Clods->SetupAttachment(Owner->GetRootComponent());
	Clods->RegisterComponent();
	Clods->SetWorldTransform(FTransform::Identity);
	// The spyglass: the barrel, and its drawtubes slid along its axis.
	SpyglassHand = Hand(TEXT("SpyglassHand"));
	SpyglassMesh = MeshPart(TEXT("HeldSpyglass"), SpyglassMeshPath, SpyglassHand);
	for (int32 K = 0; K < 3; ++K)
	{
		UStaticMeshComponent* Draw = MeshPart(*FString::Printf(TEXT("HeldSpyglassDraw%d"), K + 1), SpyglassDrawPaths[K], SpyglassMesh);
		Draw->SetRelativeLocation(FVector(DrawClosed[K], 0.f, 0.f));
		SpyglassDraws.Add(Draw);
	}
	CompassHand->SetVisibility(false, true);
	LanternHand->SetVisibility(false, true);
	ShovelHand->SetVisibility(false, true);
	SpyglassHand->SetVisibility(false, true);
	// First-person primitives are drawn with the eyes' own field of view (kept at the player's, also when the spyglass narrows
	// the view) and squeezed towards the eye so they never poke into walls.
	Camera->SetEnableFirstPersonFieldOfView(true);
	Camera->SetEnableFirstPersonScale(true);
	// D80: 0.4 (0.25 cut everything nearer than 20 cm open: a fist raised close to the eyes showed its insides as shards).
	Camera->SetFirstPersonScale(0.4f);
}

void USaltHeldToolsComponent::SetTool(FName InTool)
{
	Tool = InTool;
}

void USaltHeldToolsComponent::SetUsing(bool bInUsing)
{
	if (bInUsing && !bUsing && Shown == TEXT("Shovel") && Raise > 0.9f && StrokeTime < 0.f)
	{
		// A click with nothing to dig: one stroke.
		StrokeTime = 0.f;
	}
	bUsing = bInUsing;
}

void USaltHeldToolsComponent::GetViewLean(float& OutDown, float& OutPitch) const
{
	// D80: the body bends into the thrust and the lever and straightens with the lift.
	const float P = StrokePhase;
	const float Lean = P < 0.f ? 0.f : FMath::SmoothStep(0.12f, 0.38f, P) * (1.f - FMath::SmoothStep(0.58f, 0.8f, P));
	OutDown = 16.f * Lean;
	OutPitch = -8.f * Lean;
}

void USaltHeldToolsComponent::ThrowClods()
{
	// The spadeful leaves the blade as it turns over: eight lumps thrown to the right and on, tumbling down onto the heap.
	if (!Clods || !ShovelLoad)
	{
		return;
	}
	const FVector From = ShovelLoad->Bounds.Origin;
	const FVector Right = Camera ? Camera->GetRightVector().GetSafeNormal2D() : FVector::RightVector;
	const FVector Fwd = Camera ? Camera->GetForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	FRandomStream R(FMath::Rand());
	for (int32 I = 0; I < 8; ++I)
	{
		FClod C;
		C.Pos = From + FVector(R.FRandRange(-5.f, 5.f), R.FRandRange(-5.f, 5.f), R.FRandRange(0.f, 4.f));
		C.Vel = Right * R.FRandRange(160.f, 260.f) + Fwd * R.FRandRange(-20.f, 60.f) + FVector(0.f, 0.f, R.FRandRange(120.f, 220.f));
		C.Size = R.FRandRange(0.12f, 0.3f);
		C.Life = 1.2f;
		Flying.Add(C);
	}
}

void USaltHeldToolsComponent::UpdateClods(float DeltaTime)
{
	if (!Clods)
	{
		return;
	}
	if (Flying.Num() == 0 && Clods->GetInstanceCount() == 0)
	{
		return;
	}
	const float G = GetWorld() ? GetWorld()->GetGravityZ() : -980.f;
	TArray<FTransform> Xf;
	for (int32 I = Flying.Num() - 1; I >= 0; --I)
	{
		FClod& C = Flying[I];
		C.Life -= DeltaTime;
		C.Vel.Z += G * DeltaTime;
		C.Pos += C.Vel * DeltaTime;
		if (C.Life <= 0.f)
		{
			Flying.RemoveAtSwap(I);
			continue;
		}
		// The lump as a small piece of the spadeful (its mesh sits 90-108 cm along the blade: brought back to the lump's place).
		const float S = C.Size * FMath::Min(1.f, C.Life / 0.25f);
		const FRotator Spin(C.Life * 400.f, C.Life * 260.f, 0.f);
		const FVector MeshCentre(98.f, 0.f, 3.f);
		Xf.Add(FTransform(Spin, C.Pos - Spin.RotateVector(MeshCentre * S), FVector(S)));
	}
	Clods->ClearInstances();
	if (Xf.Num() > 0)
	{
		Clods->AddInstances(Xf, false, true, false);
	}
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

FTransform USaltHeldToolsComponent::ShovelPose(float Phase, const FTransform& ViewXf) const
{
	const FVector ReadyAt = ShovelCarryAt - ShovelReady.Vector() * ShovelCarryHold;
	const FVector ReadyTip = ReadyAt + ShovelReady.Vector() * ShovelLength;
	const FTransform Ready = SpadeFrame(ReadyAt, ReadyTip, 0.f);
	if (Phase < 0.f)
	{
		return Ready;
	}
	const float P = FMath::Clamp(Phase, 0.f, 1.f);
	if (!bDigSpot)
	{
		// Nothing to dig in sight (looking up, over the water): a stroke in the air, in the view's frame.
		constexpr int32 N = UE_ARRAY_COUNT(StrokeKeys);
		int32 K = 0;
		while (K < N - 2 && P > StrokeKeys[K + 1].T)
		{
			++K;
		}
		const float T = FMath::Clamp((P - StrokeKeys[K].T) / FMath::Max(StrokeKeys[K + 1].T - StrokeKeys[K].T, 1e-3f), 0.f, 1.f);
		const FVector Offset = CatmullRom(StrokeKeys[FMath::Max(K - 1, 0)].Offset, StrokeKeys[K].Offset, StrokeKeys[K + 1].Offset,
			StrokeKeys[FMath::Min(K + 2, N - 1)].Offset, T);
		const FQuat Turn = FQuat::Slerp(StrokeKeys[K].Turn.Quaternion(), StrokeKeys[K + 1].Turn.Quaternion(), FMath::SmoothStep(0.f, 1.f, T));
		return FTransform(Turn * Ready.GetRotation(), ReadyAt + Offset);
	}
	// Into the spot: the ready pose, the keys (their tips placed at the spot in the world, seen from the view now), the ready pose.
	constexpr int32 NK = UE_ARRAY_COUNT(DigKeys);
	constexpr int32 N = NK + 2;
	float Ts[N];
	FVector Grips[N];
	FVector Tips[N];
	float Rolls[N];
	Ts[0] = 0.f;
	Grips[0] = ReadyAt;
	Tips[0] = ReadyTip;
	Rolls[0] = 0.f;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, DigFwd).GetSafeNormal();
	const FQuat Level = FRotator(0.f, ViewXf.Rotator().Yaw, 0.f).Quaternion();
	for (int32 I = 0; I < NK; ++I)
	{
		const FDigKey& Key = DigKeys[I];
		Ts[I + 1] = Key.T;
		Grips[I + 1] = ViewXf.InverseTransformPosition(ViewXf.GetLocation() + Level.RotateVector(Key.Grip));
		Tips[I + 1] = ViewXf.InverseTransformPosition(DigSpot + DigFwd * Key.Tip.X + Right * Key.Tip.Y + FVector::UpVector * Key.Tip.Z);
		Rolls[I + 1] = Key.Roll;
	}
	Ts[N - 1] = 1.f;
	Grips[N - 1] = ReadyAt;
	Tips[N - 1] = ReadyTip;
	Rolls[N - 1] = 0.f;
	int32 K = 0;
	while (K < N - 2 && P > Ts[K + 1])
	{
		++K;
	}
	const float T = FMath::Clamp((P - Ts[K]) / FMath::Max(Ts[K + 1] - Ts[K], 1e-3f), 0.f, 1.f);
	const int32 K0 = FMath::Max(K - 1, 0);
	const int32 K3 = FMath::Min(K + 2, N - 1);
	const FVector Grip = CatmullRom(Grips[K0], Grips[K], Grips[K + 1], Grips[K3], T);
	const FVector Tip = CatmullRom(Tips[K0], Tips[K], Tips[K + 1], Tips[K3], T);
	return SpadeFrame(Grip, Tip, FMath::Lerp(Rolls[K], Rolls[K + 1], FMath::SmoothStep(0.f, 1.f, T)));
}

bool USaltHeldToolsComponent::FindDigSpot()
{
	bDigSpot = false;
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Player || !World || !Camera)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltDigSpot), false, Player);
	const FVector Eye = Camera->GetComponentLocation();
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Eye, Eye + Camera->GetForwardVector() * DigLook, ECC_Visibility, Params) || Hit.ImpactNormal.Z < 0.5f)
	{
		return false;
	}
	FVector Spot = Hit.ImpactPoint;
	const FVector Feet = Player->GetActorLocation() - FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector Flat = Spot - Feet;
	Flat.Z = 0.f;
	if (Flat.Size() > DigReach)
	{
		// Further than the blade reaches: the ground that far in front of the feet, the same way.
		const FVector Probe = Feet + Flat.GetSafeNormal() * DigReach;
		if (!World->LineTraceSingleByChannel(Hit, Probe + FVector(0.f, 0.f, 120.f), Probe - FVector(0.f, 0.f, 200.f), ECC_Visibility, Params))
		{
			return false;
		}
		Spot = Hit.ImpactPoint;
	}
	DigSpot = Spot;
	DigFwd = Flat.SizeSquared() > 100.f ? Flat.GetSafeNormal() : Camera->GetForwardVector().GetSafeNormal2D();
	bDigSpot = true;
	return true;
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
	bWorldDebug = CVarArmsWorld.GetValueOnGameThread() != 0;
	// The shown tool sinks out before the next one rises in (and while the eyes are elsewhere).
	const FName Wanted = bSuppressed ? FName() : Tool;
	if (Shown != Wanted)
	{
		Raise = FMath::Max(0.f, Raise - DeltaTime / RaiseTime);
		if (Raise <= 0.f)
		{
			Shown = Wanted;
			Use = 0.f;
			StrokeTime = -1.f;
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
	const bool bCompass = Shown == TEXT("Compass");
	const bool bLantern = Shown == TEXT("Lantern");
	const bool bShovel = Shown == TEXT("Shovel");
	const bool bSpy = Shown == TEXT("Spyglass");
	// D80: the use comes in and goes out (only for the tool fully in hand; the spade's use is its stroke).
	const bool bUseNow = bUsing && Shown == Wanted && Raise > 0.9f && !bShovel;
	Use = FMath::Clamp(Use + (bUseNow ? 1.f : -1.f) * DeltaTime / (bSpy ? SpyglassUseTime : UseTime), 0.f, 1.f);
	const float U = FMath::SmoothStep(0.f, 1.f, Use);
	const float Up = 1.f - FMath::Square(1.f - Raise);
	const FVector Sink(0.f, 0.f, SinkDepth * (1.f - Up));
	Breath += DeltaTime;
	const FVector Sway(0.f, 0.f, 0.3f * FMath::Sin(Breath * 1.7f));

	CompassHand->SetVisibility(bCompass, true);
	LanternHand->SetVisibility(bLantern, true);
	ShovelHand->SetVisibility(bShovel, true);
	CompassHand->SetRelativeLocation(FMath::Lerp(CompassAt, CompassClose, U) - Sink + Sway);
	CompassHand->SetRelativeRotation(FQuat::Slerp(CompassTilt.Quaternion(), CompassCloseTilt.Quaternion(), U));
	LanternHand->SetRelativeLocation(FMath::Lerp(LanternAt, LanternLifted, U) - Sink + Sway);
	LanternHand->SetWorldRotation(FRotator::ZeroRotator);

	// The spade's stroke: three over a dig's hold, one at a click with nothing to dig (sb.Tools.DigPhase holds one for captures).
	StrokePhase = -1.f;
	if (const ASaltPlayerCharacter* Holder = Cast<ASaltPlayerCharacter>(GetOwner()); bShovel && Holder && Holder->GetHoldTarget()
		&& Holder->GetHoldTarget()->ComponentHasTag(TEXT("Dig")) && Holder->GetHoldProgress() > 0.f)
	{
		StrokePhase = FMath::Fmod(Holder->GetHoldProgress() * 3.f, 1.f);
		StrokeTime = -1.f;
	}
	else if (StrokeTime >= 0.f)
	{
		StrokeTime += DeltaTime;
		StrokePhase = FMath::Min(StrokeTime / StrokeSeconds, 1.f);
		if (StrokeTime >= StrokeSeconds)
		{
			StrokeTime = -1.f;
			StrokePhase = -1.f;
		}
	}
	if (const float Held = CVarDigPhase.GetValueOnGameThread(); bShovel && Held >= 0.f)
	{
		StrokePhase = FMath::Min(Held, 1.f);
	}
	if (StrokePhase >= 0.f && LastStrokePhase < 0.f)
	{
		FindDigSpot();
	}
	if (bShovel && LastStrokePhase >= 0.f && LastStrokePhase < LoadTo && StrokePhase >= LoadTo)
	{
		ThrowClods();
	}
	LastStrokePhase = StrokePhase;
	UpdateClods(DeltaTime);
	const FTransform ShovelXf = ShovelPose(StrokePhase, View->GetComponentTransform());
	ShovelHand->SetRelativeLocationAndRotation(ShovelXf.GetLocation() - Sink + Sway, ShovelXf.GetRotation());
	// While its blade is in the ground the spade is drawn in the world, so the ground hides the blade (the same field of view:
	// it stays where it was on the screen); the spadeful shows from the lift to the toss.
	bShovelInWorld = bShovel && StrokePhase >= InGroundFrom && StrokePhase <= InGroundTo;
	SetFirstPerson(Shovel, !bShovelInWorld);
	ShovelLoad->SetVisibility(bShovel && StrokePhase >= LoadFrom && StrokePhase <= LoadTo);
	SetFirstPerson(ShovelLoad, !bShovelInWorld);

	// D80: the spyglass goes up to the eye drawing its tubes out; the view narrows once it is nearly there, and the glass itself is
	// hidden then (the HUD's round view is what is seen).
	SpyglassHand->SetRelativeLocation(FMath::Lerp(SpyglassAt, SpyglassAtEye, U) - Sink + Sway * (1.f - U));
	SpyglassHand->SetRelativeRotation(FQuat::Slerp(SpyglassTilt.Quaternion(), FQuat::Identity, U));
	for (int32 K = 0; K < SpyglassDraws.Num(); ++K)
	{
		const float Draw = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Use - 0.1f * K) / 0.7f, 0.f, 1.f));
		SpyglassDraws[K]->SetRelativeLocation(FVector(FMath::Lerp(DrawClosed[K], DrawOpen[K], Draw), 0.f, 0.f));
	}
	const float Zoom = bSpy && !bSuppressed ? FMath::Clamp((Use - 0.6f) / 0.4f, 0.f, 1.f) : 0.f;
	Spyglass = FMath::FInterpConstantTo(Spyglass, Zoom, DeltaTime, 1.f / 0.2f);
	SpyglassHand->SetVisibility(bSpy && Spyglass < 0.5f, true);

	for (UPrimitiveComponent* Part : { (UPrimitiveComponent*)Compass, (UPrimitiveComponent*)Needle, (UPrimitiveComponent*)Lantern,
		(UPrimitiveComponent*)SpyglassMesh.Get() })
	{
		SetFirstPerson(Part, true);
	}
	for (UStaticMeshComponent* Draw : SpyglassDraws)
	{
		SetFirstPerson(Draw, true);
	}
	if (bCompass)
	{
		UpdateCompass(DeltaTime);
	}
	if (bLantern)
	{
		// The flame's flicker: a slow breath and a small fast flutter (as the boat's lantern); lifted, it lights further.
		Flicker += DeltaTime;
		const float Factor = 1.f + 0.08f * (0.7f * FMath::PerlinNoise1D(Flicker * 0.9f + 5.f) + 0.3f * FMath::PerlinNoise1D(Flicker * 7.3f + 29.f));
		LanternLight->SetIntensity(FMath::Lerp(28.f, 40.f, U) * Factor * Up);
		LanternLight->SetAttenuationRadius(FMath::Lerp(900.f, 1300.f, U));
		if (LanternGlass)
		{
			LanternGlass->SetScalarParameterValue(TEXT("GlowIntensity"), 80.f * Factor);
		}
	}
	Camera->SetFieldOfView(GetFieldOfView());
	Camera->SetFirstPersonFieldOfView(BaseFOV);
	PlaceHands(Up);
}

void USaltHeldToolsComponent::PlaceHands(float Up)
{
	if (!Arms)
	{
		Arms = GetOwner() ? GetOwner()->FindComponentByClass<USaltArmsComponent>() : nullptr;
	}
	if (!Arms || !View)
	{
		return;
	}
	Arms->SetWorldDebug(bWorldDebug);
	// Each tool's place in the view's frame (the arms hang from the same view); the hands go to the tool's grips.
	const FTransform ViewXf = View->GetComponentTransform();
	auto InView = [&ViewXf](const USceneComponent* C) { return C->GetComponentTransform().GetRelativeTransform(ViewXf); };
	// D80: a tool held further than the arm reaches comes back to the hand (the hand never lets go of it); Anchor is the part
	// that places the tool in the view.
	auto Reach = [this](int32 Side, FSaltHandTarget& T, USceneComponent* Anchor)
	{
		const FVector Clamped = Arms->ClampToReach(Side, T.Wrist);
		const FVector Delta = Clamped - T.Wrist;
		if (!Delta.IsNearlyZero(0.05f))
		{
			Anchor->SetRelativeLocation(Anchor->GetRelativeLocation() + Delta);
			T.Wrist = Clamped;
		}
	};
	if (Shown == TEXT("Compass"))
	{
		// In the right palm, cupped under the case, the fingers round its far edge.
		const FTransform C = InView(Compass);
		const FQuat Turn = USaltArmsComponent::HandTurn(C.TransformVectorNoScale(CompassFingers), C.GetUnitAxis(EAxis::Z), 0);
		FSaltHandTarget T = HandAt(C.TransformPosition(CompassHold), Turn, CradleGrip, FSaltGrip::Cradle(), Up);
		Reach(0, T, CompassHand);
		Arms->SetHandTarget(0, T);
	}
	else if (Shown == TEXT("Lantern"))
	{
		// The left fist round the ring on top, knuckles ahead.
		const FTransform C = InView(LanternHand);
		const FQuat Turn = USaltArmsComponent::HandTurn(FVector(0.85f, 0.3f, -0.4f), FVector(0.1f, 0.f, -1.f), 1);
		FSaltHandTarget T = HandAt(C.GetLocation(), Turn, Mirror(FistGrip, 1), FSaltGrip::Fist(0.9f), Up);
		Reach(1, T, LanternHand);
		Arms->SetHandTarget(1, T);
	}
	else if (Shown == TEXT("Shovel"))
	{
		// Carried, the right fist round the shaft at the whipping (the palm up under it); digging, it slides up to the crossbar
		// (the palm towards the shaft) and the left takes the shaft under it, as far down as it reaches.
		const float P = StrokePhase;
		const float Dig = P < 0.f ? 0.f : FMath::SmoothStep(0.f, 0.18f, P) * (1.f - FMath::SmoothStep(0.86f, 1.f, P));
		const FTransform C0 = InView(Shovel);
		const FSaltHandTarget Carry = FistOnTool(C0, FVector(ShovelCarryHold, 0.f, 0.f), FVector::XAxisVector, FVector::ZAxisVector, 0, 0.9f, Up);
		const FSaltHandTarget Grip = FistOnTool(C0, FVector::ZeroVector, FVector::YAxisVector, FVector::XAxisVector, 0, 0.95f, Up);
		FSaltHandTarget R = Grip;
		R.Wrist = FMath::Lerp(Carry.Wrist, Grip.Wrist, Dig);
		R.Turn = FQuat::Slerp(Carry.Turn, Grip.Turn, Dig);
		R.Grip = FSaltGrip::Blend(Carry.Grip, Grip.Grip, Dig);
		Reach(0, R, ShovelHand);
		Arms->SetHandTarget(0, R);
		if (Dig > 0.01f)
		{
			const FTransform C = InView(Shovel);
			const FVector Shoulder = Arms->GetShoulder(1);
			const float Max = Arms->GetReach(1) - 2.f;
			for (float S = ShovelLowerMax; S >= ShovelLowerMin; S -= 2.f)
			{
				const FSaltHandTarget L = FistOnTool(C, FVector(S, 0.f, 0.f), FVector::XAxisVector, FVector::ZAxisVector, 1, 0.95f, Up * Dig);
				if (FVector::DistSquared(L.Wrist, Shoulder) <= Max * Max)
				{
					Arms->SetHandTarget(1, L);
					break;
				}
			}
		}
	}
	else if (Shown == TEXT("Spyglass") && Spyglass < 0.5f)
	{
		// The right fist round the leather, under it.
		FSaltHandTarget T = FistOnTool(InView(SpyglassMesh), FVector(SpyglassFist, 0.f, 0.f), FVector::XAxisVector, FVector::ZAxisVector, 0, 0.9f, Up);
		Reach(0, T, SpyglassHand);
		Arms->SetHandTarget(0, T);
	}
}

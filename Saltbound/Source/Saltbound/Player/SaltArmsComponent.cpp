// Saltbound - the player's arms (D80; see SaltArmsComponent.h).

#include "Player/SaltArmsComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/SkeletalMesh.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"

namespace SaltArmsLocal
{
	// The joints' bend at full curl (degrees): knuckle, middle, last; the thumb's base, middle, last; its swing across the palm.
	constexpr float FingerBend[3] = { 72.f, 95.f, 62.f };
	constexpr float ThumbBend[3] = { 18.f, 42.f, 50.f };
	constexpr float ThumbAcrossDeg = 38.f;
	// Where a hand that holds nothing waits: below the shoulder, out of view (cm from the shoulder, y outwards).
	const FVector LoweredFromShoulder(30.f, 6.f, -48.f);
	// D80: the arms hang from the view a little forward and up of where build_arms.py put the shoulders (10 cm behind the eyes,
	// 30 cm below): first-person arms need the reach in front of the eyes, and a straight arm from far behind ran through the
	// lens (first-person parts nearer than 20 cm are cut and smeared into a flat flap).
	const FVector ShoulderOffset(12.f, 0.f, 8.f);
	// How fast a hand comes up or sinks (its weight per second) and a grip changes.
	constexpr float WeightRate = 5.f;
	constexpr float GripRate = 10.f;

	FTransform About(const FVector& Pivot, const FQuat& R)
	{
		return FTransform(-Pivot) * FTransform(R) * FTransform(Pivot);
	}
}

FSaltGrip FSaltGrip::Relaxed()
{
	return FSaltGrip();
}

FSaltGrip FSaltGrip::Fist(float Tight)
{
	FSaltGrip G;
	G.Curl[0] = 0.55f * Tight;
	for (int32 I = 1; I < 5; ++I)
	{
		G.Curl[I] = Tight * (0.92f + 0.03f * I);
	}
	G.ThumbAcross = 0.65f * Tight;
	return G;
}

FSaltGrip FSaltGrip::Cradle()
{
	FSaltGrip G;
	G.Curl[0] = 0.15f;
	G.Curl[1] = 0.32f;
	G.Curl[2] = 0.36f;
	G.Curl[3] = 0.4f;
	G.Curl[4] = 0.44f;
	G.ThumbAcross = 0.05f;
	return G;
}

FSaltGrip FSaltGrip::Blend(const FSaltGrip& A, const FSaltGrip& B, float T)
{
	FSaltGrip G;
	for (int32 I = 0; I < 5; ++I)
	{
		G.Curl[I] = FMath::Lerp(A.Curl[I], B.Curl[I], T);
	}
	G.ThumbAcross = FMath::Lerp(A.ThumbAcross, B.ThumbAcross, T);
	return G;
}

USaltArmsComponent::USaltArmsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the held tools have placed themselves (they set the targets), before the frame is drawn.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void USaltArmsComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureMesh();
}

void USaltArmsComponent::EnsureMesh()
{
	AActor* Owner = GetOwner();
	if (Mesh || !Owner)
	{
		return;
	}
	USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, SaltContent::ArmsMesh);
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(Owner);
	USceneComponent* View = Player && Player->GetViewAnchor() ? Player->GetViewAnchor() : Owner->FindComponentByClass<UCameraComponent>();
	if (!Asset || !View)
	{
		return;
	}
	// Its own name: the component itself is the character's subobject "Arms".
	Mesh = NewObject<UPoseableMeshComponent>(Owner, TEXT("ArmsMesh"));
	Mesh->SetSkinnedAssetAndUpdate(Asset);
	Mesh->SetupAttachment(View);
	Mesh->SetRelativeLocation(SaltArmsLocal::ShoulderOffset);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->bAffectDistanceFieldLighting = false;
	Mesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	// The arms swing far from their rest pose's bounds.
	Mesh->SetBoundsScale(4.f);
	Mesh->RegisterComponent();
	BuildRig();
}

void USaltArmsComponent::BuildRig()
{
	const USkeletalMesh* Asset = Mesh ? Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()) : nullptr;
	if (!Asset)
	{
		return;
	}
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	const int32 N = Ref.GetNum();
	const TArray<FTransform>& RefLocal = Ref.GetRefBonePose();
	Rig.Parent.SetNum(N);
	Rig.RefCS.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		Rig.Parent[I] = Ref.GetParentIndex(I);
		Rig.RefCS[I] = Rig.Parent[I] >= 0 ? RefLocal[I] * Rig.RefCS[Rig.Parent[I]] : RefLocal[I];
	}
	auto Find = [&Ref](const TCHAR* Name, const TCHAR* Tag) { return Ref.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%s"), Name, Tag))); };
	const TCHAR* Fingers[5] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	bool bOk = true;
	for (int32 K = 0; K < 2; ++K)
	{
		const TCHAR* Tag = K == 0 ? TEXT("r") : TEXT("l");
		FArmRig& A = Rig.Arm[K];
		A.Upper = Find(TEXT("upperarm"), Tag);
		A.Lower = Find(TEXT("lowerarm"), Tag);
		A.Twist = Find(TEXT("lowerarm_twist"), Tag);
		A.Hand = Find(TEXT("hand"), Tag);
		bOk &= A.Upper != INDEX_NONE && A.Lower != INDEX_NONE && A.Twist != INDEX_NONE && A.Hand != INDEX_NONE;
		for (int32 F = 0; F < 5; ++F)
		{
			for (int32 J = 0; J < 3; ++J)
			{
				A.Finger[F][J] = Find(*FString::Printf(TEXT("%s_%02d"), Fingers[F], J + 1), Tag);
				bOk &= A.Finger[F][J] != INDEX_NONE;
			}
		}
		if (!bOk)
		{
			break;
		}
		// The importer decides the mirror: which way this arm lies in the rest pose.
		A.Sign = Rig.RefCS[A.Hand].GetLocation().Y < 0.0 ? -1.f : 1.f;
		// Fingers bend about up x their direction (towards the palm, which faces down in the rest pose); the thumb about its own
		// up (its nail turned 55 degrees out from the back of the hand) x its direction, and swings across about the hand's up.
		const FVector Up = FVector::UpVector;
		const FVector Out(0.f, A.Sign, 0.f);
		for (int32 F = 0; F < 5; ++F)
		{
			const FVector D = (Rig.RefCS[A.Finger[F][1]].GetLocation() - Rig.RefCS[A.Finger[F][0]].GetLocation()).GetSafeNormal();
			FVector FingerUp = F == 0 ? (Up * FMath::Cos(FMath::DegreesToRadians(55.f)) - Out * FMath::Sin(FMath::DegreesToRadians(55.f))) : Up;
			FingerUp = (FingerUp - D * FVector::DotProduct(FingerUp, D)).GetSafeNormal();
			A.Bend[F] = FVector::CrossProduct(FingerUp, D).GetSafeNormal();
		}
		A.Across = Up * A.Sign;
	}
	Rig.bValid = bOk;
	ScratchW.SetNum(N);
	ScratchCS.SetNum(N);
	for (int32 K = 0; K < 2 && bOk; ++K)
	{
		Hands[K].Wrist = Rig.RefCS[Rig.Arm[K].Upper].GetLocation() + SaltArmsLocal::LoweredFromShoulder * FVector(1.f, Rig.Arm[K].Sign, 1.f);
	}
	static bool bLogged = false;
	if (!bLogged)
	{
		bLogged = true;
		const FVector H = bOk ? Rig.RefCS[Rig.Arm[0].Hand].GetLocation() : FVector::ZeroVector;
		UE_LOG(LogSaltbound, Display, TEXT("SB_ARMS rig bones=%d ok=%d hand_r=(%.1f, %.1f, %.1f) signs=%.0f/%.0f"), N, bOk ? 1 : 0, H.X, H.Y, H.Z,
			Rig.Arm[0].Sign, Rig.Arm[1].Sign);
	}
}

void USaltArmsComponent::SetHandTarget(int32 Side, const FSaltHandTarget& Target)
{
	if (Side == 0 || Side == 1)
	{
		Hands[Side].Target = Target;
		Hands[Side].bTargetSet = true;
	}
}

FQuat USaltArmsComponent::HandTurn(const FVector& Fingers, const FVector& Palm, int32 Side)
{
	// The rest hand: fingers along +X, palm towards -Z, the thumb towards the middle (-Y for the right hand, +Y for the left).
	const FVector X = Fingers.GetSafeNormal();
	const FVector Z = (-(Palm - X * FVector::DotProduct(Palm, X))).GetSafeNormal();
	const FVector Y = FVector::CrossProduct(Z, X);
	return FQuat(FMatrix(X, Y, Z, FVector::ZeroVector));
}

void USaltArmsComponent::SetWorldDebug(bool bWorld)
{
	const EFirstPersonPrimitiveType Want = bWorld ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::FirstPerson;
	if (Mesh && Mesh->FirstPersonPrimitiveType != Want)
	{
		Mesh->SetFirstPersonPrimitiveType(Want);
	}
}

FVector USaltArmsComponent::GetWrist(int32 Side) const
{
	return Side == 0 || Side == 1 ? Hands[Side].Wrist + SaltArmsLocal::ShoulderOffset : FVector::ZeroVector;
}

FVector USaltArmsComponent::GetShoulder(int32 Side) const
{
	if (!Rig.bValid || (Side != 0 && Side != 1))
	{
		// Before the mesh is loaded: build_arms.py's shoulders.
		return FVector(-10.f, Side == 1 ? -20.f : 20.f, -30.f) + SaltArmsLocal::ShoulderOffset;
	}
	return Rig.RefCS[Rig.Arm[Side].Upper].GetLocation() + SaltArmsLocal::ShoulderOffset;
}

float USaltArmsComponent::GetReach(int32 Side) const
{
	if (!Rig.bValid || (Side != 0 && Side != 1))
	{
		return 52.f;
	}
	const FArmRig& A = Rig.Arm[Side];
	return (Rig.RefCS[A.Lower].GetLocation() - Rig.RefCS[A.Upper].GetLocation()).Size()
		+ (Rig.RefCS[A.Hand].GetLocation() - Rig.RefCS[A.Lower].GetLocation()).Size();
}

FVector USaltArmsComponent::ClampToReach(int32 Side, const FVector& Wrist, float Margin) const
{
	const FVector S = GetShoulder(Side);
	const FVector D = Wrist - S;
	const float Max = FMath::Max(GetReach(Side) - Margin, 1.f);
	return D.SizeSquared() > Max * Max ? S + D.GetSafeNormal() * Max : Wrist;
}

void USaltArmsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EnsureMesh();
	if (!Mesh || !Rig.bValid)
	{
		return;
	}
	const int32 N = Rig.RefCS.Num();
	if (Mesh->BoneSpaceTransforms.Num() != N)
	{
		return;
	}
	for (int32 I = 0; I < N; ++I)
	{
		ScratchW[I] = FTransform::Identity;
	}
	PoseArm(0, ScratchW, DeltaTime);
	PoseArm(1, ScratchW, DeltaTime);
	// Component-space transforms and back to bone space (parents come before children in the reference skeleton).
	for (int32 I = 0; I < N; ++I)
	{
		ScratchCS[I] = Rig.RefCS[I] * ScratchW[I];
		const int32 Pa = Rig.Parent[I];
		Mesh->BoneSpaceTransforms[I] = Pa >= 0 ? ScratchCS[I].GetRelativeTransform(ScratchCS[Pa]) : ScratchCS[I];
	}
	Mesh->MarkRefreshTransformDirty();
	const bool bShow = Hands[0].Weight > 0.01f || Hands[1].Weight > 0.01f;
	if (Mesh->IsVisible() != bShow)
	{
		Mesh->SetVisibility(bShow);
	}
	Hands[0].bTargetSet = false;
	Hands[1].bTargetSet = false;
}

void USaltArmsComponent::PoseArm(int32 Side, TArray<FTransform>& W, float DeltaTime)
{
	using namespace SaltArmsLocal;
	const FArmRig& A = Rig.Arm[Side];
	FHandState& H = Hands[Side];
	const float WantWeight = H.bTargetSet ? FMath::Clamp(H.Target.Weight, 0.f, 1.f) : 0.f;
	H.Weight = FMath::FInterpConstantTo(H.Weight, WantWeight, DeltaTime, WeightRate);
	H.Grip = FSaltGrip::Blend(H.Grip, H.bTargetSet ? H.Target.Grip : FSaltGrip::Relaxed(), FMath::Clamp(DeltaTime * GripRate, 0.f, 1.f));

	const FVector S = Rig.RefCS[A.Upper].GetLocation();
	const FVector E0 = Rig.RefCS[A.Lower].GetLocation();
	const FVector W0 = Rig.RefCS[A.Hand].GetLocation();
	const FVector Mid0 = Rig.RefCS[A.Twist].GetLocation();
	const float L1 = (E0 - S).Size();
	const float L2 = (W0 - E0).Size();

	// Where the wrist goes: the target (the view's frame, so less the shoulders' offset), or down out of view.
	const FVector Lowered = S + LoweredFromShoulder * FVector(1.f, A.Sign, 1.f);
	const float Up = FMath::SmoothStep(0.f, 1.f, H.Weight);
	const FVector WT = FMath::Lerp(Lowered, H.Target.Wrist - ShoulderOffset, Up);
	const FQuat Turn = FQuat::Slerp(FQuat::Identity, H.Target.Turn, Up);
	H.Wrist = WT;

	// Two-bone IK: the elbow down and out from the line shoulder - wrist.
	FVector ToW = WT - S;
	const float Dist = FMath::Clamp((float)ToW.Size(), FMath::Abs(L1 - L2) + 0.5f, L1 + L2 - 0.05f);
	const FVector Dir = ToW.GetSafeNormal();
	const float Along = (L1 * L1 - L2 * L2 + Dist * Dist) / (2.f * Dist);
	const float Height = FMath::Sqrt(FMath::Max(L1 * L1 - Along * Along, 0.f));
	FVector Pole = FVector(-0.2f, 0.55f * A.Sign, -1.f);
	Pole = (Pole - Dir * FVector::DotProduct(Pole, Dir)).GetSafeNormal();
	const FVector E1 = S + Dir * Along + Pole * Height;
	const FVector W1 = S + Dir * Dist;

	const FQuat RUpper = FQuat::FindBetweenNormals((E0 - S).GetSafeNormal(), (E1 - S).GetSafeNormal());
	W[A.Upper] = About(S, RUpper);
	const FQuat RLower = FQuat::FindBetweenNormals((W0 - E0).GetSafeNormal(), (W1 - E1).GetSafeNormal());
	W[A.Lower] = FTransform(-E0) * FTransform(RLower) * FTransform(E1);
	// The hand turns as asked (its delta from the rest pose), at the reached wrist.
	W[A.Hand] = FTransform(-W0) * FTransform(Turn) * FTransform(W1);
	// The forearm's twist bone: the forearm's turn plus half of the hand's roll about the forearm.
	const FVector Axis0 = (W0 - E0).GetSafeNormal();
	FQuat Swing, Twist;
	(RLower.Inverse() * Turn).ToSwingTwist(Axis0, Swing, Twist);
	const FQuat Half = FQuat::Slerp(FQuat::Identity, Twist, 0.5f);
	W[A.Twist] = FTransform(-Mid0) * FTransform(RLower * Half) * FTransform(E1 + RLower.RotateVector(Mid0 - E0));

	// The fingers: each joint turns about itself in the rest pose, then goes with its parent.
	for (int32 F = 0; F < 5; ++F)
	{
		const float C = FMath::Clamp(H.Grip.Curl[F], 0.f, 1.f);
		for (int32 J = 0; J < 3; ++J)
		{
			const int32 B = A.Finger[F][J];
			const FVector Joint = Rig.RefCS[B].GetLocation();
			FQuat R(A.Bend[F], FMath::DegreesToRadians((F == 0 ? ThumbBend[J] : FingerBend[J]) * C));
			if (F == 0 && J == 0)
			{
				R = FQuat(A.Across, FMath::DegreesToRadians(ThumbAcrossDeg * FMath::Clamp(H.Grip.ThumbAcross, 0.f, 1.f))) * R;
			}
			const int32 Pa = J == 0 ? A.Hand : A.Finger[F][J - 1];
			W[B] = About(Joint, R) * W[Pa];
		}
	}
}

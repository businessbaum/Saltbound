// Saltbound - seagulls flying over an area (ambient life; D39, D76; see SaltBirds.h).

#include "World/SaltBirds.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Saltbound.h"

namespace SaltBirdsLocal
{
	TAutoConsoleVariable<int32> CVarFlap(TEXT("sb.Birds.Flap"), -1,
		TEXT("The gulls' wings (D76): -1 as they fly (long glides, bursts of flapping), 0 gliding only, 1 flapping only (to watch the wingbeat)."));

	float Rad(float Deg)
	{
		return FMath::DegreesToRadians(Deg);
	}

	/** A rotation (and scale) about a joint, in the rest pose's component frame. */
	FTransform Pivot(const FVector& Joint, const FQuat& Q, const FVector& Scale = FVector::OneVector)
	{
		return FTransform(-Joint) * FTransform(Q, FVector::ZeroVector, Scale) * FTransform(Joint);
	}
}

ASaltBirds::ASaltBirds()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
	for (int32 I = 0; I < MaxBirds; ++I)
	{
		UPoseableMeshComponent* M = CreateDefaultSubobject<UPoseableMeshComponent>(*FString::Printf(TEXT("Gull%02d"), I));
		M->SetupAttachment(Root);
		M->SetMobility(EComponentMobility::Movable);
		M->SetUsingAbsoluteLocation(true);
		M->SetUsingAbsoluteRotation(true);
		M->SetUsingAbsoluteScale(true);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->SetCastShadow(true);
		M->bAffectDistanceFieldLighting = false;
		M->SetVisibility(false);
		Meshes.Add(M);
	}
}

void ASaltBirds::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveContent();
	InitBirds();
	UpdateBirds(0.f);
}

void ASaltBirds::BeginPlay()
{
	Super::BeginPlay();
	ResolveContent();
	InitBirds();
	UpdateBirds(0.f);
}

void ASaltBirds::ResolveContent()
{
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	USkeletalMesh* Gull = nullptr;
	for (int32 I = 0; I < Meshes.Num(); ++I)
	{
		UPoseableMeshComponent* M = Meshes[I];
		if (!M)
		{
			continue;
		}
		if (!M->GetSkinnedAsset())
		{
			if (!Gull)
			{
				Gull = LoadObject<USkeletalMesh>(nullptr, SaltContent::GullMesh);
			}
			if (Gull)
			{
				M->SetSkinnedAssetAndUpdate(Gull);
			}
		}
		M->SetVisibility(I < Count && M->GetSkinnedAsset() != nullptr);
	}
	Rig.bValid = false;
	BuildRig();
}

void ASaltBirds::BuildRig()
{
	const USkeletalMesh* Mesh = Meshes.Num() ? Cast<USkeletalMesh>(Meshes[0]->GetSkinnedAsset()) : nullptr;
	if (!Mesh)
	{
		return;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 N = Ref.GetNum();
	Rig.Parent.SetNum(N);
	Rig.RefLocal = Ref.GetRefBonePose();
	Rig.RefCS.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		Rig.Parent[I] = Ref.GetParentIndex(I);
		Rig.RefCS[I] = Rig.Parent[I] >= 0 ? Rig.RefLocal[I] * Rig.RefCS[Rig.Parent[I]] : Rig.RefLocal[I];
	}
	Rig.Neck = Ref.FindBoneIndex(TEXT("neck"));
	Rig.Head = Ref.FindBoneIndex(TEXT("head"));
	Rig.Tail = Ref.FindBoneIndex(TEXT("tail"));
	const TCHAR* Sides[2] = {TEXT("R"), TEXT("L")};
	for (int32 K = 0; K < 2; ++K)
	{
		Rig.Upper[K] = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("upper_%s"), Sides[K])));
		Rig.Fore[K] = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("fore_%s"), Sides[K])));
		Rig.Hand[K] = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("hand_%s"), Sides[K])));
		// Which way the wing lies in the rest pose (the importer decides the mirror; the flap's signs follow it).
		Rig.Side[K] = Rig.Hand[K] != INDEX_NONE && Rig.RefCS[Rig.Hand[K]].GetLocation().Y < 0.0 ? -1.f : 1.f;
	}
	Rig.bValid = Rig.Upper[0] != INDEX_NONE && Rig.Upper[1] != INDEX_NONE && Rig.Hand[0] != INDEX_NONE && Rig.Hand[1] != INDEX_NONE;
	ScratchW.SetNum(N);
	ScratchCS.SetNum(N);
	static bool bLogged = false;
	if (!bLogged && Rig.bValid)
	{
		bLogged = true;
		const FVector H = Rig.Head != INDEX_NONE ? Rig.RefCS[Rig.Head].GetLocation() : FVector::ZeroVector;
		UE_LOG(LogSaltbound, Display, TEXT("SB_BIRDS rig bones=%d head=(%.1f, %.1f, %.1f) hand_R=(%.1f, %.1f, %.1f) sides=%.0f/%.0f"), N, H.X, H.Y, H.Z,
			Rig.RefCS[Rig.Hand[0]].GetLocation().X, Rig.RefCS[Rig.Hand[0]].GetLocation().Y, Rig.RefCS[Rig.Hand[0]].GetLocation().Z, Rig.Side[0], Rig.Side[1]);
	}
}

void ASaltBirds::InitBirds()
{
	FRandomStream Rng(Seed);
	Birds.Reset();
	const int32 N = FMath::Clamp(Count, 0, MaxBirds);
	for (int32 I = 0; I < N; ++I)
	{
		FBird B;
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = AreaRadius * FMath::Sqrt(Rng.FRand());
		B.Centre = FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
		const float D = Rng.FRandRange(0.f, 2.f * PI);
		B.Drift = FVector2D(FMath::Cos(D), FMath::Sin(D)) * Rng.FRandRange(30.f, 90.f);
		B.Radius = Rng.FRandRange(CircleRadius.X, CircleRadius.Y);
		B.Angle = Rng.FRandRange(0.f, 2.f * PI);
		B.Dir = Rng.FRand() < 0.5f ? -1.f : 1.f;
		B.BaseHeight = Rng.FRandRange(HeightRange.X, HeightRange.Y);
		B.HeightPhase = Rng.FRandRange(0.f, 2.f * PI);
		B.SpeedScale = Rng.FRandRange(0.85f, 1.15f);
		B.Beat = BeatsPerSecond * Rng.FRandRange(0.9f, 1.1f);
		B.FlapPhase = Rng.FRandRange(0.f, 2.f * PI);
		B.FlapTimer = Rng.FRandRange(0.5f, 6.f);
		B.bFlapping = Rng.FRand() < 0.3f;
		B.FlapBlend = B.bFlapping ? 1.f : 0.f;
		B.Noise = Rng.FRandRange(0.f, 20.f);
		B.LookTimer = Rng.FRandRange(0.5f, 3.f);
		Birds.Add(B);
	}
	for (int32 I = 0; I < Meshes.Num(); ++I)
	{
		if (Meshes[I])
		{
			Meshes[I]->SetVisibility(I < N && Meshes[I]->GetSkinnedAsset() != nullptr);
		}
	}
}

void ASaltBirds::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBirds(FMath::Min(DeltaSeconds, 0.1f));
	UpdateWatch(DeltaSeconds);
}

FTransform ASaltBirds::GetBirdTransform(int32 Index) const
{
	return Birds.IsValidIndex(Index) ? Birds[Index].Xf : GetActorTransform();
}

void ASaltBirds::UpdateBirds(float DeltaSeconds)
{
	if (!Rig.bValid)
	{
		BuildRig();
		if (!Rig.bValid)
		{
			return;
		}
	}
	using namespace SaltBirdsLocal;
	Time += DeltaSeconds;
	FRandomStream Rng(Seed + (int32)(Time * 10.0));
	const FVector Origin = GetActorLocation();
	for (int32 I = 0; I < Birds.Num() && I < Meshes.Num(); ++I)
	{
		FBird& B = Birds[I];
		UPoseableMeshComponent* M = Meshes[I];
		if (!M || !M->GetSkinnedAsset())
		{
			continue;
		}
		// Soaring circle round a slowly drifting centre (turned back towards the area when it strays).
		B.Centre += B.Drift * DeltaSeconds;
		if (B.Centre.Size() > AreaRadius)
		{
			B.Drift = -B.Centre.GetSafeNormal() * B.Drift.Size();
		}
		const float V = Speed * B.SpeedScale * (1.f + 0.12f * B.FlapBlend);
		const float R = FMath::Max(B.Radius, 100.f);
		B.Angle += B.Dir * V / R * DeltaSeconds;
		const FVector2D Radial(FMath::Cos(B.Angle), FMath::Sin(B.Angle));
		const FVector2D P = B.Centre + Radial * B.Radius;
		const float Height = B.BaseHeight + 400.f * FMath::Sin((float)Time * 0.11f + B.HeightPhase);
		const float Climb = 400.f * 0.11f * FMath::Cos((float)Time * 0.11f + B.HeightPhase);

		// Long glides, bursts of flapping (more of it while climbing); the wings settle into and out of the beat.
		B.FlapTimer -= DeltaSeconds;
		if (B.FlapTimer <= 0.f)
		{
			B.bFlapping = !B.bFlapping;
			B.FlapTimer = B.bFlapping ? Rng.FRandRange(1.2f, 3.2f) * (Climb > 0.f ? 1.4f : 1.f) : Rng.FRandRange(4.f, 10.f);
		}
		const int32 Force = CVarFlap.GetValueOnGameThread();
		const bool bFlap = Force < 0 ? B.bFlapping : Force > 0;
		B.FlapBlend = FMath::FInterpTo(B.FlapBlend, bFlap ? 1.f : 0.f, DeltaSeconds, 2.5f);
		B.FlapPhase = FMath::Fmod(B.FlapPhase + DeltaSeconds * 2.f * PI * B.Beat, 2.f * PI);

		// Heading along the circle; the bank of the turn (tan(bank) = v^2 / (r g)) eased in; a little pitch when climbing.
		const FVector2D Vel = FVector2D(-Radial.Y, Radial.X) * B.Dir;
		const float Yaw = FMath::Atan2(Vel.Y, Vel.X);
		B.Bank = FMath::FInterpTo(B.Bank, FMath::Atan(V * V / (R * 980.f)), DeltaSeconds, 1.5f);
		const float Pitch = FMath::Atan2(Climb, V);
		// The body bobs up as the wings push down and noses up a touch with each stroke.
		const float Bob = B.FlapBlend * 1.6f * -FMath::Cos(B.FlapPhase) * BirdScale;
		const float BodyPitch = Pitch + Rad(B.FlapBlend * 2.5f * FMath::Sin(B.FlapPhase));
		const FQuat Q = FQuat(FVector::UpVector, Yaw) * FQuat(FVector::RightVector, -BodyPitch) * FQuat(FVector::ForwardVector, -B.Dir * B.Bank);
		B.Xf = FTransform(Q, Origin + FVector(P.X, P.Y, Height + Bob), FVector(BirdScale));
		M->SetWorldTransform(B.Xf);

		// Glances: ahead, about, down at the water; held a moment, turned to smoothly.
		B.LookTimer -= DeltaSeconds;
		if (B.LookTimer <= 0.f)
		{
			const bool bAhead = Rng.FRand() < 0.45f;
			B.LookYawTarget = bAhead ? 0.f : Rng.FRandRange(15.f, 45.f) * (Rng.FRand() < 0.5f ? -1.f : 1.f);
			B.LookPitchTarget = bAhead ? 0.f : Rng.FRandRange(6.f, 26.f);
			B.LookTimer = Rng.FRandRange(0.8f, 2.6f);
		}
		B.LookYaw = FMath::FInterpTo(B.LookYaw, B.LookYawTarget, DeltaSeconds, 3.5f);
		B.LookPitch = FMath::FInterpTo(B.LookPitch, B.LookPitchTarget, DeltaSeconds, 3.5f);
		PoseBird(M, B, FMath::RadiansToDegrees(B.Dir * V / R), FMath::RadiansToDegrees(Pitch));
	}
}

void ASaltBirds::PoseBird(UPoseableMeshComponent* M, const FBird& B, float TurnRateDeg, float ClimbPitchDeg)
{
	using namespace SaltBirdsLocal;
	const int32 N = Rig.RefLocal.Num();
	if (M->BoneSpaceTransforms.Num() != N)
	{
		return;
	}
	TArray<FTransform, TInlineAllocator<16>> Own;
	Own.Init(FTransform::Identity, N);
	auto Joint = [this](int32 Bone)
	{
		return Rig.RefCS[Bone].GetLocation();
	};
	const float A = B.FlapBlend;
	const float Ph = B.FlapPhase;
	const float T = (float)Time;
	const float Upstroke = FMath::Max(FMath::Cos(Ph), 0.f);
	const float Downstroke = FMath::Max(-FMath::Cos(Ph), 0.f);
	for (int32 K = 0; K < 2; ++K)
	{
		const float S = Rig.Side[K];
		// Small trimming movements while gliding, different on each side.
		const float Trim = FMath::Sin(T * 0.83f + B.Noise + K * 1.7f) * 0.6f + FMath::Sin(T * 2.1f + B.Noise * 1.3f + K) * 0.4f;
		// (up = the tip up, back = swept back, twist = the leading edge down; all relative to the rest pose, spread level)
		auto WingQ = [S](float UpDeg, float BackDeg, float TwistDeg)
		{
			return FQuat(FVector::ForwardVector, Rad(S * UpDeg)) * FQuat(FVector::UpVector, Rad(S * BackDeg)) * FQuat(FVector::RightVector, Rad(TwistDeg));
		};
		// The arm: raised in the glide, through about 60 degrees in the beat (-24..+36); it leads.
		const float ArmUp = FMath::Lerp(7.f + 2.5f * Trim, 6.f + 30.f * FMath::Sin(Ph), A);
		const float ArmBack = FMath::Lerp(1.5f, 4.f + 8.f * Upstroke, A);
		const float ArmTwist = FMath::Lerp(0.8f * Trim, 9.f * Downstroke - 4.f * Upstroke, A);
		// The forearm: a little further up in the glide (the bend of the wing forward), folding back on the upstroke.
		const float ForeUp = FMath::Lerp(4.f, 12.f * FMath::Sin(Ph - 0.7f), A);
		const float ForeBack = FMath::Lerp(-5.f, 20.f * Upstroke - 2.f, A);
		const float ForeTwist = FMath::Lerp(0.f, 4.f * Downstroke, A);
		// The hand: the gull's crooked glide (drooping, swept back); in the beat it trails the arm, folds back on the
		// upstroke and spreads and twists on the downstroke.
		const float HandUp = FMath::Lerp(-15.f + 2.5f * Trim, 15.f * FMath::Sin(Ph - 1.3f) - 4.f, A);
		const float HandBack = FMath::Lerp(22.f + 1.5f * Trim, 6.f + 30.f * FMath::Max(FMath::Cos(Ph - 0.35f), 0.f), A);
		const float HandTwist = FMath::Lerp(2.f, 12.f * FMath::Max(-FMath::Cos(Ph - 0.4f), 0.f) - 3.f * Upstroke, A);
		if (Rig.Upper[K] != INDEX_NONE)
		{
			Own[Rig.Upper[K]] = Pivot(Joint(Rig.Upper[K]), WingQ(ArmUp, ArmBack, ArmTwist));
		}
		if (Rig.Fore[K] != INDEX_NONE)
		{
			Own[Rig.Fore[K]] = Pivot(Joint(Rig.Fore[K]), WingQ(ForeUp, ForeBack, ForeTwist));
		}
		if (Rig.Hand[K] != INDEX_NONE)
		{
			Own[Rig.Hand[K]] = Pivot(Joint(Rig.Hand[K]), WingQ(HandUp, HandBack, HandTwist));
		}
	}
	// The tail: up a little (more with the beat), twisted into the turn, fanned out in turns and while flapping.
	const float BankDeg = FMath::RadiansToDegrees(B.Bank) * B.Dir;
	if (Rig.Tail != INDEX_NONE)
	{
		const float TailUp = 2.f + A * 3.f * FMath::Sin(Ph) - ClimbPitchDeg * 0.4f;
		const float TailRoll = -BankDeg * 0.3f;
		const float Spread = 1.f + 0.25f * A + 0.3f * FMath::Min(FMath::Abs(TurnRateDeg) / 40.f, 1.f);
		Own[Rig.Tail] = Pivot(Joint(Rig.Tail), FQuat(FVector::ForwardVector, Rad(TailRoll)) * FQuat(FVector::RightVector, Rad(TailUp)),
			FVector(1.f, Spread, 1.f));
	}
	// The head: looks into the turn and about, stays level against the bank and the climb (the neck takes part of it).
	const float LookYaw = FMath::Clamp(TurnRateDeg * 0.5f, -30.f, 30.f) + B.LookYaw;
	const float LookPitch = B.LookPitch + ClimbPitchDeg * 0.5f;
	if (Rig.Neck != INDEX_NONE)
	{
		Own[Rig.Neck] = Pivot(Joint(Rig.Neck), FQuat(FVector::UpVector, Rad(LookYaw * 0.4f)) * FQuat(FVector::RightVector, Rad(LookPitch * 0.4f)));
	}
	if (Rig.Head != INDEX_NONE)
	{
		Own[Rig.Head] = Pivot(Joint(Rig.Head), FQuat(FVector::UpVector, Rad(LookYaw * 0.6f)) * FQuat(FVector::RightVector, Rad(LookPitch * 0.6f))
			* FQuat(FVector::ForwardVector, Rad(BankDeg * 0.65f)));
	}
	// Forward through the skeleton: each bone's own motion about its joint in the rest frame, then its parent's.
	for (int32 I = 0; I < N; ++I)
	{
		const int32 Pa = Rig.Parent[I];
		ScratchW[I] = Pa >= 0 ? Own[I] * ScratchW[Pa] : Own[I];
		ScratchCS[I] = Rig.RefCS[I] * ScratchW[I];
		M->BoneSpaceTransforms[I] = Pa >= 0 ? ScratchCS[I].GetRelativeTransform(ScratchCS[Pa]) : ScratchCS[I];
	}
	M->MarkRefreshTransformDirty();
}

void ASaltBirds::Watch(int32 Index, float DistanceCm, float YawDeg, float PitchDeg)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!Birds.IsValidIndex(Index))
	{
		WatchIndex = INDEX_NONE;
		if (WatchCamera)
		{
			if (PC && PC->GetViewTarget() == WatchCamera && PC->GetPawn())
			{
				PC->SetViewTarget(PC->GetPawn());
			}
			WatchCamera->Destroy();
			WatchCamera = nullptr;
		}
		return;
	}
	WatchIndex = Index;
	WatchDistance = DistanceCm;
	WatchYaw = YawDeg;
	WatchPitch = PitchDeg;
	if (!WatchCamera && World)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		WatchCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Birds[Index].Xf.GetLocation(), FRotator::ZeroRotator, Params);
		if (WatchCamera)
		{
			WatchCamera->GetCameraComponent()->bConstrainAspectRatio = false;
			WatchCamera->GetCameraComponent()->SetFieldOfView(60.f);
		}
	}
	UpdateWatch(0.f);
	if (PC && WatchCamera)
	{
		PC->SetViewTarget(WatchCamera);
	}
}

void ASaltBirds::UpdateWatch(float DeltaSeconds)
{
	if (!WatchCamera || !Birds.IsValidIndex(WatchIndex))
	{
		return;
	}
	const FTransform& X = Birds[WatchIndex].Xf;
	const float Yaw = (float)X.Rotator().Yaw;
	const FVector Bird = X.GetLocation();
	const FVector Want = Bird + FRotator(WatchPitch, Yaw + WatchYaw, 0.f).Vector() * WatchDistance;
	const FVector At = DeltaSeconds > 0.f ? FMath::VInterpTo(WatchCamera->GetActorLocation(), Want, DeltaSeconds, 6.f) : Want;
	WatchCamera->SetActorLocationAndRotation(At, (Bird - At).Rotation());
}

#if !UE_BUILD_SHIPPING
namespace SaltBirdsCommands
{
	FAutoConsoleCommandWithWorldAndArgs CmdBirdCam(
		TEXT("sb.Dev.BirdCam"),
		TEXT("sb.Dev.BirdCam <bird|off> [distance cm, 400] [yaw deg round from its heading, 160] [pitch deg, 12]: a camera following a gull of the flock nearest the player (D76)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			const FVector Eye = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			ASaltBirds* Best = nullptr;
			for (TActorIterator<ASaltBirds> It(World); It; ++It)
			{
				It->Watch(INDEX_NONE, 0.f, 0.f, 0.f);
				if (It->GetBirdCount() > 0 && (!Best || FVector::Dist(It->GetActorLocation(), Eye) < FVector::Dist(Best->GetActorLocation(), Eye)))
				{
					Best = *It;
				}
			}
			if (!Best || Args.Num() == 0 || Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogSaltbound, Display, TEXT("SB_BIRDCAM off"));
				return;
			}
			const int32 Index = FMath::Clamp(FCString::Atoi(*Args[0]), 0, Best->GetBirdCount() - 1);
			const float Dist = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 400.f;
			const float Yaw = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 160.f;
			const float Pitch = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 12.f;
			Best->Watch(Index, Dist, Yaw, Pitch);
			UE_LOG(LogSaltbound, Display, TEXT("SB_BIRDCAM %s bird %d dist=%.0f yaw=%.0f pitch=%.0f"), *Best->GetName(), Index, Dist, Yaw, Pitch);
		}));
}
#endif

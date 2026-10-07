// Saltbound - people (D83; see SaltPerson.h).

#include "World/SaltPerson.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PoseableMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Saltbound.h"
#include "World/SaltTalk.h"

namespace SaltPersonLocal
{
	FTransform About(const FVector& Pivot, const FQuat& R)
	{
		return FTransform(-Pivot) * FTransform(R) * FTransform(Pivot);
	}

	constexpr float BreathPeriod = 4.2f;
	constexpr float LookRange = 700.f;       // cm
	constexpr float LookMaxYaw = 65.f;
	constexpr float LookMaxPitch = 25.f;
	constexpr float LookRate = 2.2f;         // how quickly the head follows (1/s)
	constexpr float GestureRate = 1.6f;
}

ASaltPerson::ASaltPerson()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Figure"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(true);
	Mesh->SetBoundsScale(1.5f);
	Rng.Initialize(83);
}

void ASaltPerson::BeginPlay()
{
	Super::BeginPlay();
	EnsureMesh();
}

void ASaltPerson::EnsureMesh()
{
	if (Rig.bValid || MeshPath.IsNull())
	{
		return;
	}
	USkeletalMesh* Asset = Cast<USkeletalMesh>(MeshPath.TryLoad());
	if (!Asset)
	{
		return;
	}
	if (Mesh->GetSkinnedAsset() != Asset)
	{
		Mesh->SetSkinnedAssetAndUpdate(Asset);
	}
	BuildRig();
}

void ASaltPerson::BuildRig()
{
	const USkeletalMesh* Asset = Cast<USkeletalMesh>(Mesh->GetSkinnedAsset());
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
	Rig.Spine1 = Ref.FindBoneIndex(TEXT("spine_01"));
	Rig.Spine2 = Ref.FindBoneIndex(TEXT("spine_02"));
	Rig.Neck = Ref.FindBoneIndex(TEXT("neck"));
	Rig.Head = Ref.FindBoneIndex(TEXT("head"));
	Rig.UpperR = Ref.FindBoneIndex(TEXT("upperarm_r"));
	Rig.LowerR = Ref.FindBoneIndex(TEXT("lowerarm_r"));
	Rig.HandR = Ref.FindBoneIndex(TEXT("hand_r"));
	Rig.bValid = Rig.Spine2 != INDEX_NONE && Rig.Neck != INDEX_NONE && Rig.Head != INDEX_NONE && Rig.UpperR != INDEX_NONE
		&& Rig.LowerR != INDEX_NONE && Rig.HandR != INDEX_NONE;
	W.SetNum(N);
	CS.SetNum(N);
	UE_LOG(LogSaltbound, Display, TEXT("SB_PERSON %s rig bones=%d ok=%d"), *SpeakerId.ToString(), N, Rig.bValid ? 1 : 0);
}

void ASaltPerson::Tick(float DeltaSeconds)
{
	using namespace SaltPersonLocal;
	Super::Tick(DeltaSeconds);
	EnsureMesh();
	const int32 N = Rig.RefCS.Num();
	if (!Rig.bValid || Mesh->BoneSpaceTransforms.Num() != N)
	{
		return;
	}
	Time += DeltaSeconds;
	UWorld* World = GetWorld();
	const FTransform MeshXf = Mesh->GetComponentTransform();
	const FVector HeadJoint = Rig.RefCS[Rig.Head].GetLocation();

	// Who is looked at: the player's eyes, near and in front; else a glance about now and then.
	FVector2D Want = Glance;
	if (const APlayerCameraManager* Cam = World ? UGameplayStatics::GetPlayerCameraManager(World, 0) : nullptr)
	{
		const FVector Local = MeshXf.InverseTransformPosition(Cam->GetCameraLocation()) - HeadJoint;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Local.Z, Local.Size2D()));
		if (Local.Size() < LookRange && FMath::Abs(Yaw) < 105.f)
		{
			Want = FVector2D(FMath::Clamp(Yaw, -LookMaxYaw, LookMaxYaw), FMath::Clamp(Pitch, -LookMaxPitch, LookMaxPitch));
			NextGlance = Time + Rng.FRandRange(2.f, 5.f);
		}
	}
	if (Time > NextGlance)
	{
		Glance = FVector2D(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-10.f, 4.f));
		NextGlance = Time + Rng.FRandRange(3.f, 7.f);
	}
	LookYaw = FMath::FInterpTo(LookYaw, Want.X, DeltaSeconds, LookRate);
	LookPitch = FMath::FInterpTo(LookPitch, Want.Y, DeltaSeconds, LookRate);

	// Talking: the gesture comes up and the head nods with the words.
	const USaltTalkSubsystem* Talk = World ? World->GetSubsystem<USaltTalkSubsystem>() : nullptr;
	const bool bTalking = Talk && Talk->IsTalking() && Talk->GetSpeakerId() == SpeakerId;
	Gesture = FMath::FInterpConstantTo(Gesture, bTalking ? 1.f : 0.f, DeltaSeconds, GestureRate);
	const float G = FMath::SmoothStep(0.f, 1.f, Gesture);
	const float Breath = FMath::Sin(TWO_PI * Time / BreathPeriod);
	const float Nod = 2.5f * G * FMath::Sin(Time * 5.3f) * (0.6f + 0.4f * FMath::Sin(Time * 1.3f));

	// Each bone's own turn about its joint (rest space), composed down the chain.
	TMap<int32, FQuat> Own;
	Own.Add(Rig.Spine2, FRotator(0.8f * Breath, LookYaw * 0.15f, 0.f).Quaternion());
	Own.Add(Rig.Neck, FRotator(LookPitch * 0.4f, LookYaw * 0.35f, 0.f).Quaternion());
	Own.Add(Rig.Head, FRotator(LookPitch * 0.6f + Nod, LookYaw * 0.5f, 0.f).Quaternion());
	if (G > 0.001f)
	{
		// The right hand up off the counter in an open gesture: the upper arm forward, the forearm up, the palm turned up.
		const FVector S = Rig.RefCS[Rig.UpperR].GetLocation();
		const FVector E = Rig.RefCS[Rig.LowerR].GetLocation();
		const FVector Wr = Rig.RefCS[Rig.HandR].GetLocation();
		const FVector Du = (E - S).GetSafeNormal();
		const float Wave = 1.f + 0.15f * FMath::Sin(Time * 2.1f);
		const FVector Dut = FMath::Lerp(Du, FVector(0.55f, 0.25f, -0.8f).GetSafeNormal(), FMath::Min(0.45f * G * Wave, 1.f)).GetSafeNormal();
		const FQuat RUpper = FQuat::FindBetweenNormals(Du, Dut);
		const FVector Df = (Wr - E).GetSafeNormal();
		const FVector Dft = FMath::Lerp(RUpper.RotateVector(Df), FVector(0.75f, -0.1f, 0.62f).GetSafeNormal(), 0.85f * G).GetSafeNormal();
		const FQuat RLower = FQuat::FindBetweenNormals(Df, RUpper.Inverse().RotateVector(Dft));
		Own.Add(Rig.UpperR, RUpper);
		Own.Add(Rig.LowerR, RLower);
		Own.Add(Rig.HandR, FQuat(Df, FMath::DegreesToRadians(-70.f * G)));
	}
	for (int32 I = 0; I < N; ++I)
	{
		const int32 Pa = Rig.Parent[I];
		const FQuat* R = Own.Find(I);
		const FTransform Mine = R ? About(Rig.RefCS[I].GetLocation(), *R) : FTransform::Identity;
		W[I] = Pa >= 0 ? Mine * W[Pa] : Mine;
		CS[I] = Rig.RefCS[I] * W[I];
		Mesh->BoneSpaceTransforms[I] = Pa >= 0 ? CS[I].GetRelativeTransform(CS[Pa]) : CS[I];
	}
	Mesh->MarkRefreshTransformDirty();
}

// --- the people at their places -----------------------------------------------------------------------------------------------

bool USaltPeopleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USaltPeopleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Who has a figure (the people come one by one, FH2 and FH6).
	const TMap<FName, const TCHAR*> Figures = { { TEXT("Keeper"), SaltContent::KeeperMesh } };
	USaltTalkSubsystem* Talk = InWorld.GetSubsystem<USaltTalkSubsystem>();
	for (TActorIterator<ATargetPoint> It(&InWorld); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			FString S = Tag.ToString();
			if (!S.RemoveFromEnd(TEXT("Spot")))
			{
				continue;
			}
			const FName Speaker(*S);
			const TCHAR* const* Path = Figures.Find(Speaker);
			if (!Path || FindPerson(Speaker))
			{
				continue;
			}
			const FRotator Facing(0.f, It->GetActorRotation().Yaw, 0.f);
			ASaltPerson* Person = InWorld.SpawnActorDeferred<ASaltPerson>(ASaltPerson::StaticClass(), FTransform(Facing, It->GetActorLocation()),
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Person)
			{
				continue;
			}
			Person->SpeakerId = Speaker;
			Person->MeshPath = FSoftObjectPath(*Path);
			Person->FinishSpawning(FTransform(Facing, It->GetActorLocation()));
			People.Add(Person);
			if (Talk)
			{
				Talk->SetSpeakerActor(Speaker, Person);
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_PEOPLE %s at %s yaw %.0f"), *Speaker.ToString(), *It->GetActorLocation().ToString(), Facing.Yaw);
		}
	}
}

ASaltPerson* USaltPeopleSubsystem::FindPerson(FName SpeakerId) const
{
	for (ASaltPerson* P : People)
	{
		if (P && P->SpeakerId == SpeakerId)
		{
			return P;
		}
	}
	return nullptr;
}

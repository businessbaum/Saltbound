// Saltbound - the player's arms (D80).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaltArmsComponent.generated.h"

class UPoseableMeshComponent;
class USkeletalMesh;

/** How a hand's fingers lie: 0 open .. 1 closed for the thumb, index, middle, ring and little finger, and the thumb across the
 *  palm (0 beside it .. 1 over it). */
struct SALTBOUND_API FSaltGrip
{
	float Curl[5] = { 0.2f, 0.22f, 0.25f, 0.28f, 0.3f };
	float ThumbAcross = 0.15f;

	/** Hanging loose. */
	static FSaltGrip Relaxed();
	/** Closed round a handle (a shaft, a bail, a tube): Tight 1 a fist. */
	static FSaltGrip Fist(float Tight = 0.85f);
	/** Cupped under something carried on the palm, the thumb along its edge. */
	static FSaltGrip Cradle();
	static FSaltGrip Blend(const FSaltGrip& A, const FSaltGrip& B, float T);
};

/** Where a hand should be: its wrist in the view's frame (cm), the hand's turn from its rest pose (palm down, fingers ahead, the
 *  thumb towards the middle), its grip, and how much it is wanted (0 lowered out of view .. 1 there). */
struct SALTBOUND_API FSaltHandTarget
{
	FVector Wrist = FVector::ZeroVector;
	FQuat Turn = FQuat::Identity;
	FSaltGrip Grip;
	float Weight = 0.f;
};

/**
 * D80 (owner: "Every object now will first be taken into your hand ... This means the Design of our charakter begins: Make it in a
 * comic style, be detailed but not realistic"): the player's arms, SK_Arms (build_arms.py), seen from the eyes. Each frame what
 * the hands hold (USaltHeldToolsComponent) says where each wrist should be and how its fingers close; the arms reach there:
 * - the shoulder stays, the elbow is found from the two bones' lengths (two-bone IK), hanging down and out;
 * - the hand turns as asked, the forearm's twist bone takes half of its roll about the forearm so the wrist does not wring;
 * - each finger bends at its three joints by its curl, the thumb also swings across the palm.
 * A hand that holds nothing sinks out of view below the shoulder. Drawn as a first-person primitive (it never pokes into walls),
 * no shadows, no collision. The mesh loads on first use.
 */
UCLASS(ClassGroup = (Saltbound))
class SALTBOUND_API USaltArmsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USaltArmsComponent();

	/** Side 0: the right hand, 1: the left. Set every frame by what the hands hold; unset hands sink out of view. */
	void SetHandTarget(int32 Side, const FSaltHandTarget& Target);
	/** The rest hand's turn that points its fingers along Fingers and its palm towards Palm (view frame). Side flips the thumb. */
	static FQuat HandTurn(const FVector& Fingers, const FVector& Palm, int32 Side);
	UPoseableMeshComponent* GetMesh() const { return Mesh; }
	/** Where a wrist is now (view frame, cm) and how far its hand is up (0 lowered .. 1 at its target). */
	FVector GetWrist(int32 Side) const;
	/** The shoulder (view frame, cm) and how far its wrist reaches from it (the arm straight, cm). */
	FVector GetShoulder(int32 Side) const;
	float GetReach(int32 Side) const;
	/** A wrist target moved within the arm's reach (Margin cm short of straight). */
	FVector ClampToReach(int32 Side, const FVector& Wrist, float Margin = 2.f) const;
	float GetHandWeight(int32 Side) const { return Side == 0 ? Hands[0].Weight : Hands[1].Weight; }
	bool IsReady() const { return Rig.bValid; }
	/** Drawn in the world instead of as a first-person part (sb.Arms.World, to look at the arms from outside). */
	void SetWorldDebug(bool bWorld);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FArmRig
	{
		int32 Upper = INDEX_NONE;
		int32 Lower = INDEX_NONE;
		int32 Twist = INDEX_NONE;
		int32 Hand = INDEX_NONE;
		/** Thumb, index, middle, ring, little finger; three joints each. */
		int32 Finger[5][3];
		/** The axis each finger bends about towards the palm (rest component space). */
		FVector Bend[5];
		/** The axis the thumb swings across the palm about. */
		FVector Across = FVector::UpVector;
		float Sign = 1.f;
	};
	struct FRig
	{
		bool bValid = false;
		TArray<int32> Parent;
		TArray<FTransform> RefCS;
		FArmRig Arm[2];
	};
	struct FHandState
	{
		FSaltHandTarget Target;
		bool bTargetSet = false;
		/** Smoothed: how far the hand is up, and the grip it shows. */
		float Weight = 0.f;
		FSaltGrip Grip;
		FVector Wrist = FVector::ZeroVector;
	};

	void EnsureMesh();
	void BuildRig();
	void PoseArm(int32 Side, TArray<FTransform>& W, float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<UPoseableMeshComponent> Mesh;
	FRig Rig;
	FHandState Hands[2];
	TArray<FTransform> ScratchW;
	TArray<FTransform> ScratchCS;
};

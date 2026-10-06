// Saltbound - seagulls flying over an area (ambient life; D39, D76).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltBirds.generated.h"

class ACameraActor;
class UPoseableMeshComponent;

/**
 * A small flock of gulls flying over the actor: each soars in wide circles round a drifting centre, banks into its turn, rises
 * and sinks slowly, and alternates long glides with bursts of flapping.
 *
 * D76 (owner: "fix the birds. I want detailed birds with good animation when flying"): each gull is the skinned SK_Gull
 * (build_birds.py) on a poseable mesh, its bones posed every frame from the flight:
 * - the wingbeat: the arm sweeps through about 60 degrees, the hand trails it (a wave runs out along the wing), the wing
 *   folds back at the elbow and the wrist on the upstroke and spreads on the downstroke, the leading edge dips as it pushes;
 *   the body bobs up on each downstroke;
 * - the glide: the gull's crooked wing, the arm raised and the hand swept back and drooping, with small trimming movements;
 * - in a turn the body banks, the tail twists into the turn and fans out, the head stays level and looks where it goes; now and
 *   then it glances about and down at the water.
 * Deterministic per Seed. No collision.
 */
UCLASS()
class SALTBOUND_API ASaltBirds : public AActor
{
	GENERATED_BODY()

public:
	ASaltBirds();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static constexpr int32 MaxBirds = 24;

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0", ClampMax = "24"))
	int32 Count = 9;

	/** Circle centres wander within this radius around the actor. */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (Units = "cm"))
	float AreaRadius = 6000.f;

	/** Flight height above the actor (min, max). */
	UPROPERTY(EditAnywhere, Category = "Birds")
	FVector2D HeightRange = FVector2D(1400.f, 4500.f);

	/** Circle radius (min, max). */
	UPROPERTY(EditAnywhere, Category = "Birds")
	FVector2D CircleRadius = FVector2D(1200.f, 3000.f);

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (Units = "cm/s"))
	float Speed = 850.f;

	/** Size of the birds (1 = SK_Gull's 1.42 m wingspan, a herring gull's). */
	UPROPERTY(EditAnywhere, Category = "Birds")
	float BirdScale = 1.3f;

	/** Wingbeats a second while flapping (each bird a little faster or slower). */
	UPROPERTY(EditAnywhere, Category = "Birds")
	float BeatsPerSecond = 2.9f;

	UPROPERTY(EditAnywhere, Category = "Birds")
	int32 Seed = 7;

	int32 GetBirdCount() const { return Birds.Num(); }
	FTransform GetBirdTransform(int32 Index) const;
	/** Development: a camera following a bird (cm behind it, degrees round and above), or none (Index < 0). */
	void Watch(int32 Index, float DistanceCm, float YawDeg, float PitchDeg);

private:
	struct FBird
	{
		FVector2D Centre = FVector2D::ZeroVector;
		FVector2D Drift = FVector2D::ZeroVector;
		float Radius = 2000.f;
		float Angle = 0.f;
		float Dir = 1.f;
		float BaseHeight = 2500.f;
		float HeightPhase = 0.f;
		float SpeedScale = 1.f;
		float Beat = 2.9f;
		float FlapPhase = 0.f;
		float FlapTimer = 0.f;
		bool bFlapping = false;
		float FlapBlend = 0.f;
		float Bank = 0.f;
		float Noise = 0.f;
		// Glances: where the head looks (degrees off the way ahead) and how long it holds.
		float LookYaw = 0.f;
		float LookPitch = 0.f;
		float LookYawTarget = 0.f;
		float LookPitchTarget = 0.f;
		float LookTimer = 0.f;
		FTransform Xf;
	};

	/** The skeleton as SK_Gull has it: parents, rest pose (local and component space), the bones that move. */
	struct FRig
	{
		bool bValid = false;
		TArray<int32> Parent;
		TArray<FTransform> RefLocal;
		TArray<FTransform> RefCS;
		int32 Neck = INDEX_NONE;
		int32 Head = INDEX_NONE;
		int32 Tail = INDEX_NONE;
		int32 Upper[2] = {INDEX_NONE, INDEX_NONE};
		int32 Fore[2] = {INDEX_NONE, INDEX_NONE};
		int32 Hand[2] = {INDEX_NONE, INDEX_NONE};
		/** +1 when the wing [k] lies to +Y in the rest pose. */
		float Side[2] = {1.f, -1.f};
	};

	void ResolveContent();
	void BuildRig();
	void InitBirds();
	void UpdateBirds(float DeltaSeconds);
	void PoseBird(UPoseableMeshComponent* Mesh, const FBird& B, float TurnRateDeg, float ClimbPitchDeg);
	void UpdateWatch(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UPoseableMeshComponent>> Meshes;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> WatchCamera;

	TArray<FBird> Birds;
	FRig Rig;
	TArray<FTransform> ScratchW;
	TArray<FTransform> ScratchCS;
	double Time = 0.0;
	int32 WatchIndex = INDEX_NONE;
	float WatchDistance = 400.f;
	float WatchYaw = 160.f;
	float WatchPitch = 12.f;
};

// Saltbound - people (D83): a figure at its place that breathes, looks at whoever comes near and gestures while it speaks.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltPerson.generated.h"

class UPoseableMeshComponent;

/**
 * D83 (owner, D77: "simple stylised figures behind counters"): one of the people, a skinned figure (build_keeper.py) posed every
 * frame, no animation asset:
 *  - breathing: the chest rises and falls (4.2 s), the shoulders with it;
 *  - looking: within 7 m and in front of them the head turns to the player's eyes (the neck and the chest take a share), slowly,
 *    as a person does; otherwise now and then a glance about (the sea, the pier);
 *  - talking (USaltTalkSubsystem, their SpeakerId): the right hand comes up off the counter in a slow open gesture, the head
 *    nods a little with the words.
 * The figure stands where its feet are (the actor's origin), facing +X. Spawned at play by USaltPeopleSubsystem.
 */
UCLASS()
class SALTBOUND_API ASaltPerson : public AActor
{
	GENERATED_BODY()

public:
	ASaltPerson();

	/** Who this is in the talks (Lines.json): Keeper, ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Person")
	FName SpeakerId;

	/** The figure (a skinned mesh with root, pelvis, spine_01, spine_02, neck, head and sb_hand's arms). */
	UPROPERTY(EditAnywhere, Category = "Person")
	FSoftObjectPath MeshPath;

	UPoseableMeshComponent* GetMesh() const { return Mesh; }
	/** The head's turn now (degrees, from facing ahead) and how far the talking gesture has come (0..1). */
	FVector2D GetLook() const { return FVector2D(LookYaw, LookPitch); }
	float GetGesture() const { return Gesture; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void EnsureMesh();
	void BuildRig();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPoseableMeshComponent> Mesh;

	struct FRig
	{
		bool bValid = false;
		TArray<int32> Parent;
		TArray<FTransform> RefCS;
		int32 Spine1 = INDEX_NONE, Spine2 = INDEX_NONE, Neck = INDEX_NONE, Head = INDEX_NONE;
		int32 UpperR = INDEX_NONE, LowerR = INDEX_NONE, HandR = INDEX_NONE;
	};
	FRig Rig;
	TArray<FTransform> W;
	TArray<FTransform> CS;
	float Time = 0.f;
	float LookYaw = 0.f;
	float LookPitch = 0.f;
	float Gesture = 0.f;
	/** An idle glance: where, and when the next one comes. */
	FVector2D Glance = FVector2D::ZeroVector;
	float NextGlance = 3.f;
	FRandomStream Rng;
};

/** D83: stands the people at their places when play begins: a target point tagged "<Speaker>Spot" (the map's Keeper_Spot, tag
 *  KeeperSpot) gets that speaker's figure, which the talks then know as where the speaker stands. */
UCLASS()
class SALTBOUND_API USaltPeopleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	ASaltPerson* FindPerson(FName SpeakerId) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASaltPerson>> People;
};

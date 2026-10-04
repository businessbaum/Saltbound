// Saltbound - seagulls soaring over an area (ambient life).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltBirds.generated.h"

class UInstancedStaticMeshComponent;

/**
 * A small flock of gulls circling over the actor: each bird soars in wide circles around a drifting centre,
 * banks into its turn, rises and sinks slowly, and alternates long glides with short bursts of flapping.
 * Instanced meshes (body, left and right wing, Art/Blender/Scripts/build_birds.py); the wings are separate
 * instances rotated at the shoulder each frame. Deterministic per Seed. No collision.
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

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0", ClampMax = "64"))
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

	/** Size of the birds (1 = about 1.4 m wingspan, a herring gull; a little larger reads better from below). */
	UPROPERTY(EditAnywhere, Category = "Birds")
	float BirdScale = 1.3f;

	UPROPERTY(EditAnywhere, Category = "Birds")
	int32 Seed = 7;

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
		float FlapPhase = 0.f;
		float FlapTimer = 0.f;
		bool bFlapping = false;
		float FlapBlend = 0.f;
	};

	void ResolveContent();
	void InitBirds();
	void UpdateBirds(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Bodies;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> WingsLeft;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> WingsRight;

	TArray<FBird> Birds;
	double Time = 0.0;
};

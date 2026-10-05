// Saltbound - what the hub tests measure against (D63): the data's open floors, routes and sample points, written into L_Hub
// by build_hub_level.py, so the C++ checks (sb.Dev.RunHubTests) take every number from HubShip_Data.json.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubTestData.generated.h"

/** An open floor of the data (open zones' rectangles): x/y in the ship's level frame (cm), the floor's height. */
USTRUCT(BlueprintType)
struct FSaltHubArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector2D Min = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector2D Max = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float Floor = 0.f;
};

/** A walk from the fitting berth to a service (feet positions, the level frame, cm). */
USTRUCT(BlueprintType)
struct FSaltHubRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FVector> Points;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "s")) float TargetSeconds = 40.f;
};

/** The actor stands at L_Hub's origin (the ship's pivot, bow +X); every position is in its frame (cm). */
UCLASS()
class SALTBOUND_API ASaltHubTestData : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubTestData();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FSaltHubArea> OpenFloors;
	/** Open floor allowed below MinFloorZ: the slipway runs down into the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FSaltHubArea> LowFloorExempt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FSaltHubRoute> Routes;
	/** The sea under her hull and in the water hall (x, y; z unused). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FVector> HullWater;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FVector> BasinWater;
	/** Under the keel (x, y), the keel's depth (cm, negative). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FVector> KeelPoints;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float KeelZ = -1200.f;
	/** The prompt's limits (section 9). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float SeabedClear = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float HullWaterMax = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float BasinWaterMax = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float MinFloorZ = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float MinHeadroom = 220.f;
	/** The water hall's side doors (D64): the clear way between the open leaves along x, the hull's half-breadth at them, where a
	 *  boat starts to sail in (out from the starboard pair) and the hall's middle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector2D DoorsX = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float DoorsHullHalf = 1950.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector BoatInStart = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector HallMiddle = FVector::ZeroVector;
	/** The slipway (dock.slipway): its rectangle, and where a boat starts to run up it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FSaltHubArea Slipway;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector SlipStart = FVector::ZeroVector;

	FVector ToWorld(const FVector& Local) const { return GetActorTransform().TransformPosition(Local); }
	FVector ToLocal(const FVector& World) const { return GetActorTransform().InverseTransformPosition(World); }
	float LocalYawToWorld(float Yaw) const { return (float)FRotator::NormalizeAxis(GetActorRotation().Yaw + Yaw); }
};

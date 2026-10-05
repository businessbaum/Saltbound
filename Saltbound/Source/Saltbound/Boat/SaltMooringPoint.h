// Saltbound - a place the player's boat can moor (D46, D62): the pier at home, the hub ship's berths.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SaltMooringPoint.generated.h"

/** A mooring: the boat's pose when it lies there (its origin on the waterline, its heading), which side the quay or pier is
 *  and how far from the boat's centreline its edge is (Reach, + starboard), and how high that edge stands over the pose (the
 *  lines and the gangway land there). */
USTRUCT(BlueprintType)
struct FSaltMooring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring") FTransform Pose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring", meta = (Units = "cm")) float Reach = 240.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring", meta = (Units = "cm")) float PierHeight = 111.f;

	bool IsValid() const { return !Id.IsNone(); }
	/** +1 when the quay is to starboard, -1 to port. */
	float Side() const { return Reach >= 0.f ? 1.f : -1.f; }
};

/**
 * A mooring placed in a level (the hub's fitting and fleet berths, build_hub_level.py): its transform is the boat's moored
 * pose (+X the boat's bow). The boat's windlass moors to the nearest one within its MooringRadius (or to its own start
 * position, the pier).
 */
UCLASS(ClassGroup = (Saltbound), meta = (BlueprintSpawnableComponent))
class SALTBOUND_API USaltMooringPointComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring") FName MooringId;
	/** The quay's edge from the boat's centreline: + starboard, - port (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring", meta = (Units = "cm")) float Reach = 210.f;
	/** The quay's edge over the pose (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mooring", meta = (Units = "cm")) float PierHeight = 150.f;

	FSaltMooring GetMooring() const;
};

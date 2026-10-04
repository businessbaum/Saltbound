// Saltbound - the named places of the world (D55): where they are, their shores, and finding a new island (landfall).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltPlaces.generated.h"

/**
 * A named place on the charts. Positions are world metres (north = +X, east = +Y, as the charts). An island's shore is
 * an ellipse (semi-axes in metres, turned by ShoreYawDeg); a region (the storm waters) or the home cove has none and is
 * not found by coming near.
 */
struct FSaltPlace
{
	FName Id;
	FText Name;
	FVector2D CentreM = FVector2D::ZeroVector;
	FVector2D ShoreM = FVector2D::ZeroVector;
	float ShoreYawDeg = 0.f;
	bool bIsland = false;
};

namespace SaltPlaces
{
	/** An island is found the first time the player comes this close to its shore (owner, D55). */
	constexpr float DiscoverDistanceM = 300.f;

	SALTBOUND_API const TArray<FSaltPlace>& All();
	SALTBOUND_API const FSaltPlace* Find(FName Id);
	/** Metres from the place's shore (negative inside it); from its centre when it has no shore. */
	SALTBOUND_API float DistanceToShoreM(const FSaltPlace& Place, const FVector2D& WorldM);
	/** Where it lies from Saltbound Cove: "1.7 km north-north-east of Saltbound Cove" (16 points of the compass). */
	SALTBOUND_API FText FromHome(const FSaltPlace& Place);
}

DECLARE_MULTICAST_DELEGATE_OneParam(FSaltLandfallEvent, FName /*PlaceId*/);

/**
 * Watches for landfall: about four times a second, whether the player has come within DiscoverDistanceM of an island not
 * found yet. Then the island is marked found (in USaltSaveSubsystem, saved with the game) and OnLandfall fires once (the
 * HUD shows the banner).
 */
UCLASS()
class SALTBOUND_API USaltLandfallSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Checks a world position (cm) now; returns the island found there (None if none new). */
	FName CheckAt(const FVector& WorldCm);

	FSaltLandfallEvent OnLandfall;

private:
	float Accumulated = 0.f;
};

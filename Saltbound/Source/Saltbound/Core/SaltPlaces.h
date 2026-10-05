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

/**
 * The charts (D57): no map in the satchel or among the tools; a small chart is pinned to the start boat's mast. It shows a
 * window of the region chart (T_Map_Region, Tools/textures/gen_ui.py, the open sea at about 3 m a pixel) round the boat,
 * north up, live. Keep these in sync with gen_ui.py (MAPS) and Tools/ue_scripts/setup_chart_materials.py.
 */
namespace SaltChart
{
	/** The region chart's square: centre (m, north +X, east +Y) and size (m). */
	inline const FVector2D CentreM(3000.0, 0.0);
	constexpr float SizeM = 12000.f;
	/** The mast chart shows this much round the boat (m, across), on a sheet this much taller than wide (0.33 / 0.26 m). */
	constexpr float WindowM = 1600.f;
	constexpr float SheetWidthM = 0.26f;
	constexpr float SheetHeightM = 0.33f;
	constexpr float SheetAspect = SheetHeightM / SheetWidthM;
	/** Where a world position (cm) lies on the region chart (0..1: u east, v south). */
	inline FVector2D ToUV(const FVector& WorldCm)
	{
		return FVector2D(0.5 + (WorldCm.Y * 0.01 - CentreM.Y) / SizeM, 0.5 - (WorldCm.X * 0.01 - CentreM.X) / SizeM);
	}
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

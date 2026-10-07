// Saltbound - the Gale (D87): the storm wall round the sea, the storms that break out of it, the wreck at the edge.
//
// The Gale is the world's edge inside the story (GameConcept 3.2, Docs/Design/Islands.md 5): a ring of storm about 9.5 km out, a
// wall of dark cloud to 3 km high with rain curtains and lightning, seen from everywhere in the sea. Sailing at it, the sky closes,
// the wind rises against the boat and the sea grows to its biggest; inside the wall the hull is sprung plank by plank, and past its
// core the Gale wrecks the boat and gives the castaway back on the wreck beach. Now and then a storm tears loose and drifts in (a
// breakout): its own cloud, rain, lightning, wind and sea, for a quarter of an hour, more often from the south where the relays fail.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltGale.generated.h"

class ASaltSeaStateZone;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

namespace SaltGale
{
	/** The wall's segments stand on this ring (cm from the cove). */
	constexpr double WallRadius = 950000.0;
	/** The storm begins to close in 1.5 km before the wall's face and is full at its core. */
	constexpr double StormStart = 750000.0;
	constexpr double StormFull = 960000.0;
	/** Past this the Gale wrecks the boat (and whoever is in the water). */
	constexpr double CoreRadius = 980000.0;
	/** Inside the wall (past its face) the hull is sprung. */
	constexpr double FaceRadius = 900000.0;
	/** The wall's foot: local fog volumes round the sea (cm), overlapping into one wall of rain and fog. */
	constexpr double FogRadius = 990000.0;
	constexpr int32 FogVolumes = 48;

	inline const FName WreckBeat = TEXT("GaleWreck");
}

/** How the wall looks (sb.Gale look tunes it in play): its fog (density, grey albedo, height, height falloff) and its clouds (how stormy,
 *  how much more cover, how high the tops reach; they need setup_gale_sky.py's cloud material, not applied yet). The fog is off by
 *  default: the wall's look is still open (Decisions D87). */
struct FSaltGaleLook
{
	float FogDensity = 0.f;
	float FogGrey = 0.12f;
	float FogHeightKm = 1.5f;
	float CloudStorm = 1.f;
	float CoverageBoost = 1.2f;
	float TopsKm = 9.f;
	float FogFalloff = 1000.f;
};

/** A storm that broke out of the wall and drifts in (cm, cm/s, s). */
struct FSaltGaleCell
{
	FVector2D At = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	float Radius = 100000.f;
	float Age = 0.f;
	float Life = 1200.f;
	int32 Id = 0;
	/** Broke out of the wall (it dies away when it reaches the cove); one set over a point by hand does not. */
	bool bFromWall = false;
	/** 0..1: it gathers for a minute, then fades over its last two. */
	float Strength() const;
};

/** What the Gale looks like (owner: "fog, clouds and the storm form a natural wall with no edges"): nothing of it is a mesh but the
 *  bolts and the rain round the camera. The wall's clouds are the sky's own volumetric clouds turned to storm along a ring round the
 *  sea (setup_gale_sky.py: Gale_Ring, Gale_Cell0..2 on the rig's cloud material, with the engine's lightning in them); under the cloud
 *  layer (5 km) a ring of local fog volumes is the dark, rain-heavy foot of the wall, and a breakout is a fog column under its clouds.
 *  Spawned by USaltGaleSubsystem; content loads at run time (Environment/Sky/Gale, always cooked). */
UCLASS(NotPlaceable)
class SALTBOUND_API ASaltGale : public AActor
{
	GENERATED_BODY()

public:
	ASaltGale();
	virtual void Tick(float DeltaSeconds) override;

	void Build();
	/** The wall's fog volumes round the sea. */
	int32 GetWallCount() const { return WallFog.Num(); }
	/** The breakouts as they are now: their fog columns and their storm in the clouds. */
	void ShowCells(const TArray<FSaltGaleCell>& Cells);
	/** Breakouts showing now (fog columns with any strength). */
	int32 GetCellCount() const;
	/** The storm ring and the breakouts as the cloud material has them now (tests). */
	bool GetCloudStorm(FLinearColor& OutRing, FLinearColor& OutCell0) const;
	/** A bolt striking the sea at At (cm), the flash lighting the sea round it. */
	void Strike(const FVector& At, float CloudBaseCm);
	/** The rain round the camera (0..1). */
	void SetNearRain(float Amount, const FVector& View);
	float GetNearRain() const { return NearRainAmount; }
	bool IsStriking() const { return BoltLeft > 0.f; }
	void SetLook(const FSaltGaleLook& Look);
	const FSaltGaleLook& GetLook() const { return WallLook; }

	UPROPERTY(VisibleAnywhere, Category = "Gale") TArray<TObjectPtr<class ULocalFogVolumeComponent>> WallFog;
	UPROPERTY(VisibleAnywhere, Category = "Gale") TArray<TObjectPtr<class ULocalFogVolumeComponent>> CellFog;
	UPROPERTY(VisibleAnywhere, Category = "Gale") TObjectPtr<UStaticMeshComponent> Bolt;
	UPROPERTY(VisibleAnywhere, Category = "Gale") TObjectPtr<UPointLightComponent> FlashLight;
	UPROPERTY(VisibleAnywhere, Category = "Gale") TObjectPtr<UStaticMeshComponent> NearRain;

private:
	static float Flicker(float Age, float Length);
	UMaterialInstanceDynamic* CloudMaterial() const;
	void PushClouds(const TArray<FSaltGaleCell>& Cells);

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> BoltMeshes;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BoltMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> NearRainMID;
	float BoltAge = 0.f;
	float BoltLeft = 0.f;
	float NearRainAmount = 0.f;
	bool bBuilt = false;
	FSaltGaleLook WallLook;
};

UCLASS()
class SALTBOUND_API USaltGaleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** How stormy it is at a point (0 calm .. 1 the Gale's core or a breakout's heart). */
	float StormAt(const FVector2D& WorldXY) const;
	float WallAt(const FVector2D& WorldXY) const;
	/** The storm where the viewer is now (what the weather, sea, wind and rain follow). */
	float GetViewerStorm() const { return ViewerStorm; }

	/** A storm breaks out of the wall now: on a bearing from the cove (degrees; < 0 the south-west most often) at a distance
	 *  (cm; < 0 the wall's face), drifting in. Returns its id. */
	int32 Breakout(float BearingDeg = -1.f, float DistanceCm = -1.f);
	/** A breakout over a point now (tests, sb.Gale.Breakout here), drifting along Velocity (cm/s). */
	int32 BreakoutAt(const FVector2D& At, const FVector2D& Velocity, float Radius = 110000.f, float StartAge = 0.f);
	const TArray<FSaltGaleCell>& GetCells() const { return Cells; }
	void ClearCells();
	ASaltGale* GetVisuals() const { return Visuals; }
	ASaltSeaStateZone* GetWallZone() const { return WallZone; }
	const TArray<TObjectPtr<ASaltSeaStateZone>>& GetCellZones() const { return CellZones; }

	/** The castaway is given back: the screen goes dark, the boat is found at her mooring, the castaway wakes on the wreck beach. */
	void Wreck();
	bool IsWrecking() const { return WreckIn >= 0.f; }
	/** Tests: the wreck happens at once (no fade), no breakouts on their own. */
	void SetInstant(bool bOn) { bInstant = bOn; }
	/** The weather the Gale has put the rig in (None: it left it alone). */
	FName GetWeatherSet() const { return WeatherSet; }

private:
	void SpawnVisuals();
	void UpdateCells(float DeltaTime);
	void ApplyToWorld(float DeltaTime);
	void UpdateLightning(float DeltaTime, const FVector& View);
	void UpdateWreck(float DeltaTime, const FVector& View);
	void FinishWreck();
	FVector ViewLocation() const;

	TArray<FSaltGaleCell> Cells;
	int32 NextId = 1;
	float NextBreakoutIn = 480.f;
	float ViewerStorm = 0.f;
	float StrikeIn = 5.f;
	float HoleIn = 8.f;
	float CoreTime = 0.f;
	float WreckIn = -1.f;
	bool bInstant = false;
	bool bActive = false;
	FName BaseWeather;
	FName WeatherSet;

	UPROPERTY(Transient) TObjectPtr<ASaltGale> Visuals;
	UPROPERTY(Transient) TObjectPtr<ASaltSeaStateZone> WallZone;
	UPROPERTY(Transient) TArray<TObjectPtr<ASaltSeaStateZone>> CellZones;
};

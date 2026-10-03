// Saltbound - camera-following ocean surface with shared Gerstner waves.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ocean/SaltWaveTypes.h"
#include "SaltOcean.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPostProcessComponent;
class UTextureRenderTarget2D;
class UMaterialParameterCollection;
class UStaticMesh;

/**
 * The sea. Owns the wave field used by both rendering and gameplay:
 * - each tick advances ocean time, recomputes wave phases (double precision) and pushes them to
 *   MPC_Ocean; the material evaluates the same waves (SaltOcean.ush), so queries match the visible surface;
 * - renders a CDLOD quadtree of instanced grid tiles around the viewer; vertices morph between LOD
 *   levels in the material, so there are no cracks and no swimming (tiles are world-aligned).
 * The actor's Z is the still-water level. Gameplay queries: SampleSurface() / USaltOceanSubsystem.
 * Boats that query the water should tick after the ocean (AddTickPrerequisiteActor).
 */
UCLASS()
class SALTBOUND_API ASaltOcean : public AActor
{
	GENERATED_BODY()

public:
	ASaltOcean();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return bAnimateInEditor; }

	/** Water surface at world XY for the current ocean time. */
	UFUNCTION(BlueprintPure, Category = "Ocean")
	FSaltWaterSample SampleSurface(FVector2D WorldXY) const;

	UFUNCTION(BlueprintPure, Category = "Ocean")
	float GetWaterLevel() const { return (float)Field.WaterLevel; }

	UFUNCTION(BlueprintPure, Category = "Ocean")
	double GetOceanTime() const { return OceanTime; }

	const FSaltWaveField& GetWaveField() const { return Field; }

	/** Re-reads the wave and calm-zone settings (call after changing them at runtime). */
	UFUNCTION(BlueprintCallable, Category = "Ocean")
	void RebuildWaveField();

	// Waves --------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TArray<FSaltWaveSpec> Waves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	float AmplitudeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	float SteepnessScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	float TimeScale = 1.f;

	/** Stops ocean time (waves and surface detail travel together). For diagnostics and comparisons;
	 *  console: sb.Ocean.Freeze 0|1, sb.Ocean.Time <seconds>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	bool bFreezeTime = false;

	/** Jumps to an ocean time (seconds). The previous-frame phases are reset, so no false motion vectors. */
	UFUNCTION(BlueprintCallable, Category = "Ocean")
	void SetOceanTime(double NewTime);

	/** The first SwellCount waves are swell: they keep their full height everywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", ClampMax = "12"))
	int32 SwellCount = 3;

	/** Medium waves and chop calm down in patches by up to this fraction (0 = uniform sea). Leaves
	 *  quieter wave faces between choppier areas instead of a uniformly crumpled surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float ChopVariation = 0.6f;

	/** Typical size of the calmer / choppier patches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (Units = "cm"))
	float ChopPatchSize = 26000.f;

	/** Up to 4 sheltered areas with reduced waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TArray<FSaltCalmZone> CalmZones;

	// Wind sea (FFT, see SaltOceanFFT.h) ---------------------------------------
	/** Simulated wind sea on top of the Gerstner swells. Off = swells only (e.g. windless flat water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea")
	bool bWindSea = true;

	/** Significant height of the wind sea (4 x standard deviation of the height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (ClampMin = "0", Units = "cm"))
	float WindSeaHeight = 150.f;

	/** Dominant wind-wave length (spectral peak). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (ClampMin = "100", Units = "cm"))
	float WindSeaPeakWavelength = 3500.f;

	/** Direction the wind waves travel towards (degrees, 0 = +X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea")
	float WindDirection = 30.f;

	/** JONSWAP peak enhancement: 1 = mature, broad sea; 3.3 = young sea with a pronounced dominant wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (ClampMin = "1", ClampMax = "7"))
	float WindSeaGamma = 3.3f;

	/** Directional spreading at the peak (higher = longer, more parallel crests) and its floor for shorter
	 *  and longer waves (lower = more crossing directions, short interrupted crests). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (ClampMin = "0"))
	float WindSeaSpread = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (ClampMin = "0"))
	float WindSeaSpreadMin = 2.f;

	/** Waves shorter than this are damped (low wind leaves smooth areas). 0 = no damping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (ClampMin = "0", Units = "cm"))
	float ShortWaveLength = 0.f;

	/** Amplitude per cascade: X waves >= 6 m, Y 0.9-6 m, Z ripples < 0.9 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea")
	FVector CascadeGain = FVector(1.0, 1.0, 1.0);

	/** Crest sharpness (horizontal displacement) per cascade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea")
	FVector CascadeChop = FVector(1.0, 1.0, 0.6);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea")
	int32 WindSeaSeed = 1;

	/** Current wind-sea spectrum (from the properties above). */
	SaltOceanFFT::FSpectrum GetSpectrum() const;

	/** Travel rates of the detail layers in tiles per second (tile sizes live in the material):
	 *  X, Y, Z = unused (legacy ripple layers), W = drift of the quiet patches.
	 *  Offsets are wrapped in double precision on the CPU, so ripples stop with bFreezeTime and never lose
	 *  precision in long sessions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	FVector4 RippleScroll = FVector4(0.1, 0.042, 0.3, 0.039);

	// Rendering ----------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Rendering")
	TObjectPtr<UMaterialParameterCollection> ParameterCollection;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	TObjectPtr<UMaterialInterface> Material;

	/** Grid tile with TileCells x TileCells cells, vertices on integer local coordinates centred at 0. */
	UPROPERTY(EditAnywhere, Category = "Rendering")
	TObjectPtr<UStaticMesh> TileMesh;

	/** Same as TileMesh with half the cells per side (used for quarter nodes). */
	UPROPERTY(EditAnywhere, Category = "Rendering")
	TObjectPtr<UStaticMesh> QuarterTileMesh;

	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (ClampMin = "4"))
	int32 TileCells = 32;

	/** Size of the finest quadtree node. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (Units = "cm"))
	float Lod0NodeSize = 800.f;

	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (ClampMin = "1", ClampMax = "14"))
	int32 LodCount = 12;

	/** View distance covered by LOD 0; doubles per level. Should be >= ~2x Lod0NodeSize. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (Units = "cm"))
	float Lod0Range = 2200.f;

	/** Fraction of each LOD range used to morph into the next level. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float MorphFraction = 0.35f;

	/** Re-select tiles after the viewer moved this far. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (Units = "cm"))
	float ReselectDistance = 150.f;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	bool bAnimateInEditor = true;

	/** FFT output textures per cascade (created by setup_ocean.py, sampled by M_Ocean). */
	UPROPERTY(EditAnywhere, Category = "Rendering|FFT")
	TObjectPtr<UTextureRenderTarget2D> FFTDisp[SaltOceanFFT::NumCascades];

	UPROPERTY(EditAnywhere, Category = "Rendering|FFT")
	TObjectPtr<UTextureRenderTarget2D> FFTDispPrev[SaltOceanFFT::NumCascades];

	UPROPERTY(EditAnywhere, Category = "Rendering|FFT")
	TObjectPtr<UTextureRenderTarget2D> FFTDeriv[SaltOceanFFT::NumCascades];

	UPROPERTY(EditAnywhere, Category = "Rendering|FFT")
	TObjectPtr<UTextureRenderTarget2D> FFTAux[SaltOceanFFT::NumCascades];

	/** Debug: number of drawn tiles after the last selection. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Rendering")
	int32 DebugTileCount = 0;

	// Underwater ---------------------------------------------------------------
	/** The sea seen from below: the same displaced tiles drawn with reversed culling (Snell's window, total
	 *  internal reflection). */
	UPROPERTY(EditAnywhere, Category = "Underwater")
	TObjectPtr<UMaterialInterface> UnderMaterial;

	/** Post-process pass for a submerged camera (fog, light lost with depth). It finds the surface above
	 *  the camera itself on the GPU, so it matches the rendered waves exactly. */
	UPROPERTY(EditAnywhere, Category = "Underwater")
	TObjectPtr<UMaterialInterface> UnderwaterPostMaterial;

	/** Underside and post-process pass are switched on while the camera is below or less than this far
	 *  above the surface (CPU estimate; generous, because the GPU surface also has the shorter waves). */
	UPROPERTY(EditAnywhere, Category = "Underwater", meta = (ClampMin = "0", Units = "cm"))
	float UnderwaterActivationHeight = 500.f;

	/** Camera height above the surface at the camera (negative = under water), CPU estimate, last frame. */
	UFUNCTION(BlueprintPure, Category = "Ocean")
	float GetCameraHeightAboveWater() const { return CameraHeightAboveWater; }

private:
	/** Resolves unset content defaults (SaltContent.h) and assigns meshes and material to the tiles. */
	void ApplyContent();

	struct FTileInstance
	{
		FVector2D Center;
		float CellSize;
		int32 Lod;
	};

	void PushParameters();
	/** Applies the spectrum to the CPU field and queues this frame's FFT simulation. */
	void SimulateWindSea(float DeltaSeconds);
	void UpdateTiles(bool bForce);
	/** Switches the underside tiles and the underwater pass on near or below the surface. */
	void UpdateUnderwater(const FVector& View);
	bool SelectNode(const FVector2D& Center, double Size, int32 Lod, const FVector& View, TArray<FTileInstance>& OutFull, TArray<FTileInstance>& OutQuarter) const;
	/** ZSign -1: the component has a mirrored Z scale (reversed culling), instances compensate. */
	void ApplyTiles(UInstancedStaticMeshComponent* Component, const TArray<FTileInstance>& Tiles, double ZSign = 1.0);
	bool GetViewLocation(FVector& OutLocation) const;
	double RangeOf(int32 Lod) const { return Lod < 0 ? 0.0 : Lod0Range * FMath::Pow(2.0, (double)Lod); }

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> FullTiles;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> QuarterTiles;

	/** Underside copies of the tiles (mirrored Z scale: front faces are culled, the surface is seen from
	 *  below only). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> UnderFullTiles;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> UnderQuarterTiles;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> UnderwaterPost;

	FSaltWaveField Field;
	float CameraHeightAboveWater = 1.0e6f;
	bool bUnderwaterActive = false;
	double LastContentRetry = -1.0e9;
	double OceanTime = 0.0;
	/** Wave phases of the previous frame: the material evaluates last frame's displacement with them
	 *  (PreviousFrameSwitch), so the velocity buffer contains the wave motion and TSR can reproject. */
	float PrevPhases[FSaltWaveField::MaxWaves] = {};
	bool bHavePrevPhases = false;
	FVector LastSelectionView = FVector(1.0e12);
};

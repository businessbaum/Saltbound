// Saltbound - camera-following ocean surface with shared Gerstner waves.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ocean/SaltWaveTypes.h"
#include "SaltOcean.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
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

	/** Up to 4 sheltered areas with reduced waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TArray<FSaltCalmZone> CalmZones;

	/** Scroll speeds (UV per second) of the two detail ripple layers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	FVector4 RippleScroll = FVector4(0.012, 0.004, -0.006, 0.010);

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
	float Lod0NodeSize = 1600.f;

	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (ClampMin = "1", ClampMax = "14"))
	int32 LodCount = 11;

	/** View distance covered by LOD 0; doubles per level. Should be >= ~2x Lod0NodeSize. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (Units = "cm"))
	float Lod0Range = 4000.f;

	/** Fraction of each LOD range used to morph into the next level. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float MorphFraction = 0.35f;

	/** Re-select tiles after the viewer moved this far. */
	UPROPERTY(EditAnywhere, Category = "Rendering", meta = (Units = "cm"))
	float ReselectDistance = 150.f;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	bool bAnimateInEditor = true;

	/** Debug: number of drawn tiles after the last selection. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Rendering")
	int32 DebugTileCount = 0;

private:
	struct FTileInstance
	{
		FVector2D Center;
		float CellSize;
		int32 Lod;
	};

	void PushParameters();
	void UpdateTiles(bool bForce);
	bool SelectNode(const FVector2D& Center, double Size, int32 Lod, const FVector& View, TArray<FTileInstance>& OutFull, TArray<FTileInstance>& OutQuarter) const;
	void ApplyTiles(UInstancedStaticMeshComponent* Component, const TArray<FTileInstance>& Tiles);
	bool GetViewLocation(FVector& OutLocation) const;
	double RangeOf(int32 Lod) const { return Lod < 0 ? 0.0 : Lod0Range * FMath::Pow(2.0, (double)Lod); }

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> FullTiles;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> QuarterTiles;

	FSaltWaveField Field;
	double OceanTime = 0.0;
	FVector LastSelectionView = FVector(1.0e12);
};

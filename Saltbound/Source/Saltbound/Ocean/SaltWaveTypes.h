// Saltbound - shared Gerstner wave model (CPU side).

#pragma once

#include "CoreMinimal.h"
#include "SaltWaveTypes.generated.h"

/** One Gerstner wave component as authored in the editor. */
USTRUCT(BlueprintType)
struct FSaltWaveSpec
{
	GENERATED_BODY()

	/** Direction of travel in degrees (0 = +X, 90 = +Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float DirectionDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "50", Units = "cm"))
	float Wavelength = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0", Units = "cm"))
	float Amplitude = 20.f;

	/** Crest sharpness 0..1 (horizontal amplitude = Steepness / k). Keep the sum over all waves below ~0.9. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0", ClampMax = "1"))
	float Steepness = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (Units = "rad"))
	float PhaseOffset = 0.f;
};

/** Circular area with reduced wave amplitude (sheltered harbour, lee of an island). */
USTRUCT(BlueprintType)
struct FSaltCalmZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calm Zone")
	FVector2D Center = FVector2D::ZeroVector;

	/** Full calming inside this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calm Zone", meta = (Units = "cm"))
	float InnerRadius = 3000.f;

	/** No calming beyond this radius (smooth transition in between). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calm Zone", meta = (Units = "cm"))
	float OuterRadius = 12000.f;

	/** Wave amplitude factor inside the inner radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calm Zone", meta = (ClampMin = "0", ClampMax = "1"))
	float MinFactor = 0.25f;
};

/** Result of a water surface query. */
USTRUCT(BlueprintType)
struct FSaltWaterSample
{
	GENERATED_BODY()

	/** Point on the displaced surface directly at the queried XY. */
	UPROPERTY(BlueprintReadOnly, Category = "Water")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Water")
	FVector Normal = FVector::UpVector;

	/** 1 = undisturbed surface; low values = compressed crest (foam). */
	UPROPERTY(BlueprintReadOnly, Category = "Water")
	float Jacobian = 1.f;
};

/**
 * Evaluates the wave field. Mirrors Saltbound/Shaders/SaltOcean.ush exactly; keep both in sync.
 * Units: centimetres, seconds, radians. Deep-water dispersion: omega = sqrt(g * k).
 */
struct SALTBOUND_API FSaltWaveField
{
	static constexpr int32 MaxWaves = 12;
	static constexpr int32 MaxCalmZones = 4;
	static constexpr double Gravity = 980.665; // cm/s^2

	struct FWave
	{
		FVector2D K = FVector2D::ZeroVector;   // wave vector (rad/cm)
		FVector2D Dir = FVector2D::UnitX();
		float Amplitude = 0.f;
		float Horizontal = 0.f;
		double Omega = 0.0;
		double PhaseOffset = 0.0;
		float Phase = 0.f;                     // wrapped, current time
	};

	TArray<FWave, TInlineAllocator<MaxWaves>> Waves;
	FVector4f Calm[MaxCalmZones];              // x, y, inner, outer
	float CalmMin[MaxCalmZones];
	double WaterLevel = 0.0;

	FSaltWaveField();

	void Build(const TArray<FSaltWaveSpec>& Specs, float AmplitudeScale, float SteepnessScale, const TArray<FSaltCalmZone>& Zones, double InWaterLevel);
	void SetTime(double TimeSeconds);

	float Attenuation(const FVector2D& P) const;
	FVector Displacement(const FVector2D& P0) const;
	void NormalAndJacobian(const FVector2D& P0, FVector& OutNormal, float& OutJacobian) const;

	/** Surface sample at world XY: inverts the horizontal displacement by fixed-point iteration. */
	FSaltWaterSample Sample(const FVector2D& XY, int32 Iterations = 5) const;

	/** Upper bound of vertical displacement (for bounds). */
	float MaxHeight() const;
	float MaxHorizontal() const;
};

// Saltbound - shared Gerstner wave model (CPU side).

#pragma once

#include "CoreMinimal.h"
#include "SaltOceanFFT.h"
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

/**
 * D69: water under a roof (the hub's water hall). The sky light reaches the water's volume as if it were open to the sky
 * (the water's in-scattering is not occluded), so sheltered water glows; inside this box (world XY, turned by Yaw) the
 * material scales its in-scattering and crest light down, fading over EdgeCm at the box's sides.
 */
USTRUCT(BlueprintType)
struct FSaltShelterBox
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter")
	FVector2D Center = FVector2D::ZeroVector;

	/** Half the box's size along its own X and Y (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter")
	FVector2D HalfExtent = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter", meta = (Units = "deg"))
	float Yaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter", meta = (Units = "cm", ClampMin = "1"))
	float EdgeCm = 150.f;

	/** 1 = fully sheltered inside the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter", meta = (ClampMin = "0", ClampMax = "1"))
	float Amount = 1.f;
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
 * Gerstner waves are the long swells; the wind sea comes from the FFT spectrum (SaltOceanFFT), of which
 * the CPU evaluates the long waves (cascade 0, >= ~12 m). Both are scaled by the calm zones; the FFT
 * sea also by the chop patches (calmer and choppier areas).
 */
struct SALTBOUND_API FSaltWaveField
{
	static constexpr int32 MaxWaves = 12;
	static constexpr int32 MaxCalmZones = 8;           // D62: the play map's four and the hub's berth
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

	/** Waves [0, SwellCount) are swell; later Gerstner waves and the FFT sea vary in patches (ChopFactor). */
	int32 SwellCount = 3;
	/** Chop factor in the calmest patches (1 = no variation). */
	float ChopMin = 1.f;
	/** 2 pi / patch size (rad/cm). */
	float ChopFreq = 0.f;

	/** FFT wind sea (long waves only; see SaltOceanFFT::FLowBand). Evaluated lazily per time. */
	bool bUseFFT = false;
	mutable SaltOceanFFT::FLowBand LowBand;
	double Time = 0.0;

	FSaltWaveField();

	void SetSpectrum(const SaltOceanFFT::FSpectrum& Spectrum, bool bEnable);
	void Build(const TArray<FSaltWaveSpec>& Specs, float AmplitudeScale, float SteepnessScale, const TArray<FSaltCalmZone>& Zones, double InWaterLevel);
	void SetTime(double TimeSeconds);

	float Attenuation(const FVector2D& P) const;
	/** Smooth, static patches of calmer and choppier water (0..1 noise mapped to [ChopMin, 1]). */
	float ChopFactor(const FVector2D& P) const;
	void SetChop(int32 InSwellCount, float Variation, float PatchSize);
	FVector Displacement(const FVector2D& P0) const;
	void NormalAndJacobian(const FVector2D& P0, FVector& OutNormal, float& OutJacobian) const;

	/** Surface sample at world XY: inverts the horizontal displacement by fixed-point iteration. */
	FSaltWaterSample Sample(const FVector2D& XY, int32 Iterations = 5) const;

	/** Upper bound of vertical displacement (for bounds). */
	float MaxHeight() const;
	float MaxHorizontal() const;
};

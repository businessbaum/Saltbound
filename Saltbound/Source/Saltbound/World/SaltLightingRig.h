// Saltbound - lighting presets (day / evening ...) applied to the scene's sun, sky, fog and exposure.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltLightingRig.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class ASkyAtmosphere;
class APostProcessVolume;

USTRUCT(BlueprintType)
struct FSaltLightingPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FName Name = TEXT("Day");

	/** Sun elevation above the horizon (degrees) and compass direction it shines towards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunElevation = 38.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunYaw = -60.f;
	/** Physical sun illuminance; the sky atmosphere attenuates and colours it near the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (Units = "lux")) float SunIlluminance = 90000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (Units = "K")) float SunTemperature = 5800.f;
	/** Apparent sun diameter. Larger than the real 0.53 deg gives the big, soft sun reflection on the sea
	 *  (Sea of Thieves uses an area highlight for this) and softer shadow edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (Units = "deg")) float SunSourceAngle = 0.5357f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float SkyLightIntensity = 1.f;
	/** Tint of the sky light: cool shadows against the golden sun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") FLinearColor SkyLightTint = FLinearColor::White;
	/** Atmosphere: Rayleigh scale (sky blue depth) and Mie scale (whitish haze near the horizon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float RayleighScale = 0.0331f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float MieScale = 0.003996f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogDensity = 0.012f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogHeightFalloff = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogInscattering = FLinearColor(0.42f, 0.5f, 0.56f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogDirectionalInscattering = FLinearColor(0.6f, 0.55f, 0.45f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogStartDistance = 1500.f;

	/** Auto-exposure range (EV100) and bias: keeps evenings darker than days. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureMinEV = 11.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureMaxEV = 15.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureBias = 0.f;

	/** Global grading: saturation and a gentle colour cast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") float Saturation = 0.95f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") FLinearColor Gain = FLinearColor(1.f, 1.f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") float Contrast = 1.f;
	/** Split toning: colour of the shadows and of the highlights (multiplied in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") FLinearColor ShadowGain = FLinearColor(1.f, 1.f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") FLinearColor HighlightGain = FLinearColor(1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props") bool bLanternsLit = false;
};

/**
 * Finds the level's sun / sky light / height fog / unbound post-process volume (or uses the assigned ones)
 * and applies a named preset. Console: sb.Lighting <PresetName>.
 */
UCLASS()
class SALTBOUND_API ASaltLightingRig : public AActor
{
	GENERATED_BODY()

public:
	ASaltLightingRig();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Lighting")
	bool ApplyPreset(FName PresetName);

	UFUNCTION(CallInEditor, Category = "Lighting")
	void ApplyDay() { ApplyPreset(TEXT("Day")); }

	UFUNCTION(CallInEditor, Category = "Lighting")
	void ApplyEvening() { ApplyPreset(TEXT("Evening")); }

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	TArray<FSaltLightingPreset> Presets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	FName StartPreset = TEXT("Day");

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Lighting")
	FName CurrentPreset;

	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ASkyLight> SkyLight;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<AExponentialHeightFog> Fog;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<APostProcessVolume> PostProcess;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ASkyAtmosphere> Atmosphere;

private:
	void ResolveSceneActors();
};

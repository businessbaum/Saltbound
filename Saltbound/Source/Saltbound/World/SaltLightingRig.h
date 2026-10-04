// Saltbound - environment rig: time of day, region, weather and sea state, applied to sun, sky, fog, clouds,
// post-processing and the ocean.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltLightingRig.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ASkyAtmosphere;
class ASaltOcean;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UVolumetricCloudComponent;

/** Time of day: sun position and the base light, sky, fog and exposure. */
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
 * A sea region's palette (D25). Water values are absolute; atmosphere, light and grading values multiply
 * the time-of-day preset, so every region works at every time of day. Reference directions, not fixed
 * colours: Shores of Plenty, Ancient Isles, The Wilds, The Devil's Roar (Sea of Thieves).
 */
USTRUCT(BlueprintType)
struct FSaltRegionPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") FName Name = TEXT("ShoresOfPlenty");

	/** Single Layer Water volume (1/m): absorption sets the deep colour, scattering the body colour and
	 *  how bright shallows become over light ground. */
	// Defaults = Shores of Plenty (D27, D30 a little greener): blue-green body and deep troughs, turquoise only where light passes
	// through thin water.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor WaterAbsorption = FLinearColor(1.35f, 0.26f, 0.15f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor WaterScattering = FLinearColor(0.01f, 0.072f, 0.095f);
	/** Light passing through thin, backlit crests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor CrestLightColor = FLinearColor(0.1f, 0.95f, 0.9f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") float CrestLightStrength = 1.f;

	/** Rayleigh scattering colour (absolute; hue of the clear sky) and scale/haze multipliers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") FLinearColor RayleighColor = FLinearColor(0.175287f, 0.409607f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float RayleighScaleMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float MieScaleMul = 1.f;
	/** Mie absorption (absolute): smoke and ash darken the sky. Engine default 0.000444. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float MieAbsorptionScale = 0.000444f;
	/** Overall sky and aerial-perspective brightness tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") FLinearColor SkyLuminance = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogDensityMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogDirectionalTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") FLinearColor SunTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") float SunIntensityMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") FLinearColor SkyLightTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") float SkyLightMul = 1.f;

	/** Volumetric cloud coverage (material parameter, -1 none .. 1 full) and albedo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") float CloudCoverage = -0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds") FLinearColor CloudAlbedo = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") float SaturationMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") float ContrastMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") FLinearColor GainMul = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grading") FLinearColor ShadowGainMul = FLinearColor::White;
};

/** Weather on top of time of day and region: clouds, light and haze. */
USTRUCT(BlueprintType)
struct FSaltWeatherPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") FName Name = TEXT("Clear");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float CloudCoverageOffset = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float CloudAlbedoMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float SunIntensityMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float SkyLightMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float FogDensityMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float SaturationMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float ContrastMul = 1.f;
};

/** Sea state: swell, wind sea (FFT spectrum) and the balance of medium waves and ripples. Each state has its own wave
 *  balance (D27): low wind is not the rough sea scaled down. */
USTRUCT(BlueprintType)
struct FSaltSeaStatePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") FName Name = TEXT("Rough");
	/** Swell (Gerstner): ocean AmplitudeScale / SteepnessScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float WaveHeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float WaveSteepness = 1.f;
	/** Wind sea (FFT): significant height and dominant wavelength (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (Units = "cm")) float WindSeaHeight = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (Units = "cm")) float WindSeaWavelength = 2600.f;
	/** Peak enhancement: higher = one dominant wave size, more distinctive major ridges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea") float WindSeaGamma = 5.f;
	/** Directional spreading at the peak and its floor (lower = shorter, more interrupted crests). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea") float WindSeaSpread = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea") float WindSeaSpreadMin = 1.5f;
	/** Crest sharpness of the wind sea (scales the ocean's CascadeChop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea") float Choppiness = 1.f;
	/** Waves shorter than this are damped (0 = none): smooth areas between ripples in low wind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Sea", meta = (Units = "cm")) float ShortWaveLength = 0.f;
	/** Ocean ChopVariation: how much calmer the quiet patches are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float ChopVariation = 0.6f;
	/** Medium waves (0.9-6 m) and ripples (< 0.9 m) relative to the default balance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float MediumWaveScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float RippleScale = 1.f;
};

/**
 * Finds the level's sun / sky light / sky atmosphere / height fog / unbound post-process volume / ocean
 * (or uses the assigned ones) and composes four independent layers:
 *   time of day (sb.Lighting), region (sb.Region), weather (sb.Weather), sea state (sb.SeaState).
 * Region, weather and sea state blend smoothly over time; ASaltRegionZone actors blend regions by the
 * viewer's position. The rig owns the volumetric clouds (a transient component with a dynamic material),
 * so weather and region can change them without modifying assets. Water look parameters go to the
 * ocean's material parameter collection (MPC_Ocean: Look*).
 */
UCLASS()
class SALTBOUND_API ASaltLightingRig : public AActor
{
	GENERATED_BODY()

public:
	ASaltLightingRig();

	/** Time of day. Re-applies all layers. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Environment")
	bool ApplyPreset(FName PresetName);

	UFUNCTION(CallInEditor, Category = "Environment")
	void ApplyDay() { ApplyPreset(TEXT("Day")); }

	UFUNCTION(CallInEditor, Category = "Environment")
	void ApplyEvening() { ApplyPreset(TEXT("Evening")); }

	UFUNCTION(BlueprintCallable, Category = "Environment")
	bool SetRegion(FName RegionName, float BlendSeconds = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Environment")
	bool SetWeather(FName WeatherName, float BlendSeconds = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Environment")
	bool SetSeaState(FName SeaStateName, float BlendSeconds = 0.f);

	/** Recomposes all layers and pushes them to the scene. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Environment")
	void ApplyEnvironment();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	TArray<FSaltLightingPreset> Presets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	TArray<FSaltRegionPreset> Regions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	TArray<FSaltWeatherPreset> Weathers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	TArray<FSaltSeaStatePreset> SeaStates;

	/** Selections applied at BeginPlay (and in the editor when the rig is constructed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FName StartPreset = TEXT("Day");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FName StartRegion = TEXT("ShoresOfPlenty");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FName StartWeather = TEXT("Clear");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FName StartSeaState = TEXT("Rough");

	/** Base material of the rig's volumetric clouds (a dynamic instance is created from it). */
	UPROPERTY(EditAnywhere, Category = "Environment")
	TObjectPtr<UMaterialInterface> CloudMaterial;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Environment") FName CurrentPreset;
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Environment") FName CurrentRegion;
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Environment") FName CurrentWeather;
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Environment") FName CurrentSeaState;

	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ASkyLight> SkyLight;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<AExponentialHeightFog> Fog;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<APostProcessVolume> PostProcess;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ASkyAtmosphere> Atmosphere;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<ASaltOcean> Ocean;

	static FSaltRegionPreset LerpRegion(const FSaltRegionPreset& A, const FSaltRegionPreset& B, float T);
	static FSaltWeatherPreset LerpWeather(const FSaltWeatherPreset& A, const FSaltWeatherPreset& B, float T);
	static FSaltSeaStatePreset LerpSeaState(const FSaltSeaStatePreset& A, const FSaltSeaStatePreset& B, float T);

	/** One line describing the current layers and blend state (console: sb.Env). */
	FString DescribeEnvironment() const;

private:
	template <typename T>
	struct FLayer
	{
		T From;
		T To;
		float Alpha = 1.f;
		float Duration = 0.f;
		bool IsBlending() const { return Alpha < 1.f; }
	};

	void ResolveSceneActors();
	void EnsureClouds();
	FSaltRegionPreset ComposeRegion(float* OutZoneWeightSum) const;
	/** Sea state layer blended with ASaltSeaStateZone actors by the viewer's position. */
	FSaltSeaStatePreset ComposeSeaState(float* OutZoneWeightSum) const;
	void PushEnvironment(const FSaltLightingPreset& P, const FSaltRegionPreset& R, const FSaltWeatherPreset& W, const FSaltSeaStatePreset& S);
	void InitLayersIfNeeded();

	FLayer<FSaltRegionPreset> RegionLayer;
	FLayer<FSaltWeatherPreset> WeatherLayer;
	FLayer<FSaltSeaStatePreset> SeaLayer;
	bool bLayersInitialised = false;
	float LastZoneWeightSum = -1.f;
	float LastSeaZoneWeightSum = -1.f;
	FSaltSeaStatePreset LastAppliedSea;
	bool bHaveAppliedSea = false;

	UPROPERTY(Transient) TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudMID;
};

/**
 * Area with its own sea state (e.g. a storm area): inside InnerRadius the sea state fully applies, it fades
 * out to the rig's sea state at OuterRadius (horizontal distance from the viewer). The ocean is one global
 * simulation, so the whole visible sea changes with the viewer's position (like ASaltRegionZone).
 */
UCLASS()
class SALTBOUND_API ASaltSeaStateZone : public AActor
{
	GENERATED_BODY()

public:
	ASaltSeaStateZone();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") FName SeaState = TEXT("Storm");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea", meta = (Units = "cm")) float InnerRadius = 50000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea", meta = (Units = "cm")) float OuterRadius = 150000.f;

	/** 0..1 influence at a world position. */
	float WeightAt(const FVector& WorldLocation) const;
};

/**
 * Sphere of influence of a region: inside InnerRadius the region fully applies, it fades out to the
 * default region at OuterRadius (horizontal distance from the viewer).
 */
UCLASS()
class SALTBOUND_API ASaltRegionZone : public AActor
{
	GENERATED_BODY()

public:
	ASaltRegionZone();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") FName Region = TEXT("AncientIsles");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (Units = "cm")) float InnerRadius = 100000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (Units = "cm")) float OuterRadius = 250000.f;

	/** 0..1 influence at a world position. */
	float WeightAt(const FVector& WorldLocation) const;
};

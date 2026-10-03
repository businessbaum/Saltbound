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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor WaterAbsorption = FLinearColor(1.3f, 0.3f, 0.22f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor WaterScattering = FLinearColor(0.008f, 0.05f, 0.07f);
	/** Light passing through thin, backlit crests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor CrestLightColor = FLinearColor(0.15f, 0.95f, 0.85f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") float CrestLightStrength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water") FLinearColor FoamColor = FLinearColor(0.85f, 0.92f, 0.92f);

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

/** Sea state: wave height and character, whitecaps, ripples and spray. */
USTRUCT(BlueprintType)
struct FSaltSeaStatePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") FName Name = TEXT("Rough");
	/** Ocean AmplitudeScale / SteepnessScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float WaveHeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float WaveSteepness = 1.f;
	/** Ocean ChopVariation: how much calmer the quiet patches are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float ChopVariation = 0.6f;
	/** Crests break where the surface Jacobian drops below this (higher = more whitecaps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float WhitecapStart = 0.6f;
	/** Whitecap coverage scale (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float WhitecapAmount = 1.f;
	/** Higher values leave more breaking-capable crests unbroken (patchier whitecaps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float BreakPatchiness = 2.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float RippleScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sea") float SprayAmount = 0.5f;
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
	void PushEnvironment(const FSaltLightingPreset& P, const FSaltRegionPreset& R, const FSaltWeatherPreset& W, const FSaltSeaStatePreset& S);
	void InitLayersIfNeeded();

	FLayer<FSaltRegionPreset> RegionLayer;
	FLayer<FSaltWeatherPreset> WeatherLayer;
	FLayer<FSaltSeaStatePreset> SeaLayer;
	bool bLayersInitialised = false;
	float LastZoneWeightSum = -1.f;
	FSaltSeaStatePreset LastAppliedSea;
	bool bHaveAppliedSea = false;

	UPROPERTY(Transient) TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudMID;
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

// Saltbound - environment rig: time of day, region, weather and sea state, applied to sun, sky, fog, clouds,
// post-processing and the ocean.

#include "World/SaltLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Core/SaltContent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Ocean/SaltOcean.h"
#include "Props/SaltLantern.h"
#include "Saltbound.h"

namespace
{
	template <typename Fn>
	void ForEachRig(UWorld* World, Fn&& F)
	{
		if (World)
		{
			for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
			{
				F(**It);
			}
		}
	}

	float BlendArg(const TArray<FString>& Args)
	{
		return Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdLighting(
		TEXT("sb.Lighting"),
		TEXT("Time of day: sb.Lighting Day | Evening | Dusk"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0) { ForEachRig(World, [&](ASaltLightingRig& R) { R.ApplyPreset(FName(*Args[0])); }); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdRegion(
		TEXT("sb.Region"),
		TEXT("Sea region: sb.Region ShoresOfPlenty | AncientIsles | Wilds | DevilsRoar [BlendSeconds]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0) { ForEachRig(World, [&](ASaltLightingRig& R) { R.SetRegion(FName(*Args[0]), BlendArg(Args)); }); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWeather(
		TEXT("sb.Weather"),
		TEXT("Weather: sb.Weather Clear | Cloudy | Overcast | Storm [BlendSeconds]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0) { ForEachRig(World, [&](ASaltLightingRig& R) { R.SetWeather(FName(*Args[0]), BlendArg(Args)); }); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSeaState(
		TEXT("sb.SeaState"),
		TEXT("Sea state: sb.SeaState Flat | Calm | Moderate | Rough | Storm [BlendSeconds] (Flat = windless, no swell)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0) { ForEachRig(World, [&](ASaltLightingRig& R) { R.SetSeaState(FName(*Args[0]), BlendArg(Args)); }); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdEnv(
		TEXT("sb.Env"),
		TEXT("Print the environment layers (time of day, region, weather, sea state)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachRig(World, [&](ASaltLightingRig& R) { UE_LOG(LogSaltbound, Display, TEXT("SB_ENV %s"), *R.DescribeEnvironment()); });
		}));

	FLinearColor Mul(const FLinearColor& A, const FLinearColor& B) { return FLinearColor(A.R * B.R, A.G * B.G, A.B * B.B, A.A); }
	float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }

	const FName PAbsorption(TEXT("LookAbsorption"));
	const FName PScattering(TEXT("LookScattering"));
	const FName PCrest(TEXT("LookCrest"));
}

ASaltLightingRig::ASaltLightingRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// --- Time of day.
	// Stylised realism (owner reference: Sea of Thieves): golden sun with a large disc (big soft sun
	// reflection on the sea), cool sky fill so shadows read blue, a deep blue sky with little white haze,
	// blue distance haze that fades far islands into the sky, rich but controlled colour (saturation and
	// contrast up a little, cool shadows, warm highlights).
	FSaltLightingPreset Day;
	Day.Name = TEXT("Day");
	Day.SunElevation = 32.f;
	Day.SunTemperature = 5400.f;   // golden, but sun glints still read white
	// D30: 2.2 deg made large round glare spots on the waves; a smaller sun breaks into strokes along them.
	Day.SunSourceAngle = 1.3f;
	Day.SkyLightIntensity = 1.15f;
	Day.SkyLightTint = FLinearColor(0.8f, 0.92f, 1.15f);
	Day.RayleighScale = 0.045f;
	Day.MieScale = 0.0018f;
	// Low-lying haze: thin near sea level (keeps the sky blue overhead), fades far islands into the horizon.
	Day.FogDensity = 0.01f;
	Day.FogHeightFalloff = 0.2f;
	Day.FogInscattering = FLinearColor(0.35f, 0.55f, 0.85f);
	Day.FogDirectionalInscattering = FLinearColor(0.75f, 0.62f, 0.42f);
	Day.FogStartDistance = 4000.f;
	Day.Saturation = 1.12f;
	Day.Contrast = 1.08f;
	Day.ShadowGain = FLinearColor(0.9f, 0.96f, 1.1f);
	Day.HighlightGain = FLinearColor(1.06f, 1.02f, 0.94f);
	Presets.Add(Day);

	// Low sun just above the horizon: the atmosphere turns it amber; exposure kept lower so the
	// scene reads as evening, warm lanterns become the local light sources.
	FSaltLightingPreset Evening;
	Evening.Name = TEXT("Evening");
	Evening.SunElevation = 4.f;
	Evening.SunYaw = 160.f;
	Evening.SunTemperature = 5600.f;
	Evening.SkyLightIntensity = 0.8f;
	Evening.FogDensity = 0.025f;
	Evening.FogHeightFalloff = 0.09f;
	Evening.FogInscattering = FLinearColor(0.16f, 0.19f, 0.26f);
	Evening.FogDirectionalInscattering = FLinearColor(0.9f, 0.48f, 0.28f);
	Evening.FogStartDistance = 500.f;
	Evening.ExposureMinEV = 9.5f;
	Evening.ExposureMaxEV = 12.5f;
	Evening.ExposureBias = -0.7f;
	Evening.Saturation = 0.98f;
	Evening.Gain = FLinearColor(0.97f, 0.98f, 1.05f);
	Evening.SunSourceAngle = 2.0f;
	Evening.SkyLightTint = FLinearColor(0.82f, 0.9f, 1.15f);
	Evening.Contrast = 1.06f;
	Evening.ShadowGain = FLinearColor(0.88f, 0.94f, 1.12f);
	Evening.HighlightGain = FLinearColor(1.08f, 1.0f, 0.9f);
	Evening.bLanternsLit = true;
	Presets.Add(Evening);

	// Blue hour: sun just below the horizon (the atmosphere still lights the sky), lanterns carry the scene.
	FSaltLightingPreset Dusk;
	Dusk.Name = TEXT("Dusk");
	Dusk.SunElevation = -3.f;
	Dusk.SunYaw = 160.f;
	Dusk.SunTemperature = 6000.f;
	Dusk.SkyLightIntensity = 1.0f;
	Dusk.FogDensity = 0.012f;
	Dusk.FogHeightFalloff = 0.1f;
	Dusk.FogInscattering = FLinearColor(0.08f, 0.1f, 0.16f);
	Dusk.FogDirectionalInscattering = FLinearColor(0.12f, 0.08f, 0.07f);
	Dusk.FogStartDistance = 600.f;
	Dusk.ExposureMinEV = 5.0f;
	Dusk.ExposureMaxEV = 8.0f;
	Dusk.ExposureBias = -1.0f;
	Dusk.Saturation = 0.95f;
	Dusk.Contrast = 1.04f;
	Dusk.ShadowGain = FLinearColor(0.92f, 0.95f, 1.08f);
	Dusk.Gain = FLinearColor(0.96f, 0.98f, 1.06f);
	Dusk.bLanternsLit = true;
	Presets.Add(Dusk);

	// --- Regions (D25). Reference directions from the Sea of Thieves regions; water values are absolute,
	// atmosphere / light / grading multiply the time-of-day preset.
	FSaltRegionPreset Shores;          // bright tropical blues, turquoise shallows, clear sunny reflections
	Shores.Name = TEXT("ShoresOfPlenty");
	Regions.Add(Shores);

	FSaltRegionPreset Ancient;         // richer blue and teal water, lush humid tropical atmosphere
	Ancient.Name = TEXT("AncientIsles");
	Ancient.WaterAbsorption = FLinearColor(1.45f, 0.24f, 0.17f);
	Ancient.WaterScattering = FLinearColor(0.007f, 0.058f, 0.068f);
	Ancient.CrestLightColor = FLinearColor(0.1f, 0.92f, 0.68f);
	Ancient.MieScaleMul = 1.8f;
	Ancient.FogDensityMul = 1.5f;
	Ancient.FogTint = FLinearColor(0.88f, 1.05f, 0.95f);
	Ancient.FogDirectionalTint = FLinearColor(1.12f, 1.0f, 0.82f);
	Ancient.SunTint = FLinearColor(1.f, 0.96f, 0.88f);
	Ancient.SkyLightTint = FLinearColor(0.95f, 1.03f, 0.95f);
	Ancient.CloudCoverage = -0.22f;
	Ancient.SaturationMul = 1.08f;
	Ancient.GainMul = FLinearColor(1.f, 1.02f, 0.98f);
	Regions.Add(Ancient);

	FSaltRegionPreset Wilds;           // muted, murkier green-grey water, cloudier light, clear surface shading
	Wilds.Name = TEXT("Wilds");
	Wilds.WaterAbsorption = FLinearColor(0.85f, 0.3f, 0.42f);
	Wilds.WaterScattering = FLinearColor(0.022f, 0.036f, 0.028f);
	Wilds.CrestLightColor = FLinearColor(0.35f, 0.62f, 0.42f);
	Wilds.CrestLightStrength = 0.45f;
	Wilds.RayleighScaleMul = 0.7f;
	Wilds.MieScaleMul = 2.6f;
	Wilds.SkyLuminance = FLinearColor(0.86f, 0.9f, 0.86f);
	Wilds.FogDensityMul = 2.6f;
	Wilds.FogTint = FLinearColor(0.72f, 0.8f, 0.72f);
	Wilds.FogDirectionalTint = FLinearColor(0.6f, 0.62f, 0.55f);
	Wilds.SunTint = FLinearColor(0.95f, 0.97f, 1.f);
	Wilds.SunIntensityMul = 0.55f;
	Wilds.SkyLightTint = FLinearColor(0.95f, 1.f, 0.95f);
	Wilds.CloudCoverage = -0.02f;
	Wilds.CloudAlbedo = FLinearColor(0.8f, 0.82f, 0.82f);
	Wilds.SaturationMul = 0.78f;
	Wilds.ContrastMul = 0.98f;
	Wilds.ShadowGainMul = FLinearColor(0.96f, 1.f, 0.96f);
	Regions.Add(Wilds);

	FSaltRegionPreset Roar;            // dark volcanic atmosphere: ash sky, warm glow; the water stays water
	Roar.Name = TEXT("DevilsRoar");
	Roar.WaterAbsorption = FLinearColor(1.1f, 0.42f, 0.32f);
	Roar.WaterScattering = FLinearColor(0.01f, 0.02f, 0.027f);
	Roar.CrestLightColor = FLinearColor(0.25f, 0.55f, 0.55f);
	Roar.CrestLightStrength = 0.45f;
	Roar.RayleighScaleMul = 0.5f;
	Roar.MieScaleMul = 4.f;
	Roar.MieAbsorptionScale = 0.004f;
	Roar.SkyLuminance = FLinearColor(0.55f, 0.42f, 0.36f);
	Roar.FogDensityMul = 3.f;
	Roar.FogTint = FLinearColor(0.45f, 0.32f, 0.28f);
	Roar.FogDirectionalTint = FLinearColor(1.4f, 0.55f, 0.25f);
	Roar.SunTint = FLinearColor(1.f, 0.72f, 0.52f);
	Roar.SunIntensityMul = 0.45f;
	Roar.SkyLightTint = FLinearColor(0.9f, 0.75f, 0.68f);
	Roar.SkyLightMul = 0.8f;
	Roar.CloudCoverage = 0.05f;
	Roar.CloudAlbedo = FLinearColor(0.42f, 0.38f, 0.36f);
	Roar.SaturationMul = 0.92f;
	Roar.ContrastMul = 1.08f;
	Roar.GainMul = FLinearColor(1.03f, 0.98f, 0.94f);
	Roar.ShadowGainMul = FLinearColor(0.9f, 0.88f, 0.9f);
	Regions.Add(Roar);

	// --- Weather.
	FSaltWeatherPreset Clear;
	Clear.Name = TEXT("Clear");
	Weathers.Add(Clear);
	FSaltWeatherPreset Cloudy;
	Cloudy.Name = TEXT("Cloudy");
	Cloudy.CloudCoverageOffset = 0.12f;
	Cloudy.SunIntensityMul = 0.85f;
	Weathers.Add(Cloudy);
	FSaltWeatherPreset Overcast;
	Overcast.Name = TEXT("Overcast");
	Overcast.CloudCoverageOffset = 0.4f;
	Overcast.CloudAlbedoMul = 0.85f;
	Overcast.SunIntensityMul = 0.3f;
	Overcast.SkyLightMul = 0.85f;
	Overcast.FogDensityMul = 1.5f;
	Overcast.SaturationMul = 0.85f;
	Overcast.ContrastMul = 0.95f;
	Weathers.Add(Overcast);
	FSaltWeatherPreset Storm;
	Storm.Name = TEXT("Storm");
	Storm.CloudCoverageOffset = 0.55f;
	Storm.CloudAlbedoMul = 0.5f;
	Storm.SunIntensityMul = 0.12f;
	Storm.SkyLightMul = 0.55f;
	Storm.FogDensityMul = 2.2f;
	Storm.SaturationMul = 0.72f;
	Weathers.Add(Storm);

	// --- Sea states (D27; owner references: Sea of Thieves rough sea and calm water beside a ship).
	// Swell: H height scale, St steepness, chop-patch variation. Wind sea: Hs (cm), peak wavelength (cm),
	// gamma, spreading at the peak and its floor, choppiness, short-wave damping (cm). Med / Rip: medium-wave
	// and ripple balance.
	auto Sea = [this](const TCHAR* Name, float H, float St, float Chop, float Hs, float Lp, float Gamma, float Spread, float SpreadMin,
		float Choppy, float ShortWave, float Med, float Rip)
	{
		FSaltSeaStatePreset S;
		S.Name = Name; S.WaveHeight = H; S.WaveSteepness = St; S.ChopVariation = Chop;
		S.WindSeaHeight = Hs; S.WindSeaWavelength = Lp; S.WindSeaGamma = Gamma; S.WindSeaSpread = Spread; S.WindSeaSpreadMin = SpreadMin;
		S.Choppiness = Choppy; S.ShortWaveLength = ShortWave; S.MediumWaveScale = Med; S.RippleScale = Rip;
		SeaStates.Add(S);
	};
	//        name         swell H steep  chop   Hs     peak    gamma spread min  choppy short med   rip
	// Windless biome: glassy water, no swell; only a faint breath on the surface.
	Sea(TEXT("Flat"),     0.f,   0.f,   0.f,   1.5f,  150.f,  3.3f, 2.f, 1.f,  0.2f,  0.f, 1.f,  1.f);
	// Low wind (owner's calm reference): low rounded swell, soft overlapping wind waves of a few metres that
	// break the sun into a glitter path, smooth areas between them.
	Sea(TEXT("Calm"),     0.6f,  0.4f,  0.5f,  55.f,  500.f,  3.3f, 3.f, 1.5f, 0.55f, 0.f, 1.4f, 2.5f);
	Sea(TEXT("Moderate"), 0.6f,  0.8f,  0.55f, 130.f, 1800.f, 4.f,  6.f, 2.5f, 1.f,   0.f, 1.1f, 1.3f);
	// Rough (owner's rough reference): substantial swell carrying steep, short-crested wind waves with
	// distinctive major ridges (peaky spectrum, sharp crests) rather than uniformly tall waves. D30 (owner
	// brief): less broad rounded swell, narrower and more varied crests (choppier, more crossing directions),
	// stronger medium waves across the faces so highlights break into strokes. D32 (owner brief): one
	// dominant direction (narrow spreading, no equal crossing waves forming pointed peaks), broader crests
	// (less chop), broad swell restored under moderate medium waves. D33: a broader spectrum (gamma 3.3) varies
	// the crest spacing and slightly wider spreading at the peak varies the crest length, so the wave field
	// does not repeat as uniform bowls; the floor of the spreading stays high (no strongly crossing waves).
	Sea(TEXT("Rough"),    0.8f,  1.f,   0.5f,  280.f, 2600.f, 3.3f, 8.f, 3.5f, 1.1f,  0.f, 1.15f, 1.f);
	// Storm (D30: "way bigger waves", used by the storm area): tall long swell and a big, long wind sea.
	Sea(TEXT("Storm"),    1.8f,  1.f,   0.35f, 650.f, 6000.f, 4.f,  5.f, 2.f,  1.25f, 0.f, 1.3f, 1.f);}

// --- Layers ---------------------------------------------------------------------------------------

FSaltRegionPreset ASaltLightingRig::LerpRegion(const FSaltRegionPreset& A, const FSaltRegionPreset& B, float T)
{
	FSaltRegionPreset R = T < 0.5f ? A : B;
	auto L = [T](float X, float Y) { return FMath::Lerp(X, Y, T); };
	auto C = [T](const FLinearColor& X, const FLinearColor& Y) { return X + (Y - X) * T; };
	R.WaterAbsorption = C(A.WaterAbsorption, B.WaterAbsorption);
	R.WaterScattering = C(A.WaterScattering, B.WaterScattering);
	R.CrestLightColor = C(A.CrestLightColor, B.CrestLightColor);
	R.CrestLightStrength = L(A.CrestLightStrength, B.CrestLightStrength);
	R.RayleighColor = C(A.RayleighColor, B.RayleighColor);
	R.RayleighScaleMul = L(A.RayleighScaleMul, B.RayleighScaleMul);
	R.MieScaleMul = L(A.MieScaleMul, B.MieScaleMul);
	R.MieAbsorptionScale = L(A.MieAbsorptionScale, B.MieAbsorptionScale);
	R.SkyLuminance = C(A.SkyLuminance, B.SkyLuminance);
	R.FogDensityMul = L(A.FogDensityMul, B.FogDensityMul);
	R.FogTint = C(A.FogTint, B.FogTint);
	R.FogDirectionalTint = C(A.FogDirectionalTint, B.FogDirectionalTint);
	R.SunTint = C(A.SunTint, B.SunTint);
	R.SunIntensityMul = L(A.SunIntensityMul, B.SunIntensityMul);
	R.SkyLightTint = C(A.SkyLightTint, B.SkyLightTint);
	R.SkyLightMul = L(A.SkyLightMul, B.SkyLightMul);
	R.CloudCoverage = L(A.CloudCoverage, B.CloudCoverage);
	R.CloudAlbedo = C(A.CloudAlbedo, B.CloudAlbedo);
	R.SaturationMul = L(A.SaturationMul, B.SaturationMul);
	R.ContrastMul = L(A.ContrastMul, B.ContrastMul);
	R.GainMul = C(A.GainMul, B.GainMul);
	R.ShadowGainMul = C(A.ShadowGainMul, B.ShadowGainMul);
	return R;
}

FSaltWeatherPreset ASaltLightingRig::LerpWeather(const FSaltWeatherPreset& A, const FSaltWeatherPreset& B, float T)
{
	FSaltWeatherPreset R = T < 0.5f ? A : B;
	R.CloudCoverageOffset = FMath::Lerp(A.CloudCoverageOffset, B.CloudCoverageOffset, T);
	R.CloudAlbedoMul = FMath::Lerp(A.CloudAlbedoMul, B.CloudAlbedoMul, T);
	R.SunIntensityMul = FMath::Lerp(A.SunIntensityMul, B.SunIntensityMul, T);
	R.SkyLightMul = FMath::Lerp(A.SkyLightMul, B.SkyLightMul, T);
	R.FogDensityMul = FMath::Lerp(A.FogDensityMul, B.FogDensityMul, T);
	R.SaturationMul = FMath::Lerp(A.SaturationMul, B.SaturationMul, T);
	R.ContrastMul = FMath::Lerp(A.ContrastMul, B.ContrastMul, T);
	return R;
}

FSaltSeaStatePreset ASaltLightingRig::LerpSeaState(const FSaltSeaStatePreset& A, const FSaltSeaStatePreset& B, float T)
{
	FSaltSeaStatePreset R = T < 0.5f ? A : B;
	R.WaveHeight = FMath::Lerp(A.WaveHeight, B.WaveHeight, T);
	R.WaveSteepness = FMath::Lerp(A.WaveSteepness, B.WaveSteepness, T);
	R.ChopVariation = FMath::Lerp(A.ChopVariation, B.ChopVariation, T);
	R.MediumWaveScale = FMath::Lerp(A.MediumWaveScale, B.MediumWaveScale, T);
	R.RippleScale = FMath::Lerp(A.RippleScale, B.RippleScale, T);
	R.WindSeaHeight = FMath::Lerp(A.WindSeaHeight, B.WindSeaHeight, T);
	R.WindSeaWavelength = FMath::Lerp(A.WindSeaWavelength, B.WindSeaWavelength, T);
	R.WindSeaGamma = FMath::Lerp(A.WindSeaGamma, B.WindSeaGamma, T);
	R.WindSeaSpread = FMath::Lerp(A.WindSeaSpread, B.WindSeaSpread, T);
	R.WindSeaSpreadMin = FMath::Lerp(A.WindSeaSpreadMin, B.WindSeaSpreadMin, T);
	R.Choppiness = FMath::Lerp(A.Choppiness, B.Choppiness, T);
	R.ShortWaveLength = FMath::Lerp(A.ShortWaveLength, B.ShortWaveLength, T);
	return R;
}

namespace
{
	template <typename T>
	T FindPresetOr(const TArray<T>& Array, FName Name)
	{
		for (const T& X : Array)
		{
			if (X.Name == Name)
			{
				return X;
			}
		}
		return Array.Num() > 0 ? Array[0] : T();
	}
}

void ASaltLightingRig::InitLayersIfNeeded()
{
	if (bLayersInitialised)
	{
		return;
	}
	bLayersInitialised = true;
	RegionLayer.From = RegionLayer.To = FindPresetOr(Regions, StartRegion);
	WeatherLayer.From = WeatherLayer.To = FindPresetOr(Weathers, StartWeather);
	SeaLayer.From = SeaLayer.To = FindPresetOr(SeaStates, StartSeaState);
	CurrentRegion = RegionLayer.To.Name;
	CurrentWeather = WeatherLayer.To.Name;
	CurrentSeaState = SeaLayer.To.Name;
}

bool ASaltLightingRig::SetRegion(FName RegionName, float BlendSeconds)
{
	InitLayersIfNeeded();
	const FSaltRegionPreset* P = Regions.FindByPredicate([RegionName](const FSaltRegionPreset& X) { return X.Name == RegionName; });
	if (!P)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Region '%s' not found"), *RegionName.ToString());
		return false;
	}
	RegionLayer.From = LerpRegion(RegionLayer.From, RegionLayer.To, Smooth(RegionLayer.Alpha));
	RegionLayer.To = *P;
	RegionLayer.Duration = FMath::Max(0.f, BlendSeconds);
	RegionLayer.Alpha = RegionLayer.Duration > 0.f ? 0.f : 1.f;
	CurrentRegion = RegionName;
	ApplyEnvironment();
	return true;
}

bool ASaltLightingRig::SetWeather(FName WeatherName, float BlendSeconds)
{
	InitLayersIfNeeded();
	const FSaltWeatherPreset* P = Weathers.FindByPredicate([WeatherName](const FSaltWeatherPreset& X) { return X.Name == WeatherName; });
	if (!P)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Weather '%s' not found"), *WeatherName.ToString());
		return false;
	}
	WeatherLayer.From = LerpWeather(WeatherLayer.From, WeatherLayer.To, Smooth(WeatherLayer.Alpha));
	WeatherLayer.To = *P;
	WeatherLayer.Duration = FMath::Max(0.f, BlendSeconds);
	WeatherLayer.Alpha = WeatherLayer.Duration > 0.f ? 0.f : 1.f;
	CurrentWeather = WeatherName;
	ApplyEnvironment();
	return true;
}

bool ASaltLightingRig::SetSeaState(FName SeaStateName, float BlendSeconds)
{
	InitLayersIfNeeded();
	const FSaltSeaStatePreset* P = SeaStates.FindByPredicate([SeaStateName](const FSaltSeaStatePreset& X) { return X.Name == SeaStateName; });
	if (!P)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Sea state '%s' not found"), *SeaStateName.ToString());
		return false;
	}
	SeaLayer.From = LerpSeaState(SeaLayer.From, SeaLayer.To, Smooth(SeaLayer.Alpha));
	SeaLayer.To = *P;
	SeaLayer.Duration = FMath::Max(0.f, BlendSeconds);
	SeaLayer.Alpha = SeaLayer.Duration > 0.f ? 0.f : 1.f;
	CurrentSeaState = SeaStateName;
	ApplyEnvironment();
	return true;
}

FString ASaltLightingRig::DescribeEnvironment() const
{
	return FString::Printf(TEXT("time=%s region=%s (%.0f%%) weather=%s (%.0f%%) sea=%s (%.0f%%) zones=%.2f seaZones=%.2f"),
		*CurrentPreset.ToString(), *CurrentRegion.ToString(), RegionLayer.Alpha * 100.f, *CurrentWeather.ToString(),
		WeatherLayer.Alpha * 100.f, *CurrentSeaState.ToString(), SeaLayer.Alpha * 100.f, FMath::Max(LastZoneWeightSum, 0.f),
		FMath::Max(LastSeaZoneWeightSum, 0.f));
}

FSaltRegionPreset ASaltLightingRig::ComposeRegion(float* OutZoneWeightSum) const
{
	FSaltRegionPreset R = LerpRegion(RegionLayer.From, RegionLayer.To, Smooth(RegionLayer.Alpha));
	float WeightSum = 0.f;
	const UWorld* World = GetWorld();
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		const FVector View = World->ViewLocationsRenderedLastFrame[0];
		for (TActorIterator<ASaltRegionZone> It(World); It; ++It)
		{
			const float W = It->WeightAt(View);
			if (W <= 0.f)
			{
				continue;
			}
			const FName ZoneRegion = It->Region;
			if (const FSaltRegionPreset* Z = Regions.FindByPredicate([ZoneRegion](const FSaltRegionPreset& X) { return X.Name == ZoneRegion; }))
			{
				R = LerpRegion(R, *Z, W);
				WeightSum += W;
			}
		}
	}
	if (OutZoneWeightSum)
	{
		*OutZoneWeightSum = WeightSum;
	}
	return R;
}

FSaltSeaStatePreset ASaltLightingRig::ComposeSeaState(float* OutZoneWeightSum) const
{
	FSaltSeaStatePreset S = LerpSeaState(SeaLayer.From, SeaLayer.To, Smooth(SeaLayer.Alpha));
	float WeightSum = 0.f;
	const UWorld* World = GetWorld();
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		const FVector View = World->ViewLocationsRenderedLastFrame[0];
		for (TActorIterator<ASaltSeaStateZone> It(World); It; ++It)
		{
			const float W = It->WeightAt(View);
			if (W <= 0.f)
			{
				continue;
			}
			const FName ZoneSea = It->SeaState;
			if (const FSaltSeaStatePreset* Z = SeaStates.FindByPredicate([ZoneSea](const FSaltSeaStatePreset& X) { return X.Name == ZoneSea; }))
			{
				S = LerpSeaState(S, *Z, W);
				WeightSum += W;
			}
		}
	}
	if (OutZoneWeightSum)
	{
		*OutZoneWeightSum = WeightSum;
	}
	return S;
}

// --- Lifecycle ------------------------------------------------------------------------------------

void ASaltLightingRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureClouds();
}

void ASaltLightingRig::BeginPlay()
{
	Super::BeginPlay();
	EnsureClouds();
	InitLayersIfNeeded();
	if (!StartPreset.IsNone())
	{
		ApplyPreset(StartPreset);
	}
}

void ASaltLightingRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ASaltLightingRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	bool bChanged = false;
	auto Advance = [&](auto& Layer)
	{
		if (Layer.IsBlending())
		{
			Layer.Alpha = Layer.Duration > 0.f ? FMath::Min(1.f, Layer.Alpha + DeltaSeconds / Layer.Duration) : 1.f;
			bChanged = true;
		}
	};
	Advance(RegionLayer);
	Advance(WeatherLayer);
	Advance(SeaLayer);
	float ZoneSum = 0.f;
	ComposeRegion(&ZoneSum);
	if (FMath::Abs(ZoneSum - LastZoneWeightSum) > 0.002f)
	{
		bChanged = true;
	}
	float SeaZoneSum = 0.f;
	ComposeSeaState(&SeaZoneSum);
	if (FMath::Abs(SeaZoneSum - LastSeaZoneWeightSum) > 0.002f)
	{
		bChanged = true;
	}
	if (bChanged)
	{
		ApplyEnvironment();
	}
}

void ASaltLightingRig::EnsureClouds()
{
	if (!CloudMaterial)
	{
		CloudMaterial = LoadObject<UMaterialInterface>(nullptr, SaltContent::CloudMaterial);
	}
	if (!Clouds || !Clouds->IsRegistered())
	{
		// Transient: never saved with the level, so the dynamic material never ends up in an asset.
		Clouds = NewObject<UVolumetricCloudComponent>(this, NAME_None, RF_Transient);
		Clouds->SetupAttachment(GetRootComponent());
		Clouds->RegisterComponent();
		CloudMID = nullptr;
	}
	if (!CloudMID && CloudMaterial)
	{
		CloudMID = UMaterialInstanceDynamic::Create(CloudMaterial, this);
		CloudMID->SetFlags(RF_Transient);
		Clouds->SetMaterial(CloudMID);
	}
}

void ASaltLightingRig::ResolveSceneActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!Sun) { for (TActorIterator<ADirectionalLight> It(World); It; ++It) { Sun = *It; break; } }
	if (!SkyLight) { for (TActorIterator<ASkyLight> It(World); It; ++It) { SkyLight = *It; break; } }
	if (!Fog) { for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { Fog = *It; break; } }
	if (!Atmosphere) { for (TActorIterator<ASkyAtmosphere> It(World); It; ++It) { Atmosphere = *It; break; } }
	if (!Ocean) { for (TActorIterator<ASaltOcean> It(World); It; ++It) { Ocean = *It; break; } }
	if (!PostProcess)
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->bUnbound) { PostProcess = *It; break; }
		}
	}
}

bool ASaltLightingRig::ApplyPreset(FName PresetName)
{
	const FSaltLightingPreset* P = Presets.FindByPredicate([PresetName](const FSaltLightingPreset& X) { return X.Name == PresetName; });
	if (!P)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Lighting preset '%s' not found"), *PresetName.ToString());
		return false;
	}
	CurrentPreset = PresetName;
	ApplyEnvironment();
	UE_LOG(LogSaltbound, Log, TEXT("Applied lighting preset %s"), *PresetName.ToString());
	return true;
}

void ASaltLightingRig::ApplyEnvironment()
{
	InitLayersIfNeeded();
	const FName Tod = CurrentPreset.IsNone() ? StartPreset : CurrentPreset;
	const FSaltLightingPreset* P = Presets.FindByPredicate([Tod](const FSaltLightingPreset& X) { return X.Name == Tod; });
	if (!P && Presets.Num() > 0)
	{
		P = &Presets[0];
	}
	if (!P)
	{
		return;
	}
	CurrentPreset = P->Name;
	ResolveSceneActors();
	EnsureClouds();
	float ZoneSum = 0.f;
	const FSaltRegionPreset R = ComposeRegion(&ZoneSum);
	LastZoneWeightSum = ZoneSum;
	float SeaZoneSum = 0.f;
	const FSaltSeaStatePreset S = ComposeSeaState(&SeaZoneSum);
	LastSeaZoneWeightSum = SeaZoneSum;
	PushEnvironment(*P, R, LerpWeather(WeatherLayer.From, WeatherLayer.To, Smooth(WeatherLayer.Alpha)), S);
}

void ASaltLightingRig::PushEnvironment(const FSaltLightingPreset& P, const FSaltRegionPreset& R, const FSaltWeatherPreset& W, const FSaltSeaStatePreset& S)
{
	if (Sun)
	{
		// Directional light points along its forward vector: pitch = -elevation.
		Sun->SetActorRotation(FRotator(-P.SunElevation, P.SunYaw, 0.f));
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			C->SetIntensity(P.SunIlluminance * R.SunIntensityMul * W.SunIntensityMul);
			C->SetUseTemperature(true);
			C->SetTemperature(P.SunTemperature);
			C->SetLightColor(R.SunTint, false);
			C->SetLightSourceAngle(P.SunSourceAngle);
		}
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		USkyLightComponent* C = SkyLight->GetLightComponent();
		C->SetIntensity(P.SkyLightIntensity * R.SkyLightMul * W.SkyLightMul);
		C->SetLightColor(Mul(P.SkyLightTint, R.SkyLightTint));
	}
	if (Atmosphere)
	{
		if (USkyAtmosphereComponent* A = Atmosphere->FindComponentByClass<USkyAtmosphereComponent>())
		{
			A->SetRayleighScattering(R.RayleighColor);
			A->SetRayleighScatteringScale(P.RayleighScale * R.RayleighScaleMul);
			A->SetMieScatteringScale(P.MieScale * R.MieScaleMul);
			A->SetMieAbsorptionScale(R.MieAbsorptionScale);
			A->SetSkyLuminanceFactor(R.SkyLuminance);
		}
	}
	if (Fog && Fog->GetComponent())
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetFogDensity(P.FogDensity * R.FogDensityMul * W.FogDensityMul);
		F->SetFogHeightFalloff(P.FogHeightFalloff);
		F->SetFogInscatteringColor(Mul(P.FogInscattering, R.FogTint));
		F->SetDirectionalInscatteringColor(Mul(P.FogDirectionalInscattering, R.FogDirectionalTint));
		F->SetStartDistance(P.FogStartDistance);
	}
	if (CloudMID)
	{
		CloudMID->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), FMath::Clamp(R.CloudCoverage + W.CloudCoverageOffset, -1.f, 1.f));
		const FLinearColor Albedo = R.CloudAlbedo * W.CloudAlbedoMul;
		CloudMID->SetVectorParameterValue(TEXT("Cloud_AlbedoColor"), FLinearColor(Albedo.R, Albedo.G, Albedo.B, 0.5f));
	}
	if (PostProcess)
	{
		FPostProcessSettings& PS = PostProcess->Settings;
		PS.bOverride_AutoExposureMinBrightness = true;
		PS.bOverride_AutoExposureMaxBrightness = true;
		PS.bOverride_AutoExposureBias = true;
		PS.AutoExposureMinBrightness = P.ExposureMinEV;
		PS.AutoExposureMaxBrightness = P.ExposureMaxEV;
		PS.AutoExposureBias = P.ExposureBias;
		const float Sat = P.Saturation * R.SaturationMul * W.SaturationMul;
		const float Con = P.Contrast * R.ContrastMul * W.ContrastMul;
		const FLinearColor Gain = Mul(P.Gain, R.GainMul);
		const FLinearColor ShadowGain = Mul(P.ShadowGain, R.ShadowGainMul);
		PS.bOverride_ColorSaturation = true;
		PS.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
		PS.bOverride_ColorGain = true;
		PS.ColorGain = FVector4(Gain.R, Gain.G, Gain.B, 1.f);
		PS.bOverride_ColorContrast = true;
		PS.ColorContrast = FVector4(Con, Con, Con, 1.f);
		PS.bOverride_ColorGainShadows = true;
		PS.ColorGainShadows = FVector4(ShadowGain.R, ShadowGain.G, ShadowGain.B, 1.f);
		PS.bOverride_ColorGainHighlights = true;
		PS.ColorGainHighlights = FVector4(P.HighlightGain.R, P.HighlightGain.G, P.HighlightGain.B, 1.f);
		// Lumen screen traces hit the water's own depth at grazing angles and paint stair-stepped
		// reflection blocks onto the waves (D23). Hardware ray tracing alone gives clean reflections.
		PS.bOverride_LumenReflectionsScreenTraces = true;
		PS.LumenReflectionsScreenTraces = 0;
	}
	// Water look and sea state.
	if (Ocean)
	{
		const bool bSeaChanged = !bHaveAppliedSea || !FMath::IsNearlyEqual(S.WaveHeight, LastAppliedSea.WaveHeight)
			|| !FMath::IsNearlyEqual(S.WaveSteepness, LastAppliedSea.WaveSteepness)
			|| !FMath::IsNearlyEqual(S.ChopVariation, LastAppliedSea.ChopVariation)
			|| !FMath::IsNearlyEqual(Ocean->AmplitudeScale, S.WaveHeight);
		if (bSeaChanged)
		{
			Ocean->AmplitudeScale = S.WaveHeight;
			Ocean->SteepnessScale = S.WaveSteepness;
			Ocean->ChopVariation = S.ChopVariation;
			Ocean->RebuildWaveField();
			LastAppliedSea = S;
			bHaveAppliedSea = true;
		}
		// Wind sea: read by the ocean every frame (no rebuild needed).
		Ocean->bWindSea = S.WindSeaHeight > 0.f;
		Ocean->WindSeaHeight = S.WindSeaHeight;
		Ocean->WindSeaPeakWavelength = FMath::Max(S.WindSeaWavelength, 100.f);
		Ocean->WindSeaGamma = FMath::Max(S.WindSeaGamma, 1.f);
		Ocean->WindSeaSpread = S.WindSeaSpread;
		Ocean->WindSeaSpreadMin = S.WindSeaSpreadMin;
		Ocean->ShortWaveLength = S.ShortWaveLength;
		// Major ridges sharper than the medium waves; ripples subordinate (owner feedback, D27).
		Ocean->CascadeChop = FVector(1.12, 1.0, 0.5) * S.Choppiness;
		Ocean->CascadeGain = FVector(1.0, 0.5 * S.MediumWaveScale, 0.14 * S.RippleScale);
		UWorld* World = GetWorld();
		if (World && Ocean->ParameterCollection)
		{
			if (UMaterialParameterCollectionInstance* Inst = World->GetParameterCollectionInstance(Ocean->ParameterCollection))
			{
				const FLinearColor& Ab = R.WaterAbsorption;
				const FLinearColor& Sc = R.WaterScattering;
				Inst->SetVectorParameterValue(PAbsorption, FLinearColor(Ab.R, Ab.G, Ab.B, 1.f));
				Inst->SetVectorParameterValue(PScattering, FLinearColor(Sc.R, Sc.G, Sc.B, 1.f));
				Inst->SetVectorParameterValue(PCrest, FLinearColor(R.CrestLightColor.R, R.CrestLightColor.G, R.CrestLightColor.B, R.CrestLightStrength));
			}
		}
	}
	for (TActorIterator<ASaltLantern> It(GetWorld()); It; ++It)
	{
		It->SetLit(P.bLanternsLit);
	}
}

// --- Sea state zone -------------------------------------------------------------------------------

ASaltSeaStateZone::ASaltSeaStateZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

float ASaltSeaStateZone::WeightAt(const FVector& WorldLocation) const
{
	const float D = (float)FVector2D::Distance(FVector2D(WorldLocation), FVector2D(GetActorLocation()));
	const float T = FMath::Clamp((D - InnerRadius) / FMath::Max(OuterRadius - InnerRadius, 1.f), 0.f, 1.f);
	return 1.f - T * T * (3.f - 2.f * T);
}

// --- Region zone ----------------------------------------------------------------------------------

ASaltRegionZone::ASaltRegionZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

float ASaltRegionZone::WeightAt(const FVector& WorldLocation) const
{
	const float D = (float)FVector2D::Distance(FVector2D(WorldLocation), FVector2D(GetActorLocation()));
	const float T = FMath::Clamp((D - InnerRadius) / FMath::Max(OuterRadius - InnerRadius, 1.f), 0.f, 1.f);
	return 1.f - T * T * (3.f - 2.f * T);
}

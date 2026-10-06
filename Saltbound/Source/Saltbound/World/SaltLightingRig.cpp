// Saltbound - environment rig: time of day, region, weather and sea state, applied to sun, sky, fog, clouds,
// post-processing and the ocean.

#include "World/SaltLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
#include "World/SaltInteriorZone.h"
#include "Boat/SaltBoat.h"
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
		TEXT("Time of day: sb.Lighting Day | Evening | Dusk | Night | Dawn | Morning (jumps the clock there and holds it; sb.Time run lets it go on)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0) { ForEachRig(World, [&](ASaltLightingRig& R) { R.ApplyPreset(FName(*Args[0])); }); }
		}));

	// D72: the day's clock.
	TAutoConsoleVariable<float> CVarTimeScale(
		TEXT("sb.Time.Scale"), 1.f,
		TEXT("How fast the day's clock runs (1 = a day in the rig's DayLengthMinutes; 60 = an hour of game time a second at 40 minutes)."));

	FAutoConsoleCommandWithWorldAndArgs CmdSkyDome(
		TEXT("sb.Sky.Dome"),
		TEXT("D72: sb.Sky.Dome 1 | 0 (the rig's sky dome with the stars; 0 lets the atmosphere draw the sky itself)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachRig(World, [&](ASaltLightingRig& R) { R.SetSkyDomeVisible(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTime(
		TEXT("sb.Time"),
		TEXT("The day's clock: sb.Time (print) | sb.Time <hour 0-24> [hold] | sb.Time run | sb.Time hold | sb.Time day <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachRig(World, [&](ASaltLightingRig& R)
			{
				if (Args.Num() == 1 && Args[0] == TEXT("run")) { R.SetClockHeld(false); }
				else if (Args.Num() == 1 && Args[0] == TEXT("hold")) { R.SetClockHeld(true); }
				else if (Args.Num() == 2 && Args[0] == TEXT("day")) { R.SetDay(FCString::Atoi(*Args[1])); }
				else if (Args.Num() > 0) { R.SetTimeOfDay(FCString::Atof(*Args[0]), Args.Num() > 1 && Args[1] == TEXT("hold")); }
				UE_LOG(LogSaltbound, Display, TEXT("SB_TIME day=%d hour=%.2f phase=%s sun=%.1f night=%.2f held=%d lanterns=%d"), R.GetDay(),
					R.GetTimeOfDay(), *R.GetDayPhase().ToString(), R.GetSunElevation(), R.GetNightAmount(), R.IsClockHeld() ? 1 : 0,
					R.ShouldLanternsBeLit() ? 1 : 0);
			});
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

	/**
	 * D72: the looks the clock adds to Day, Evening and Dusk. Morning is the evening's sister in the east, a little cooler;
	 * Dawn the blue hour before it; Night is lit by the moon (the rig's second atmosphere light): a deep blue "day for night"
	 * sky, cool and a little desaturated, the eye opened far (EV100 about 1), so the lanterns burn in warm pools.
	 */
	TArray<FSaltLightingPreset> MakeClockLooks()
	{
		TArray<FSaltLightingPreset> Looks;

		FSaltLightingPreset Morning;
		Morning.Name = TEXT("Morning");
		Morning.SunElevation = 5.f;
		Morning.SunYaw = 245.f;
		Morning.SunTemperature = 5300.f;
		Morning.SunSourceAngle = 1.8f;
		Morning.SkyLightIntensity = 0.9f;
		Morning.SkyLightTint = FLinearColor(0.85f, 0.92f, 1.12f);
		Morning.FogDensity = 0.045f;
		Morning.FogHeightFalloff = 0.1f;
		Morning.FogInscattering = FLinearColor(0.22f, 0.26f, 0.36f);
		Morning.FogDirectionalInscattering = FLinearColor(0.95f, 0.6f, 0.42f);
		Morning.FogStartDistance = 800.f;
		Morning.ExposureMinEV = 9.5f;
		Morning.ExposureMaxEV = 12.5f;
		Morning.ExposureBias = -0.4f;
		Morning.Saturation = 1.f;
		Morning.Gain = FLinearColor(1.f, 0.99f, 1.02f);
		Morning.Contrast = 1.05f;
		Morning.ShadowGain = FLinearColor(0.9f, 0.94f, 1.12f);
		Morning.HighlightGain = FLinearColor(1.08f, 1.f, 0.92f);
		Morning.bLanternsLit = true;
		Looks.Add(Morning);

		FSaltLightingPreset Dawn;
		Dawn.Name = TEXT("Dawn");
		Dawn.SunElevation = -4.f;
		Dawn.SunYaw = 235.f;
		Dawn.SunTemperature = 6000.f;
		Dawn.SkyLightIntensity = 1.f;
		Dawn.FogDensity = 0.04f;
		Dawn.FogHeightFalloff = 0.1f;
		Dawn.FogInscattering = FLinearColor(0.09f, 0.1f, 0.17f);
		Dawn.FogDirectionalInscattering = FLinearColor(0.16f, 0.1f, 0.1f);
		Dawn.FogStartDistance = 600.f;
		Dawn.ExposureMinEV = 4.5f;
		Dawn.ExposureMaxEV = 8.f;
		Dawn.ExposureBias = -0.9f;
		Dawn.Saturation = 0.93f;
		Dawn.Contrast = 1.04f;
		Dawn.ShadowGain = FLinearColor(0.92f, 0.95f, 1.1f);
		Dawn.Gain = FLinearColor(0.98f, 0.97f, 1.05f);
		Dawn.bLanternsLit = true;
		Looks.Add(Dawn);

		// Between the blue hour and the night: the eye may open anywhere from the night's to the evening's, so it follows the
		// sky as the last glow goes (no burnt-white sky, no black jump).
		FSaltLightingPreset Twilight;
		Twilight.Name = TEXT("Twilight");
		Twilight.SunElevation = -11.f;
		Twilight.SunYaw = 165.f;
		Twilight.SkyLightIntensity = 1.f;
		Twilight.SkyLightTint = FLinearColor(0.82f, 0.9f, 1.15f);
		Twilight.FogDensity = 0.032f;
		Twilight.FogHeightFalloff = 0.11f;
		Twilight.FogInscattering = FLinearColor(0.05f, 0.06f, 0.11f);
		Twilight.FogDirectionalInscattering = FLinearColor(0.09f, 0.07f, 0.08f);
		Twilight.FogStartDistance = 600.f;
		Twilight.ExposureMinEV = -1.f;
		Twilight.ExposureMaxEV = 7.f;
		Twilight.ExposureBias = -1.3f;
		Twilight.Saturation = 0.86f;
		Twilight.Gain = FLinearColor(0.93f, 0.97f, 1.1f);
		Twilight.Contrast = 1.05f;
		Twilight.ShadowGain = FLinearColor(0.9f, 0.94f, 1.15f);
		Twilight.HighlightGain = FLinearColor(0.98f, 1.f, 1.03f);
		Twilight.bLanternsLit = true;
		Looks.Add(Twilight);

		FSaltLightingPreset Night;
		Night.Name = TEXT("Night");
		Night.SunElevation = -30.f;
		Night.SunYaw = 200.f;
		Night.SkyLightIntensity = 1.f;
		Night.SkyLightTint = FLinearColor(0.8f, 0.9f, 1.2f);
		Night.FogDensity = 0.03f;
		Night.FogHeightFalloff = 0.12f;
		Night.FogInscattering = FLinearColor(0.02f, 0.03f, 0.06f);
		Night.FogDirectionalInscattering = FLinearColor(0.05f, 0.06f, 0.09f);
		Night.FogStartDistance = 600.f;
		Night.ExposureMinEV = -1.5f;
		Night.ExposureMaxEV = 2.5f;
		Night.ExposureBias = -1.6f;
		Night.Saturation = 0.78f;
		Night.Gain = FLinearColor(0.9f, 0.96f, 1.12f);
		Night.Contrast = 1.06f;
		Night.ShadowGain = FLinearColor(0.88f, 0.94f, 1.18f);
		Night.HighlightGain = FLinearColor(0.96f, 1.f, 1.05f);
		Night.bLanternsLit = true;
		Looks.Add(Night);
		return Looks;
	}

	/**
	 * D72: the day. Hours, look, sun elevation, sun azimuth (unwrapped: midnight's 20 degrees follow the evening's 352 as 380).
	 * Day, Evening and Dusk stand where their presets stood (Day 32 degrees at azimuth 120, Evening 4 and Dusk -3 at 340), so
	 * sb.Lighting gives the reviewed looks. Night runs from 21 to 3.8 (faster, NightSpeed), the sun is up from about 5.2 to 19.4.
	 * Dusk and Dawn hold down to -8 degrees: the twilight sky darkens far faster than the hours pass, and an eye opened for
	 * the night while the sky still glows burns it white.
	 */
	TArray<FSaltTimeKey> MakeDayCycle()
	{
		auto Key = [](float Hour, const TCHAR* Look, float Elevation, float Azimuth)
		{
			FSaltTimeKey K;
			K.Hour = Hour;
			K.Look = Look;
			K.SunElevation = Elevation;
			K.SunAzimuth = Azimuth;
			return K;
		};
		return {
			Key(0.f, TEXT("Night"), -34.f, 20.f),
			Key(3.8f, TEXT("Night"), -16.f, 44.f),
			Key(4.05f, TEXT("Twilight"), -12.f, 47.f),
			Key(4.3f, TEXT("Dawn"), -8.f, 50.f),
			Key(4.8f, TEXT("Dawn"), -4.f, 55.f),
			Key(5.8f, TEXT("Morning"), 5.f, 65.f),
			Key(8.5f, TEXT("Day"), 32.f, 120.f),
			Key(13.f, TEXT("Day"), 52.f, 190.f),
			Key(17.f, TEXT("Day"), 28.f, 275.f),
			Key(19.f, TEXT("Evening"), 4.f, 340.f),
			Key(19.75f, TEXT("Dusk"), -3.f, 340.f),
			Key(20.3f, TEXT("Dusk"), -8.f, 346.f),
			Key(20.65f, TEXT("Twilight"), -12.f, 349.f),
			Key(21.f, TEXT("Night"), -16.f, 352.f),
		};
	}
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
	// Sea haze (D49, owner: "add fog, so you cant look infinite in the distance"): the horizon dissolves into a pale band,
	// the spire 420 m out is softened, Ruin Isle (1.7 km) shows faintly and comes out of the haze as you sail closer, the
	// far islands are ghosts. The sky atmosphere lights the fog (its colours here are small luminances on top).
	Day.FogDensity = 0.035f;
	Day.FogHeightFalloff = 0.2f;
	Day.FogInscattering = FLinearColor(0.35f, 0.55f, 0.85f);
	Day.FogDirectionalInscattering = FLinearColor(0.75f, 0.62f, 0.42f);
	Day.FogStartDistance = 3000.f;
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
	Evening.FogDensity = 0.05f;
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
	Dusk.FogDensity = 0.035f;
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

	// D72: Morning, Dawn, Twilight and Night, and the day's keys.
	Presets.Append(MakeClockLooks());
	DayCycle = MakeDayCycle();

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

FSaltLightingPreset ASaltLightingRig::LerpLook(const FSaltLightingPreset& A, const FSaltLightingPreset& B, float T)
{
	FSaltLightingPreset R = T < 0.5f ? A : B;
	auto L = [T](float X, float Y) { return FMath::Lerp(X, Y, T); };
	auto C = [T](const FLinearColor& X, const FLinearColor& Y) { return X + (Y - X) * T; };
	R.SunElevation = L(A.SunElevation, B.SunElevation);
	R.SunIlluminance = L(A.SunIlluminance, B.SunIlluminance);
	R.SunTemperature = L(A.SunTemperature, B.SunTemperature);
	R.SunSourceAngle = L(A.SunSourceAngle, B.SunSourceAngle);
	R.SkyLightIntensity = L(A.SkyLightIntensity, B.SkyLightIntensity);
	R.SkyLightTint = C(A.SkyLightTint, B.SkyLightTint);
	R.RayleighScale = L(A.RayleighScale, B.RayleighScale);
	R.MieScale = L(A.MieScale, B.MieScale);
	R.FogDensity = L(A.FogDensity, B.FogDensity);
	R.FogHeightFalloff = L(A.FogHeightFalloff, B.FogHeightFalloff);
	R.FogInscattering = C(A.FogInscattering, B.FogInscattering);
	R.FogDirectionalInscattering = C(A.FogDirectionalInscattering, B.FogDirectionalInscattering);
	R.FogStartDistance = L(A.FogStartDistance, B.FogStartDistance);
	R.ExposureMinEV = L(A.ExposureMinEV, B.ExposureMinEV);
	R.ExposureMaxEV = L(A.ExposureMaxEV, B.ExposureMaxEV);
	R.ExposureBias = L(A.ExposureBias, B.ExposureBias);
	R.Saturation = L(A.Saturation, B.Saturation);
	R.Gain = C(A.Gain, B.Gain);
	R.Contrast = L(A.Contrast, B.Contrast);
	R.ShadowGain = C(A.ShadowGain, B.ShadowGain);
	R.HighlightGain = C(A.HighlightGain, B.HighlightGain);
	return R;
}

// --- The day's clock (D72) --------------------------------------------------------------------------

const FSaltLightingPreset* ASaltLightingRig::FindLook(FName Look) const
{
	return Presets.FindByPredicate([Look](const FSaltLightingPreset& X) { return X.Name == Look; });
}

void ASaltLightingRig::EnsureDefaultLooks()
{
	for (const FSaltLightingPreset& Look : MakeClockLooks())
	{
		if (!FindLook(Look.Name))
		{
			Presets.Add(Look);
		}
	}
}

float ASaltLightingRig::GetLookHour(FName Look) const
{
	for (const FSaltTimeKey& K : DayCycle)
	{
		if (K.Look == Look)
		{
			return K.Hour;
		}
	}
	return -1.f;
}

FSaltLightingPreset ASaltLightingRig::ComposeTimeLook(float Hour, float& OutSunElevation, float& OutSunAzimuth) const
{
	const int32 N = DayCycle.Num();
	Hour = FMath::Fmod(Hour, 24.f);
	if (Hour < 0.f)
	{
		Hour += 24.f;
	}
	// The last key at or before the hour; before the first key that is yesterday's last.
	int32 I = N - 1;
	for (int32 K = 0; K < N; ++K)
	{
		if (DayCycle[K].Hour <= Hour)
		{
			I = K;
		}
		else
		{
			break;
		}
	}
	const FSaltTimeKey& A = DayCycle[I];
	const FSaltTimeKey& B = DayCycle[(I + 1) % N];
	float HourA = A.Hour, AzimuthA = A.SunAzimuth;
	float HourB = B.Hour, AzimuthB = B.SunAzimuth;
	if (I == N - 1)
	{
		if (Hour < A.Hour)
		{
			HourA -= 24.f;
			AzimuthA -= 360.f;
		}
		else
		{
			HourB += 24.f;
			AzimuthB += 360.f;
		}
	}
	const float T = HourB > HourA ? FMath::Clamp((Hour - HourA) / (HourB - HourA), 0.f, 1.f) : 0.f;
	// The sun moves evenly between keys; the look eases from one to the next.
	OutSunElevation = FMath::Lerp(A.SunElevation, B.SunElevation, T);
	OutSunAzimuth = FMath::Lerp(AzimuthA, AzimuthB, T);
	const FSaltLightingPreset* LookA = FindLook(A.Look);
	const FSaltLightingPreset* LookB = FindLook(B.Look);
	const FSaltLightingPreset Fallback = Presets.Num() > 0 ? Presets[0] : FSaltLightingPreset();
	return LerpLook(LookA ? *LookA : Fallback, LookB ? *LookB : Fallback, Smooth(T));
}

FName ASaltLightingRig::GetDayPhase() const
{
	if (DayCycle.Num() == 0)
	{
		return CurrentPreset;
	}
	const float E = NowSunElevation;
	const bool bMorning = TimeOfDay < 12.f;
	if (E < -6.f)
	{
		return TEXT("Night");
	}
	if (E < 2.f)
	{
		return bMorning ? TEXT("Dawn") : TEXT("Dusk");
	}
	if (bMorning)
	{
		return E < 20.f ? TEXT("Morning") : TEXT("Day");
	}
	if (E < 15.f)
	{
		return TEXT("Evening");
	}
	return TimeOfDay >= 15.f ? TEXT("Afternoon") : TEXT("Day");
}

void ASaltLightingRig::SetTimeOfDay(float Hour, bool bHold)
{
	TimeOfDay = FMath::Fmod(Hour, 24.f);
	if (TimeOfDay < 0.f)
	{
		TimeOfDay += 24.f;
	}
	bClockHeld = bHold;
	bForceSunStep = true;
	LanternState = -1;
	ApplyEnvironment();
}

void ASaltLightingRig::EnsureMoon()
{
	if (Moon && Moon->IsRegistered())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || DayCycle.Num() == 0)
	{
		return;
	}
	// Transient, like the clouds: never saved with the level. The atmosphere's second light: it lights the sky, the clouds
	// and the sea's glitter at night and draws its own disc.
	Moon = NewObject<UDirectionalLightComponent>(this, TEXT("Moon"), RF_Transient);
	Moon->SetupAttachment(GetRootComponent());
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetUseTemperature(false);
	Moon->SetLightColor(MoonColor, false);
	Moon->SetLightSourceAngle(MoonSourceAngle);
	Moon->SetAtmosphereSunDiskColorScale(FLinearColor(MoonDiskScale, MoonDiskScale, MoonDiskScale));
	Moon->SetIntensity(0.f);
	Moon->SetCastShadows(false);
	Moon->SetVisibility(false);
	Moon->RegisterComponent();
	MoonShadowState = 0;
	ForwardState = -1;
	bForceSunStep = true;
}

void ASaltLightingRig::EnsureSkyDome()
{
	UWorld* World = GetWorld();
	if (!bSkyDome || (SkyDome && SkyDome->IsRegistered()) || !World || !World->IsGameWorld())
	{
		return;
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, SaltContent::SkyDomeMesh);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, SaltContent::SkyMaterial);
	SkyCollection = LoadObject<UMaterialParameterCollection>(nullptr, SaltContent::SkyCollection);
	if (!Mesh || !Material)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Sky dome: %s or %s missing (Tools/ue_scripts/setup_night_sky.py)"), SaltContent::SkyDomeMesh,
			SaltContent::SkyMaterial);
		return;
	}
	SkyDome = NewObject<UStaticMeshComponent>(this, TEXT("SkyDome"), RF_Transient);
	SkyDome->SetupAttachment(GetRootComponent());
	SkyDome->SetUsingAbsoluteLocation(true);
	SkyDome->SetUsingAbsoluteRotation(true);
	SkyDome->SetUsingAbsoluteScale(true);
	SkyDome->SetStaticMesh(Mesh);
	SkyDome->SetMaterial(0, Material);
	// The mesh's radius is 4096 cm; far beyond the clouds and the haze.
	SkyDome->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	SkyDome->SetWorldScale3D(FVector(SkyDomeRadius / 4096.f));
	SkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyDome->SetCastShadow(false);
	SkyDome->SetAffectDistanceFieldLighting(false);
	SkyDome->SetAffectDynamicIndirectLighting(false);
	SkyDome->SetVisibleInRayTracing(false);
	SkyDome->bNeverDistanceCull = true;
	SkyDome->bVisibleInRealTimeSkyCaptures = true;
	SkyDome->RegisterComponent();
}

void ASaltLightingRig::SetSkyDomeVisible(bool bVisible)
{
	bSkyDome = bVisible;
	EnsureSkyDome();
	if (SkyDome)
	{
		SkyDome->SetVisibility(bVisible);
	}
}

void ASaltLightingRig::PushMoon(float SunElevation, float Hour)
{
	// The moon takes over as the sun sinks from -2 to -9 degrees (and hands back at dawn).
	const float Amount = Smooth((-2.f - SunElevation) / 7.f);
	NowNight = Amount;
	// The stars come out with the moon.
	if (UWorld* World = GetWorld(); World && SkyCollection)
	{
		if (UMaterialParameterCollectionInstance* Inst = World->GetParameterCollectionInstance(SkyCollection))
		{
			Inst->SetScalarParameterValue(TEXT("StarAmount"), Amount);
		}
	}
	EnsureMoon();
	if (!Moon)
	{
		return;
	}
	float Span = MoonSetHour - MoonRiseHour;
	if (Span <= 0.f)
	{
		Span += 24.f;
	}
	float Since = Hour - MoonRiseHour;
	if (Since < 0.f)
	{
		Since += 24.f;
	}
	// Rises in the east-south-east, crosses the south, sets in the west-south-west.
	const float P = FMath::Clamp(Since / Span, 0.f, 1.f);
	const float Azimuth = 100.f + 160.f * P;
	const float Elevation = 10.f + (MoonHighestElevation - 10.f) * FMath::Sin(PI * P);
	const FRotator Rotation(-Elevation, Azimuth + 180.f, 0.f);
	if (bForceSunStep || !LastMoonRotation.Equals(Rotation, SunStepDegrees))
	{
		Moon->SetWorldRotation(Rotation);
		LastMoonRotation = Rotation;
	}
	const bool bVisible = Amount > 0.001f;
	Moon->SetIntensity(MoonIlluminance * Amount);
	if (Moon->IsVisible() != bVisible)
	{
		Moon->SetVisibility(bVisible);
	}
	const int32 Shadow = Amount > 0.4f ? 1 : 0;
	if (Shadow != MoonShadowState)
	{
		Moon->SetCastShadows(Shadow != 0);
		MoonShadowState = Shadow;
	}
	// Forward shading (the water's glitter, translucency) takes one directional light: the one that rules the sky now.
	const int32 MoonFirst = Amount > 0.5f ? 1 : 0;
	if (MoonFirst != ForwardState)
	{
		ForwardState = MoonFirst;
		Moon->SetForwardShadingPriority(MoonFirst);
		if (UDirectionalLightComponent* SunLight = Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr)
		{
			SunLight->SetForwardShadingPriority(1 - MoonFirst);
		}
	}
}

void ASaltLightingRig::PushLanterns(bool bLit, bool bForce)
{
	bLanternsShouldBeLit = bLit;
	// Only when the light changes (or the clock was set): a lantern the player lit or doused keeps its state until then.
	if (!bForce && LanternState == (bLit ? 1 : 0))
	{
		return;
	}
	LanternState = bLit ? 1 : 0;
	for (TActorIterator<ASaltLantern> It(GetWorld()); It; ++It)
	{
		It->SetLit(bLit);
	}
	for (TActorIterator<ASaltBoat> It(GetWorld()); It; ++It)
	{
		It->SetLanternLit(bLit);
	}
}

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
	return FString::Printf(TEXT("time=%s (day %d, %.2f h%s) region=%s (%.0f%%) weather=%s (%.0f%%) sea=%s (%.0f%%) zones=%.2f seaZones=%.2f"),
		*CurrentPreset.ToString(), DayOfVoyage, TimeOfDay, bClockHeld ? TEXT(", held") : TEXT(""), *CurrentRegion.ToString(), RegionLayer.Alpha * 100.f, *CurrentWeather.ToString(),
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
	EnsureDefaultLooks();
	EnsureClouds();
	InitLayersIfNeeded();
	if (DayCycle.Num() > 0)
	{
		// D72: the clock starts at the start look's hour and runs (a loaded game sets its own time).
		EnsureMoon();
		EnsureSkyDome();
		const float StartHour = GetLookHour(StartPreset);
		SetTimeOfDay(StartHour >= 0.f ? StartHour : 8.5f, false);
	}
	else if (!StartPreset.IsNone())
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
	// D72: the clock. The look is recomposed four times a second; the sun itself turns in small steps (PushEnvironment).
	UWorld* World = GetWorld();
	if (DayCycle.Num() > 0 && bClockRuns && !bClockHeld && World && World->IsGameWorld())
	{
		const float Scale = FMath::Max(CVarTimeScale.GetValueOnGameThread(), 0.f);
		const float Rate = 24.f / (FMath::Max(DayLengthMinutes, 1.f) * 60.f) * Scale * FMath::Lerp(1.f, NightSpeed, NowNight);
		TimeOfDay += DeltaSeconds * Rate;
		while (TimeOfDay >= 24.f)
		{
			TimeOfDay -= 24.f;
			++DayOfVoyage;
		}
		ClockApplyAccum += DeltaSeconds;
		if (ClockApplyAccum >= 0.25f)
		{
			ClockApplyAccum = 0.f;
			bChanged = true;
		}
	}
	// Lanterns and boats that started after the rig get the lantern state once more, a second into play.
	if (!bClockStarted && GetGameTimeSinceCreation() > 1.f)
	{
		bClockStarted = true;
		LanternState = -1;
		bChanged = true;
	}
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
	else
	{
		ApplyInteriorFog();
	}
}

float ASaltLightingRig::ComposeInterior(float& OutFogStartCm, float& OutDensityScale) const
{
	OutFogStartCm = BaseFogStartCm;
	OutDensityScale = 1.f;
	const UWorld* World = GetWorld();
	if (!World || World->ViewLocationsRenderedLastFrame.Num() == 0)
	{
		return 0.f;
	}
	const FVector View = World->ViewLocationsRenderedLastFrame[0];
	float Best = 0.f;
	for (TActorIterator<ASaltInteriorZone> It(World); It; ++It)
	{
		const float Wt = It->WeightAt(View);
		if (Wt > Best)
		{
			Best = Wt;
			OutFogStartCm = FMath::Lerp(BaseFogStartCm, FMath::Max(It->FogStartCm, BaseFogStartCm), Wt);
			OutDensityScale = FMath::Lerp(1.f, It->FogDensityScale, Wt);
		}
	}
	return Best;
}

void ASaltLightingRig::ApplyInteriorFog()
{
	if (!Fog || !Fog->GetComponent())
	{
		return;
	}
	float Start = BaseFogStartCm;
	float Scale = 1.f;
	const float Wt = ComposeInterior(Start, Scale);
	if (LastInteriorWeight >= 0.f && FMath::Abs(Wt - LastInteriorWeight) < 0.002f)
	{
		return;
	}
	LastInteriorWeight = Wt;
	UExponentialHeightFogComponent* F = Fog->GetComponent();
	F->SetFogDensity(BaseFogDensity * Scale);
	F->SetStartDistance(Start);
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
	EnsureDefaultLooks();
	const FSaltLightingPreset* P = Presets.FindByPredicate([PresetName](const FSaltLightingPreset& X) { return X.Name == PresetName; });
	if (!P)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Lighting preset '%s' not found"), *PresetName.ToString());
		return false;
	}
	// D72: with a clock the preset is a moment of the day; the clock goes there and holds.
	const float Hour = GetLookHour(PresetName);
	if (DayCycle.Num() > 0 && Hour >= 0.f)
	{
		SetTimeOfDay(Hour, true);
	}
	else
	{
		CurrentPreset = PresetName;
		ApplyEnvironment();
	}
	UE_LOG(LogSaltbound, Log, TEXT("Applied lighting preset %s"), *PresetName.ToString());
	return true;
}

void ASaltLightingRig::ApplyEnvironment()
{
	InitLayersIfNeeded();
	if (DayCycle.Num() > 0)
	{
		// D72: the look of the clock's hour, with the sun where the day's keys put it.
		float Elevation = 0.f;
		float Azimuth = 0.f;
		FSaltLightingPreset Look = ComposeTimeLook(TimeOfDay, Elevation, Azimuth);
		Look.SunElevation = Elevation;
		Look.SunYaw = Azimuth + 180.f;   // the light shines away from the sun
		Look.bLanternsLit = Elevation < LanternSunElevation;
		NowSunElevation = Elevation;
		Look.Name = GetDayPhase();
		CurrentPreset = Look.Name;
		ResolveSceneActors();
		EnsureClouds();
		float ZoneSum = 0.f;
		const FSaltRegionPreset R = ComposeRegion(&ZoneSum);
		LastZoneWeightSum = ZoneSum;
		float SeaZoneSum = 0.f;
		const FSaltSeaStatePreset S = ComposeSeaState(&SeaZoneSum);
		LastSeaZoneWeightSum = SeaZoneSum;
		PushEnvironment(Look, R, LerpWeather(WeatherLayer.From, WeatherLayer.To, Smooth(WeatherLayer.Alpha)), S);
		PushMoon(Elevation, TimeOfDay);
		bForceSunStep = false;
		return;
	}
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
		// Directional light points along its forward vector: pitch = -elevation. D72: it turns in steps (each one re-renders
		// the cached shadow pages), unless the clock was just set.
		const FRotator SunRotation(-P.SunElevation, P.SunYaw, 0.f);
		if (bForceSunStep || !LastSunRotation.Equals(SunRotation, SunStepDegrees))
		{
			Sun->SetActorRotation(SunRotation);
			LastSunRotation = SunRotation;
		}
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			// D72: below -4 degrees the sun's light falls off by 0.6 stops a degree. The engine's atmosphere keeps the high sky
			// glowing long after sunset (at -11 degrees it still burnt white against the night's open eye); real twilight
			// fades about this fast.
			const float Twilight = P.SunElevation < -4.f ? FMath::Exp2((P.SunElevation + 4.f) * 0.6f) : 1.f;
			C->SetIntensity(P.SunIlluminance * R.SunIntensityMul * W.SunIntensityMul * Twilight);
			C->SetUseTemperature(true);
			C->SetTemperature(P.SunTemperature);
			C->SetLightColor(R.SunTint, false);
			C->SetLightSourceAngle(P.SunSourceAngle);
			// D72: well below the horizon the sun lights nothing; it stops casting shadows (the moon casts them) and, deeper,
			// stops being drawn at all.
			const int32 Shadow = P.SunElevation > -4.f ? 1 : 0;
			if (Shadow != SunShadowState)
			{
				C->SetCastShadows(Shadow != 0);
				SunShadowState = Shadow;
			}
			const bool bSunVisible = P.SunElevation > -18.f;
			if (C->IsVisible() != bSunVisible)
			{
				C->SetVisibility(bSunVisible);
			}
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
		BaseFogDensity = P.FogDensity * R.FogDensityMul * W.FogDensityMul;
		BaseFogStartCm = P.FogStartDistance;
		F->SetFogHeightFalloff(P.FogHeightFalloff);
		F->SetFogInscatteringColor(Mul(P.FogInscattering, R.FogTint));
		F->SetDirectionalInscatteringColor(Mul(P.FogDirectionalInscattering, R.FogDirectionalTint));
		LastInteriorWeight = -1.f;
		ApplyInteriorFog();
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
	// Without the clock every preset change lights or douses them (as before D72); with it only a change of light does.
	PushLanterns(P.bLanternsLit, DayCycle.Num() == 0);
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

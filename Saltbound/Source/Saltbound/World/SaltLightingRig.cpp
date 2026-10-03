// Saltbound - lighting presets (day / evening ...) applied to the scene's sun, sky, fog and exposure.

#include "World/SaltLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Props/SaltLantern.h"
#include "Saltbound.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdLighting(
		TEXT("sb.Lighting"),
		TEXT("Apply a lighting preset: sb.Lighting Day | Evening"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0)
			{
				return;
			}
			for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
			{
				It->ApplyPreset(FName(*Args[0]));
			}
		}));
}

ASaltLightingRig::ASaltLightingRig()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// Stylised realism (owner reference: Sea of Thieves): golden sun with a large disc (big soft sun
	// reflection on the sea), cool sky fill so shadows read blue, a deep blue sky with little white haze,
	// blue distance haze that fades far islands into the sky, rich but controlled colour (saturation and
	// contrast up a little, cool shadows, warm highlights).
	FSaltLightingPreset Day;
	Day.Name = TEXT("Day");
	Day.SunElevation = 32.f;
	Day.SunTemperature = 5400.f;   // golden, but sun glints still read white
	Day.SunSourceAngle = 2.2f;
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
}

void ASaltLightingRig::BeginPlay()
{
	Super::BeginPlay();
	if (!StartPreset.IsNone())
	{
		ApplyPreset(StartPreset);
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
	ResolveSceneActors();
	if (Sun)
	{
		// Directional light points along its forward vector: pitch = -elevation.
		Sun->SetActorRotation(FRotator(-P->SunElevation, P->SunYaw, 0.f));
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			C->SetIntensity(P->SunIlluminance);
			C->SetUseTemperature(true);
			C->SetTemperature(P->SunTemperature);
			C->SetLightSourceAngle(P->SunSourceAngle);
		}
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->GetLightComponent()->SetIntensity(P->SkyLightIntensity);
		SkyLight->GetLightComponent()->SetLightColor(P->SkyLightTint);
	}
	if (Atmosphere)
	{
		if (USkyAtmosphereComponent* A = Atmosphere->FindComponentByClass<USkyAtmosphereComponent>())
		{
			A->SetRayleighScatteringScale(P->RayleighScale);
			A->SetMieScatteringScale(P->MieScale);
		}
	}
	if (Fog && Fog->GetComponent())
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetFogDensity(P->FogDensity);
		F->SetFogHeightFalloff(P->FogHeightFalloff);
		F->SetFogInscatteringColor(P->FogInscattering);
		F->SetDirectionalInscatteringColor(P->FogDirectionalInscattering);
		F->SetStartDistance(P->FogStartDistance);
	}
	if (PostProcess)
	{
		FPostProcessSettings& S = PostProcess->Settings;
		S.bOverride_AutoExposureMinBrightness = true;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureMinBrightness = P->ExposureMinEV;
		S.AutoExposureMaxBrightness = P->ExposureMaxEV;
		S.AutoExposureBias = P->ExposureBias;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(P->Saturation, P->Saturation, P->Saturation, 1.f);
		S.bOverride_ColorGain = true;
		S.ColorGain = FVector4(P->Gain.R, P->Gain.G, P->Gain.B, 1.f);
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(P->Contrast, P->Contrast, P->Contrast, 1.f);
		S.bOverride_ColorGainShadows = true;
		S.ColorGainShadows = FVector4(P->ShadowGain.R, P->ShadowGain.G, P->ShadowGain.B, 1.f);
		S.bOverride_ColorGainHighlights = true;
		S.ColorGainHighlights = FVector4(P->HighlightGain.R, P->HighlightGain.G, P->HighlightGain.B, 1.f);
	}
	for (TActorIterator<ASaltLantern> It(GetWorld()); It; ++It)
	{
		It->SetLit(P->bLanternsLit);
	}
	CurrentPreset = PresetName;
	UE_LOG(LogSaltbound, Log, TEXT("Applied lighting preset %s"), *PresetName.ToString());
	return true;
}

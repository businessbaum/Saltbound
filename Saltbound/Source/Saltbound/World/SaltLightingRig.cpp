// Saltbound - lighting presets (day / evening ...) applied to the scene's sun, sky, fog and exposure.

#include "World/SaltLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
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

	FSaltLightingPreset Day;
	Day.Name = TEXT("Day");
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
	Evening.Saturation = 0.88f;
	Evening.Gain = FLinearColor(0.97f, 0.98f, 1.05f);
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
	Dusk.Saturation = 0.9f;
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
		}
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->GetLightComponent()->SetIntensity(P->SkyLightIntensity);
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
	}
	for (TActorIterator<ASaltLantern> It(GetWorld()); It; ++It)
	{
		It->SetLit(P->bLanternsLit);
	}
	CurrentPreset = PresetName;
	UE_LOG(LogSaltbound, Log, TEXT("Applied lighting preset %s"), *PresetName.ToString());
	return true;
}

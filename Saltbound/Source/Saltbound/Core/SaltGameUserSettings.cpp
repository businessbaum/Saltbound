// Saltbound - user settings: display (vsync, frame cap, resolution scale, window mode) and look (FOV, mouse).

#include "Core/SaltGameUserSettings.h"

#include "Engine/Engine.h"
#include "HAL/PlatformMisc.h"

namespace
{
	constexpr int32 CurrentSaltSettingsVersion = 1;
}

USaltGameUserSettings* USaltGameUserSettings::Get()
{
	return GEngine ? Cast<USaltGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

float USaltGameUserSettings::DefaultFrameRateLimit()
{
	return (float)FMath::Max(FPlatformMisc::GetMaxRefreshRate(), 60);
}

void USaltGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	SetVSyncEnabled(false);
	SetFrameRateLimit(DefaultFrameRateLimit());
	SetResolutionScaleNormalized(1.f);
	FieldOfView = 80.f;
	MouseSensitivity = 1.f;
	bInvertLookY = false;
}

void USaltGameUserSettings::ApplyDisplay()
{
	if (GIsEditor)
	{
		// The editor owns its window: vsync, frame cap and resolution scale only.
		ApplyNonResolutionSettings();
	}
	else
	{
		ApplySettings(false);
	}
	SaveSettings();
}

bool USaltGameUserSettings::IsVersionValid()
{
	return Super::IsVersionValid() && SaltSettingsVersion == CurrentSaltSettingsVersion;
}

void USaltGameUserSettings::UpdateVersion()
{
	Super::UpdateVersion();
	SaltSettingsVersion = CurrentSaltSettingsVersion;
}

// Saltbound - user settings: display (vsync, frame cap, resolution scale, window mode), look (FOV, mouse), and (D66)
// brightness, sound, the player's keys, the first start and the voyage to continue.

#include "Core/SaltGameUserSettings.h"

#include "AudioDevice.h"
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
	bPromptOnObject = true;
	// D66: the first start and the voyage are kept; the keys go back to their defaults.
	Brightness = 2.2f;
	MasterVolume = 1.f;
	KeyBindings.Reset();
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

void USaltGameUserSettings::ApplyExtras()
{
	if (!GEngine)
	{
		return;
	}
	Brightness = FMath::Clamp(Brightness, BrightnessMin, BrightnessMax);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	GEngine->DisplayGamma = Brightness;
	if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
	{
		Device->SetTransientPrimaryVolume(MasterVolume);
	}
}

FKey USaltGameUserSettings::GetKey(FName Id, const FKey& Default) const
{
	const FSaltKeyBinding* B = KeyBindings.FindByPredicate([Id](const FSaltKeyBinding& K) { return K.Id == Id; });
	return B && B->Key.IsValid() ? B->Key : Default;
}

void USaltGameUserSettings::SetKey(FName Id, const FKey& Key, const FKey& Default)
{
	KeyBindings.RemoveAll([Id](const FSaltKeyBinding& K) { return K.Id == Id; });
	if (Key.IsValid() && Key != Default)
	{
		KeyBindings.Add({ Id, Key });
	}
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

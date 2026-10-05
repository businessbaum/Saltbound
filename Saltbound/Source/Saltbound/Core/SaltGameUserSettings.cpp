// Saltbound - user settings: display (vsync, frame cap, resolution scale, window mode), look (FOV, mouse), and (D66)
// brightness, sound, the player's keys, the first start and the voyage to continue.

#include "Core/SaltGameUserSettings.h"

#include "AudioDevice.h"
#include "Engine/World.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
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
	MusicVolume = 1.f;
	SeaVolume = 1.f;
	EffectsVolume = 1.f;
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

USoundClass* USaltGameUserSettings::SoundGroup(const TCHAR* Name)
{
	return LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("/Game/Saltbound/Audio/%s.%s"), Name, Name));
}

USoundMix* USaltGameUserSettings::VolumeMix()
{
	return LoadObject<USoundMix>(nullptr, TEXT("/Game/Saltbound/Audio/SM_Volumes.SM_Volumes"));
}

void USaltGameUserSettings::ApplyExtras(const UObject* WorldContext)
{
	if (!GEngine)
	{
		return;
	}
	Brightness = FMath::Clamp(Brightness, BrightnessMin, BrightnessMax);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
	SeaVolume = FMath::Clamp(SeaVolume, 0.f, 1.f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.f, 1.f);
	GEngine->DisplayGamma = Brightness;
	// The world's audio device (play-in-editor has one per world), else the main one.
	const UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
	if (!Device)
	{
		Device = GEngine->GetMainAudioDevice();
	}
	if (!Device)
	{
		return;
	}
	Device->SetTransientPrimaryVolume(MasterVolume);
	// The groups: one mix, pushed once per device, its class volumes overridden (a group's volume reaches the classes under it).
	USoundMix* Mix = VolumeMix();
	if (!Mix)
	{
		return;
	}
	const TPair<const TCHAR*, float> Groups[] = { { TEXT("SC_Music"), MusicVolume }, { TEXT("SC_SeaAndWind"), SeaVolume }, { TEXT("SC_Effects"), EffectsVolume } };
	for (const TPair<const TCHAR*, float>& G : Groups)
	{
		if (USoundClass* Class = SoundGroup(G.Key))
		{
			Device->SetSoundMixClassOverride(Mix, Class, G.Value, 1.f, 0.f, true);
		}
	}
	static TSet<Audio::FDeviceId> Pushed;
	if (!Pushed.Contains(Device.GetDeviceID()))
	{
		Pushed.Add(Device.GetDeviceID());
		Device->PushSoundMixModifier(Mix);
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

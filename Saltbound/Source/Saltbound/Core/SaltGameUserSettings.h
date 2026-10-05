// Saltbound - user settings: display (vsync, frame cap, resolution scale, window mode), look (FOV, mouse), the prompt,
// and (D66) brightness, sound, the player's own keys, the first start and the voyage to continue.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "SaltGameUserSettings.generated.h"

/** A key the player chose for an action (Keys and buttons, D66); actions without one keep their default. */
USTRUCT()
struct FSaltKeyBinding
{
	GENERATED_BODY()

	UPROPERTY(Config) FName Id;
	UPROPERTY(Config) FKey Key;
};

/**
 * Saved in GameUserSettings.ini. Defaults: vsync off with the frame rate capped at the display's refresh rate
 * (vsync on paced the game at 55-60 fps), native resolution. The look settings are applied to the player character.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class SALTBOUND_API USaltGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static USaltGameUserSettings* Get();

	virtual void SetToDefaults() override;

	/** Applies the display settings (in the editor only the ones that do not touch the editor's window). */
	void ApplyDisplay();
	/** Applies brightness and the volumes (D66) to the audio device of the context's world (else the main one); called at
	 *  start and whenever they change. */
	void ApplyExtras(const UObject* WorldContext = nullptr);
	/** The sound groups (Tools/ue_scripts/setup_audio.py): every sound belongs to one; a sound with none is an effect. */
	static class USoundClass* SoundGroup(const TCHAR* Name);
	static class USoundMix* VolumeMix();

	/** Frame cap offered by default: the display's refresh rate, at least 60. */
	static float DefaultFrameRateLimit();

	UPROPERTY(Config) float FieldOfView = 80.f;
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertLookY = false;
	/** Where the interaction prompt shows (owner, D60): pinned on the object it belongs to, or low on the screen where
	 *  subtitles go. */
	UPROPERTY(Config) bool bPromptOnObject = true;

	// D66 ------------------------------------------------------------------------------------------------------------
	/** The display gamma (the engine's 2.2 by default): set on the first start with the knot test, and in Settings. */
	UPROPERTY(Config) float Brightness = 2.2f;
	static constexpr float BrightnessMin = 1.6f;
	static constexpr float BrightnessMax = 3.0f;
	/** Everything the game plays, 0..1. */
	UPROPERTY(Config) float MasterVolume = 1.f;
	/** The groups under it, 0..1: music (SC_Music), the sea and the wind (SC_SeaAndWind), effects (SC_Effects). */
	UPROPERTY(Config) float MusicVolume = 1.f;
	UPROPERTY(Config) float SeaVolume = 1.f;
	UPROPERTY(Config) float EffectsVolume = 1.f;
	/** The first start (brightness) has been done. */
	UPROPERTY(Config) bool bFirstStartDone = false;
	/** The voyage (save slot, 0..2) the start menu continues. */
	UPROPERTY(Config) int32 LastVoyage = 0;
	/** The keys the player changed. */
	UPROPERTY(Config) TArray<FSaltKeyBinding> KeyBindings;

	/** The player's key for an action, or its default. */
	FKey GetKey(FName Id, const FKey& Default) const;
	/** Sets (or, with the default, clears) the player's key for an action. */
	void SetKey(FName Id, const FKey& Key, const FKey& Default);

protected:
	virtual bool IsVersionValid() override;
	virtual void UpdateVersion() override;

private:
	/** Bumped when Saltbound's defaults change, so old settings files are reset once. */
	UPROPERTY(Config) int32 SaltSettingsVersion = 0;
};

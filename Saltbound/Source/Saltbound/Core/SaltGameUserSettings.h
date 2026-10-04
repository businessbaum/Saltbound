// Saltbound - user settings: display (vsync, frame cap, resolution scale, window mode) and look (FOV, mouse).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "SaltGameUserSettings.generated.h"

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

	/** Frame cap offered by default: the display's refresh rate, at least 60. */
	static float DefaultFrameRateLimit();

	UPROPERTY(Config) float FieldOfView = 80.f;
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertLookY = false;

protected:
	virtual bool IsVersionValid() override;
	virtual void UpdateVersion() override;

private:
	/** Bumped when Saltbound's defaults change, so old settings files are reset once. */
	UPROPERTY(Config) int32 SaltSettingsVersion = 0;
};

// Saltbound - player controller: input contexts and HUD.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SaltPlayerController.generated.h"

class USaltInputConfig;
class USaltHUDWidget;
class USaltMenuWidget;
class USaltSatchelWidget;

UCLASS()
class SALTBOUND_API ASaltPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASaltPlayerController();

	/** Input actions and mappings (created on first use). */
	USaltInputConfig* GetInputConfig();

	/** Shows or hides the in-game HUD (development captures and recordings). */
	void SetHUDVisible(bool bVisible);

	/** A short notice at the top of the screen ("Game saved"). */
	void ShowNotice(const FText& Text);

	/** Pause menu with the settings (Esc in the game, F10 in the editor, Start on a gamepad): pauses the game. */
	void SetMenuOpen(bool bOpen);
	void ToggleMenu() { SetMenuOpen(!IsMenuOpen()); }
	bool IsMenuOpen() const;
	USaltMenuWidget* GetMenu() const { return MenuWidget; }

	/** The satchel (D50): inventory and map. The world goes on while it is open. Page: 0 inventory, 1 map. */
	void SetSatchelOpen(bool bOpen, int32 Page = 0);
	bool IsSatchelOpen() const;
	USaltSatchelWidget* GetSatchel() const { return SatchelWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USaltMenuWidget> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<USaltSatchelWidget> SatchelWidget;

	void OpenInventory() { SetSatchelOpen(true, 0); }
	void OpenMap() { SetSatchelOpen(true, 1); }

	UPROPERTY(Transient)
	TObjectPtr<USaltInputConfig> InputConfig;

	UPROPERTY(Transient)
	TObjectPtr<USaltHUDWidget> HUDWidget;
};

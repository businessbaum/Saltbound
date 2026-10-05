// Saltbound - player controller: input contexts and HUD.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SaltPlayerController.generated.h"

class USaltInputConfig;
class USaltHUDWidget;
class USaltMenuWidget;
class USaltSatchelWidget;
class USaltStoreWidget;
class USaltInventoryComponent;
class USaltChartWidget;
class ASaltBoat;

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
	USaltHUDWidget* GetHUDWidget() const { return HUDWidget; }

	/** A short notice at the top of the screen ("Game saved"). */
	void ShowNotice(const FText& Text);

	/** Pause menu with the settings (Esc in the game, F10 in the editor, Start on a gamepad): pauses the game. */
	void SetMenuOpen(bool bOpen);
	void ToggleMenu() { SetMenuOpen(!IsMenuOpen()); }
	/** Esc / Start: cancels the tool wheel when it is open, otherwise the pause menu. */
	void HandleMenuKey();
	bool IsMenuOpen() const;
	USaltMenuWidget* GetMenu() const { return MenuWidget; }

	/** The satchel (D50). The world goes on while it is open. */
	void SetSatchelOpen(bool bOpen);
	bool IsSatchelOpen() const;
	USaltSatchelWidget* GetSatchel() const { return SatchelWidget; }

	/** A store's screen (D52): the satchel beside the store; things move with a click. The world goes on. */
	void OpenStore(USaltInventoryComponent* Store);
	void CloseStore();
	bool IsStoreOpen() const;
	USaltStoreWidget* GetStoreWidget() const { return StoreWidget; }

	/** A close look at the chart on a boat's mast (D57): the eyes glide to it (D58), the places in view are named on it.
	 *  E / Esc put it back (the eyes glide back). */
	void OpenChart(ASaltBoat* Boat);
	static constexpr float ChartGlideIn = 0.45f;
	static constexpr float ChartGlideOut = 0.35f;
	void CloseChart();
	bool IsChartOpen() const;
	USaltChartWidget* GetChartWidget() const { return ChartWidget; }

	/** The tool wheel (D55, UI_Design.md section 4): held open with Q or LB; the mouse or the right stick aims at a tool
	 *  (instead of turning the camera), letting go takes it, Esc / B cancel. The world goes on. */
	void OpenToolWheel();
	void CloseToolWheel(bool bTake);
	bool IsToolWheelOpen() const { return bToolWheel; }
	/** Mouse counts (accumulated as a pointer, 40 px dead zone) or the stick (0.35 dead zone); y up. */
	void AimToolWheel(FVector2D Input, bool bStick);
	/** Keys 1 to 5 (Index 0..4): takes that tool, closing the wheel if it is open. */
	void SelectTool(int32 Index);
	/** Does what the tool does: takes it in hand, opens the satchel, or empties the hands. */
	bool UseTool(int32 Index);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USaltMenuWidget> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<USaltSatchelWidget> SatchelWidget;

	UPROPERTY(Transient)
	TObjectPtr<USaltStoreWidget> StoreWidget;

	UPROPERTY(Transient)
	TObjectPtr<USaltChartWidget> ChartWidget;

	/** UI-only input (bOn) with or without the mouse cursor, or back to the game. */
	void SetUIInput(bool bOn, UUserWidget* Focus, bool bCursor = true);

	void OpenInventory() { SetSatchelOpen(true); }

	UPROPERTY(Transient)
	TObjectPtr<USaltInputConfig> InputConfig;

	UPROPERTY(Transient)
	TObjectPtr<USaltHUDWidget> HUDWidget;

	void TakeFromToolWheel() { CloseToolWheel(true); }
	bool bToolWheel = false;
	FVector2D WheelPointer = FVector2D::ZeroVector;
};

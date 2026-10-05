// Saltbound - player controller: input contexts and HUD.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SaltPlayerController.generated.h"

class USaltInputConfig;
class USaltHUDWidget;
class USaltLoadingWidget;
class USaltMenuWidget;
class USaltScreen;
class USaltSettingsWidget;
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
	/** The notice the HUD shows now (tests). */
	FText GetNoticeText() const;

	/** The pause menu (Esc in the game, F10 in the editor, Start on a gamepad): pauses the game. Closing it closes every
	 *  menu screen over the game. */
	void SetMenuOpen(bool bOpen);
	void ToggleMenu() { SetMenuOpen(!IsMenuOpen()); }
	/** Esc / Start: cancels the tool wheel when it is open, otherwise the pause menu. */
	void HandleMenuKey();
	/** Any menu screen is up (the title, the pause menu and what opens from them) or the loading screen. */
	bool IsMenuOpen() const;
	USaltMenuWidget* GetMenu() const { return MenuWidget; }

	// The menu screens (D66) --------------------------------------------------------------------------------------------
	/** Puts a screen on top (it takes the keyboard); the game pauses while any screen on the stack pauses it. */
	void PushScreen(USaltScreen* Screen);
	/** Takes a screen (the top one when null) off the stack; with none left the game goes on. */
	void PopScreen(USaltScreen* Screen = nullptr);
	void CloseScreens();
	USaltScreen* GetTopScreen() const { return Screens.Num() > 0 ? Screens.Last() : nullptr; }
	const TArray<TObjectPtr<USaltScreen>>& GetScreens() const { return Screens; }
	template <class T> T* FindScreen() const
	{
		for (USaltScreen* S : Screens)
		{
			if (T* Found = Cast<T>(S))
			{
				return Found;
			}
		}
		return nullptr;
	}
	bool IsTitleOpen() const;

	/** The start menu over the world (the HUD hidden, the game not paused, the play clock stopped). */
	void ShowTitle();
	/** The first start: the brightness check (then the title). From the settings it returns to them. */
	void OpenBrightness(bool bFromSettings);
	void OpenSettings();
	USaltSettingsWidget* OpenSettingsAt(uint8 Setting);
	void OpenVoyages();
	void OpenKeys();
	/** A question that can't be undone (E yes, Esc no). */
	void Ask(const FText& Title, const FText& Body, const FText& Yes, const FText& No, TFunction<void()> OnYes);

	/** The title's Continue: the world behind is the voyage, so play goes on from there. */
	void ContinueVoyage();
	/** The title's New voyage: on the first empty page, or the voyages screen when all three are taken. */
	void NewVoyage();
	/** Sails on with a voyage (a page of the voyages screen): loads it, or starts a new one on an empty page; asks first when
	 *  something unsaved would be lost. */
	void SailVoyage(int32 Index);
	/** Back to the title: the voyage as last saved behind it (what happened since stays at sea). */
	void ReturnToTitle();
	/** The loading screen over everything: Work runs covered, Then before it fades. */
	void ShowLoading(const FText& Place, const FText& Line, const FString& Picture, TFunction<void()> Work, TFunction<void()> Then);
	bool IsLoading() const;
	USaltLoadingWidget* GetLoading() const { return LoadingWidget; }

	/** Rebuilds the key mappings with the player's keys (after Keys and buttons changed one). */
	void RebuildInputMappings();

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
	/** Pause, input, cursor, HUD and which screens show, after the stack changed. */
	void UpdateScreens();
	/** The packaged game's start (D66): the world as built kept for new voyages, the last voyage loaded, then the first
	 *  start or the title. */
	void StartFromBoot();
	/** One second of play (the voyage's play time), while no menu is up. */
	void TickPlayClock();
	/** Loads a voyage behind the loading screen, then Then. */
	void LoadVoyage(int32 Index, bool bNew, TFunction<void()> Then);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USaltScreen>> Screens;

	UPROPERTY(Transient)
	TObjectPtr<USaltLoadingWidget> LoadingWidget;

	FTimerHandle PlayClock;

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

	/** The HUD as the dev commands want it (it hides while a menu screen is up). */
	bool bHUDWanted = true;
	/** What the screens last set: the pause, and who has the keys (null: the game), so other changes leave them alone. */
	bool bPausedByScreens = false;
	TWeakObjectPtr<UUserWidget> ScreenInput;

	void TakeFromToolWheel() { CloseToolWheel(true); }
	bool bToolWheel = false;
	FVector2D WheelPointer = FVector2D::ZeroVector;
};

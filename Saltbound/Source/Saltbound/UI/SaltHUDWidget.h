// Saltbound - the in-game HUD: focus dot, the interaction prompt (the key on a torn parchment scrap and the action in
// painted lettering, D55), the hold's ink soak, the save sign, the station hint and short notices.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltHUDWidget.generated.h"

class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;
class USaltLandfallWidget;
class USaltToolWheelWidget;
class USaltPaintedText;
class USaltKeyPrompt;

/**
 * Builds its widget tree in code (no widget asset). Deliberately quiet: a small dot that brightens on usable objects,
 * the prompt fixed under it (UI_Design.md sections 7 and 8), the save sign in the bottom-right corner while the game
 * saves (section 14).
 */
UCLASS()
class SALTBOUND_API USaltHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** A short notice at the top of the screen that fades after a few seconds (pickups). */
	void ShowNotice(const FText& Text);

	/** For tests: the prompt text shown now (empty when none), its hold progress and opacity; the save sign's opacity. */
	FText GetShownPrompt() const;
	float GetShownHoldProgress() const { return ShownHold; }
	float GetSaveSignOpacity() const { return SignAlpha; }
	float GetSaveSignShownTime() const { return SignTime; }
	/** The landfall banner (section 9): shown when USaltLandfallSubsystem finds an island. */
	USaltLandfallWidget* GetLandfall() const { return Landfall; }
	/** The tool wheel (section 4), driven by the player controller. */
	USaltToolWheelWidget* GetToolWheel() const { return ToolWheel; }
	/** The spyglass's round view (its opacity follows the zoom). */
	float GetSpyglassMaskOpacity() const;
	/** The controls row as shown (tests): its action, empty when hidden. */
	FText GetShownControls() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** One prompt row; two of them cross-fade when the target changes. */
	struct FPromptRow
	{
		FText Text;
		float Alpha = 0.f;
		float Age = 0.f;
		bool bShowing = false;
	};

	void LayoutRow(int32 Index);
	void TickPrompt(float DeltaSeconds);
	/** D58: the controls in use (a station's, a ladder's) in the key-scrap prompt along the bottom, a status line under it. */
	void TickControls(float DeltaSeconds);
	void TickSaveSign(float DeltaSeconds);
	void HandleSaveStarted();
	void HandleSaveFinished();
	void HandleLandfall(FName PlaceId);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> HudRoot;
	UPROPERTY(Transient) TObjectPtr<UImage> Dot;
	UPROPERTY(Transient) TObjectPtr<USaltKeyPrompt> ControlsRow;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> StatusLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Notice;
	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> RowRoots;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> RowScraps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> RowMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltPaintedText>> RowLabels;
	UPROPERTY(Transient) TObjectPtr<UImage> SignRing;
	UPROPERTY(Transient) TObjectPtr<UImage> SignRose;
	UPROPERTY(Transient) TObjectPtr<USaltLandfallWidget> Landfall;
	UPROPERTY(Transient) TObjectPtr<USaltToolWheelWidget> ToolWheel;
	UPROPERTY(Transient) TObjectPtr<UImage> SpyglassMask;

	FPromptRow Rows[2];
	int32 ActiveRow = 0;
	float ShownHold = 0.f;
	float Clock = 0.f;

	float NoticeTime = 0.f;
	float DotAlpha = 0.35f;
	/** The controls row's keys (a new set makes a new row), whether it shows, and its opacity. */
	TArray<FString> ControlKeys;
	bool bControlsShowing = false;
	float ControlsAlpha = 0.f;

	// The save sign: shown while saving, at least 1.4 s, finishing its step before it fades.
	bool bSaving = false;
	bool bSignOn = false;
	float SignTime = 0.f;
	float SignAlpha = 0.f;
	FDelegateHandle SaveStartedHandle;
	FDelegateHandle SaveFinishedHandle;
	FDelegateHandle LandfallHandle;
};

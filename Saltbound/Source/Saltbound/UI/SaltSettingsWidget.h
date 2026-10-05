// Saltbound - the settings (D66, Docs/Design/Menu_Ideas/Menus.html "Settings"): pages of real settings only, each a row with
// its control; the chosen row is described on the right.

#pragma once

#include "CoreMinimal.h"
#include "UI/SaltMenuKit.h"
#include "SaltSettingsWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

/** Every setting the game has (the pages group them). */
enum class ESaltSetting : uint8
{
	Prompts, Defaults,
	Window, FrameCap, VSync, ResolutionScale, Brightness,
	Quality, Shadows, ViewDistance, FieldOfView,
	Volume,
	MouseSensitivity, InvertLook, Keys,
	MusicVolume, SeaVolume, EffectsVolume,
};

/**
 * Pages: Game (the prompt's place, restore every setting), Display (window, frame cap, vsync, resolution scale, brightness),
 * Graphics (quality Calm / Fair / Fresh / Gale, shadows, view distance, field of view), Audio (master, music, the sea and the wind,
 * effects), Controls
 * (mouse sensitivity, invert look, keys and buttons). Choices step with the small anchors ("< value >", A / D), on / off is
 * the inked word with the rope under it, numbers are the rope with a knot as the handle. Every change applies at once and
 * is saved.
 */
UCLASS()
class SALTBOUND_API USaltSettingsWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltSettingsWidget(const FObjectInitializer& ObjectInitializer);

	/** Goes to the setting's page and chooses its row. */
	void ShowSetting(ESaltSetting Setting);
	/** Steps a choice or a number by Delta, flips a switch (any Delta), opens a link. */
	void Change(ESaltSetting Setting, int32 Delta);
	/** The value as shown. */
	FString Describe(ESaltSetting Setting) const;
	int32 GetPage() const { return CurrentPage; }
	void SetPage(int32 InPage);
	int32 GetPageCount() const;
	int32 GetChosenRow() const { return Row; }

	virtual void OnRevealed() override { Rebuild(); }

protected:
	virtual void Build() override;
	virtual void Navigate(int32 Dx, int32 Dy) override;
	virtual void Accept() override;
	virtual void Page(int32 Dir) override { SetPage(CurrentPage + Dir); }
	virtual bool RawKey(const FKey& Key, bool bRepeat) override;
	virtual void PointerMove(const FVector2D& Design) override;
	virtual void PointerDown(const FVector2D& Design) override;
	virtual void PointerUp(const FVector2D& Design) override { DragRow = INDEX_NONE; }

private:
	enum class EKind : uint8 { Choice, Switch, Number, Link };
	static EKind KindOf(ESaltSetting Setting);
	const TArray<ESaltSetting>& Rows() const;
	void Rebuild();
	void ChooseRow(int32 InRow);
	void Apply(ESaltSetting Setting);
	/** A number's place on its rope (0..1) and its label. */
	float NumberOf(ESaltSetting Setting, FText* OutLabel = nullptr) const;
	void SetNumber(ESaltSetting Setting, float Normalized);
	FText ChoiceOf(ESaltSetting Setting) const;
	bool SwitchOf(ESaltSetting Setting) const;
	int32 RowAt(const FVector2D& Design) const;

	UPROPERTY(Transient) TArray<TObjectPtr<USaltMenuItem>> Tabs;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RowsCanvas;
	UPROPERTY(Transient) TObjectPtr<class USaltPaintedText> DetailTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
	int32 CurrentPage = 0;
	int32 Row = 0;
	int32 DragRow = INDEX_NONE;
};

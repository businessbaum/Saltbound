// Saltbound - the key-scrap prompt as a widget of its own (D58): the keys on torn parchment scraps and the action in
// painted lettering (UI_Design.md section 7), for prompts outside the HUD's focus prompt (the chart's "put it back", a
// station's controls).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltKeyPrompt.generated.h"

class UCanvasPanel;
class UImage;
class USaltPaintedText;

/**
 * Built in code. Place it at the point where the row's middle should be, sized 1 x 1 (the row is drawn round that point,
 * as the HUD's prompt rows are). Each key is a 58 px scrap tilted -3 degrees with its letter inked on it (the key-soak
 * material with no ink, T_UI_KeyLetter_<Key>), 10 px apart; 18 px after the last one the action in IM Fell English, its
 * baseline 12 px under the keys' centre. PlayIn fades it in as the HUD's prompt does (rising 6 px in 0.15 s while the
 * scraps settle from -6 to -3 degrees).
 */
UCLASS()
class SALTBOUND_API USaltKeyPrompt : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The keys (letters with a T_UI_KeyLetter_ picture: A, D, E, F, G, Q, R, S, W) and the action. Call before it is shown. */
	void Setup(const TArray<FString>& InKeys, const FText& InAction, float InTextSize = 36.f);
	void SetAction(const FText& InAction);
	const FText& GetAction() const { return Action; }
	/** Fades the row in from the start. */
	void PlayIn();
	/** The row's width in 1080p pixels. */
	float GetRowWidth() const { return RowWidth; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Layout();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Row;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Scraps;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> Label;

	TArray<FString> Keys;
	FText Action;
	float TextSize = 36.f;
	float RowWidth = 0.f;
	float Age = 1.f;
};

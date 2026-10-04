// Saltbound - the tool wheel (D55, UI_Design.md section 4): a rope ring with six brass medallions, held open with Q.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltToolWheelWidget.generated.h"

class UCanvasPanel;
class UImage;
class USaltPaintedText;

/**
 * Centred on the screen: a light blur and darkening, a soft dark disc, the hemp rope ring (radius 262), and on it six
 * leather medallions with brass bezels lashed to the rope (compass, spyglass, chart, lantern, satchel, empty hands,
 * clockwise from the top; keys 1 to 6 outside them). The chosen one is bigger with an amber halo and a bright rim; its
 * name, hint and "let go of Q to take it" are in the middle. Opens and closes by scaling from 0.9 with a fade (0.12 s),
 * the medallions popping in 20 ms apart. A tool not owned yet shows an empty medallion. The player controller drives
 * it (opening, aiming, taking); this only shows it.
 */
UCLASS()
class SALTBOUND_API USaltToolWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Owned: one flag per tool (SaltTools order). */
	void Open(int32 InSelected, const TArray<bool>& InOwned);
	void Close();
	bool IsOpen() const { return bOpen; }
	void SetSelected(int32 Index);
	int32 GetSelected() const { return Selected; }
	/** 0 closed .. 1 open (the fade). */
	float GetOpenAlpha() const { return Alpha; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void UpdateText();
	void Layout();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Group;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Lashings;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Halos;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Medallions;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Pictures;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Rims;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Keys;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> NameText;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> HintText;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> LetGoText;

	TArray<bool> Owned;
	TArray<float> SelAlpha;
	int32 Selected = 0;
	bool bOpen = false;
	float Alpha = 0.f;
	float OpenTime = 0.f;
};

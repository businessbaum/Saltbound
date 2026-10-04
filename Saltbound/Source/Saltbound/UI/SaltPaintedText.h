// Saltbound - painted lettering over the world (D55, UI_Design.md section 3): salt-white letters graded from top to
// bottom with a speckled paint texture, a dark rim and a soft dark halo, so they read on bright sky and on dark wood.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/SaltUIStyle.h"
#include "SaltPaintedText.generated.h"

class UOverlay;
class UTextBlock;

/**
 * Built in code. The fill is the font material M_UI_PaintedText; the rim is the letters' outline (13 % of the size);
 * the halo is three outline-only copies behind, wider and fainter, a little low (an approximation of the design's
 * blurred shadow, which Slate text cannot draw).
 */
UCLASS()
class SALTBOUND_API USaltPaintedText : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Face and size in 1080p pixels; call before the widget is shown. */
	void Setup(ESaltFace InFace, float InSize, ETextJustify::Type InJustify = ETextJustify::Left, float InLetterSpacing = 0.f);
	void SetText(const FText& InText);
	const FText& GetText() const { return Text; }
	/** Width of the text in 1080p pixels (for laying out a prompt row). */
	float MeasureWidth() const;
	float GetSize() const { return Size; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ApplyFonts();

	UPROPERTY(Transient) TObjectPtr<UOverlay> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Layers;

	ESaltFace Face = ESaltFace::Fell;
	float Size = 36.f;
	float LetterSpacing = 0.f;
	TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Left;
	FText Text;
};

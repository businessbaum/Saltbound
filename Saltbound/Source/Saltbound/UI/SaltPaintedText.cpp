// Saltbound - painted lettering over the world (D55).

#include "UI/SaltPaintedText.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** The halo: outline growth beyond the rim (fraction of the size) and opacity, widest first. */
	constexpr float HaloGrow[] = { 0.26f, 0.15f, 0.07f };
	constexpr float HaloAlpha[] = { 0.035f, 0.07f, 0.12f };
}

void USaltPaintedText::Setup(ESaltFace InFace, float InSize, ETextJustify::Type InJustify, float InLetterSpacing)
{
	Face = InFace;
	Size = InSize;
	Justify = InJustify;
	LetterSpacing = InLetterSpacing;
	ApplyFonts();
}

void USaltPaintedText::SetText(const FText& InText)
{
	Text = InText;
	for (UTextBlock* L : Layers)
	{
		L->SetText(Text);
	}
}

float USaltPaintedText::MeasureWidth() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return 0.f;
	}
	FSlateFontInfo F = SaltUI::Font(Face, Size);
	F.LetterSpacing = FMath::RoundToInt(LetterSpacing * 1000.f / FMath::Max(Size, 1.f));
	return (float)FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, F).X;
}

TSharedRef<SWidget> USaltPaintedText::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		Layers.Reset();
		for (int32 I = 0; I < 4; ++I)
		{
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
			T->SetText(Text);
			UOverlaySlot* S = Root->AddChildToOverlay(T);
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Center);
			Layers.Add(T);
		}
		ApplyFonts();
	}
	return Super::RebuildWidget();
}

void USaltPaintedText::ApplyFonts()
{
	if (Layers.Num() < 4)
	{
		return;
	}
	const int32 Rim = FMath::Max(2, FMath::RoundToInt(Size * 0.13f));
	const int32 Spacing = FMath::RoundToInt(LetterSpacing * 1000.f / FMath::Max(Size, 1.f));
	for (int32 I = 0; I < 4; ++I)
	{
		UTextBlock* T = Layers[I];
		FSlateFontInfo F = SaltUI::Font(Face, Size);
		F.LetterSpacing = Spacing;
		T->SetJustification(Justify);
		if (I < 3)
		{
			// Halo: an outline only (no fill), a little below the letters.
			F.OutlineSettings.OutlineSize = Rim + FMath::RoundToInt(Size * HaloGrow[I]);
			F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, HaloAlpha[I]);
			F.OutlineSettings.bSeparateFillAlpha = true;
			T->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f)));
			T->SetRenderTranslation(FVector2D(0.f, 2.f));
		}
		else
		{
			// The letters: the painted fill and a dark rim (13 % of the size, rgba 18/10/4 at 62 %).
			F.FontMaterial = SaltUI::Material(TEXT("M_UI_PaintedText"));
			F.OutlineSettings.OutlineSize = Rim;
			F.OutlineSettings.OutlineColor = SaltUI::SRGB(18, 10, 4, 0.62f);
			T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
		T->SetFont(F);
	}
}

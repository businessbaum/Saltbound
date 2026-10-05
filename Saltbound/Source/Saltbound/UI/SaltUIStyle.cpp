// Saltbound - the hand-crafted UI style (D50, D55).

#include "UI/SaltUIStyle.h"

#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Core/SaltItems.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* FaceAssets[] = {
		TEXT("FF_FellEnglish_Regular"), TEXT("FF_FellEnglish_Italic"), TEXT("FF_FellEnglishSC_Regular"),
		TEXT("FF_Alegreya_Regular"), TEXT("FF_Alegreya_Italic"), TEXT("FF_Alegreya_Bold"),
	};

	/** One composite font per face, made at runtime from the imported font face (the composite's entries are not
	 *  editable from the import script). FStandaloneCompositeFont keeps the face referenced. */
	TSharedPtr<const FCompositeFont> CompositeFor(ESaltFace Face)
	{
		static TSharedPtr<const FCompositeFont> Cache[UE_ARRAY_COUNT(FaceAssets)];
		const int32 I = (int32)Face;
		if (!Cache[I].IsValid())
		{
			const TCHAR* Name = FaceAssets[I];
			const UFontFace* Asset = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/Saltbound/UI/Fonts/%s.%s"), Name, Name));
			if (!Asset)
			{
				return nullptr;
			}
			TSharedRef<FStandaloneCompositeFont> Font = MakeShared<FStandaloneCompositeFont>();
			FTypefaceEntry& Entry = Font->DefaultTypeface.Fonts.AddDefaulted_GetRef();
			Entry.Name = TEXT("Regular");
			Entry.Font = FFontData(Asset);
			Cache[I] = Font;
		}
		return Cache[I];
	}
}

FSlateFontInfo SaltUI::Font(ESaltFace Face, float Size)
{
	const TSharedPtr<const FCompositeFont> Composite = CompositeFor(Face);
	if (!Composite.IsValid())
	{
		return FCoreStyle::GetDefaultFontStyle(Face == ESaltFace::AlegreyaBold ? "Bold" : "Regular", FMath::RoundToInt(Size * 0.75f));
	}
	// Slate font sizes are points at 96 DPI (1 pt = 4/3 px); the spec gives pixels at 1080p.
	return FSlateFontInfo(Composite, Size * 0.75f, TEXT("Regular"));
}

UMaterialInterface* SaltUI::Material(const TCHAR* Name)
{
	return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Saltbound/UI/Materials/%s.%s"), Name, Name));
}

UTextBlock* SaltUI::FaceText(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, const FLinearColor& Color, ETextJustify::Type Justify)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
	T->SetFont(Font(Face, Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetJustification(Justify);
	T->SetText(Value);
	return T;
}

UTextBlock* SaltUI::InkText(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, ETextJustify::Type Justify, float Alpha)
{
	UTextBlock* T = FaceText(Tree, Value, Face, Size, SRGB(0x3A, 0x26, 0x16, Alpha), Justify);
	// Ink bleed: a faint soft edge round the letters.
	FSlateFontInfo F = T->GetFont();
	F.OutlineSettings.OutlineSize = 1;
	F.OutlineSettings.OutlineColor = SRGB(48, 30, 16, 0.1f);
	T->SetFont(F);
	return T;
}

UOverlay* SaltUI::Deboss(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, float LetterSpacing, ETextJustify::Type Justify,
	UTextBlock** OutDark, UTextBlock** OutLip)
{
	UOverlay* O = Tree->ConstructWidget<UOverlay>();
	auto Layer = [&](const FLinearColor& Color, float Dy)
	{
		UTextBlock* T = FaceText(Tree, Value, Face, Size, Color, Justify);
		FSlateFontInfo F = T->GetFont();
		F.LetterSpacing = FMath::RoundToInt(LetterSpacing * 1000.f / FMath::Max(Size, 1.f));
		T->SetFont(F);
		UOverlaySlot* S = O->AddChildToOverlay(T);
		S->SetPadding(FMargin(0.f, FMath::Max(Dy, 0.f), 0.f, FMath::Max(-Dy, 0.f)));
		S->SetHorizontalAlignment(Justify == ETextJustify::Right ? HAlign_Right : Justify == ETextJustify::Center ? HAlign_Center : HAlign_Left);
		return T;
	};
	UTextBlock* Lip = Layer(SRGB(255, 215, 160, 0.22f), 1.6f);
	UTextBlock* Dark = Layer(SRGB(30, 14, 5, 0.72f), 0.f);
	Dark->SetShadowOffset(FVector2D(0.f, -1.f));
	Dark->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	if (OutDark) { *OutDark = Dark; }
	if (OutLip) { *OutLip = Lip; }
	return O;
}

UWidget* SaltUI::Keycap(UWidgetTree* Tree, const FText& Key, float Size)
{
	const int32 Len = Key.ToString().Len();
	const float W = Size + (Len > 1 ? (Len - 1) * 12.f : 0.f);
	USizeBox* Box = Tree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(W + 8.f * Size / 40.f);
	Box->SetHeightOverride(Size * 48.f / 40.f);
	UOverlay* O = Tree->ConstructWidget<UOverlay>();
	Box->AddChild(O);
	UImage* Cap = Tree->ConstructWidget<UImage>();
	// The texture is a 40 px key in a 48 px frame (shadow); stretched as a box so words widen it.
	Cap->SetBrush(BoxBrush(Tex(TEXT("T_UI_Keycap")), FVector2D(48.f, 48.f) * Size / 40.f, 0.32f));
	O->AddChildToOverlay(Cap);
	UTextBlock* T = FaceText(Tree, Key, ESaltFace::AlegreyaBold, FMath::RoundToFloat(Size * 0.5f), SRGB(0xF3, 0xE6, 0xC8), ETextJustify::Center);
	UOverlaySlot* S = O->AddChildToOverlay(T);
	S->SetHorizontalAlignment(HAlign_Center);
	S->SetVerticalAlignment(VAlign_Center);
	S->SetPadding(FMargin(0.f, 0.f, 0.f, Size * 0.12f));
	return Box;
}

UOverlay* SaltUI::CountCoin(UWidgetTree* Tree, UTextBlock** OutText, float Radius)
{
	UOverlay* O = Tree->ConstructWidget<UOverlay>();
	UImage* Coin = Tree->ConstructWidget<UImage>();
	Coin->SetBrush(ImageBrush(Tex(TEXT("T_UI_Coin")), FVector2D(48.f, 48.f) * Radius / 17.f));
	O->AddChildToOverlay(Coin);
	UTextBlock* T = FaceText(Tree, FText::GetEmpty(), ESaltFace::AlegreyaBold, 19.f, SRGB(0xF3, 0xE6, 0xC8), ETextJustify::Center);
	UOverlaySlot* S = O->AddChildToOverlay(T);
	S->SetHorizontalAlignment(HAlign_Center);
	S->SetVerticalAlignment(VAlign_Center);
	// The coin's centre sits at (23, 22) of its 48 px frame.
	S->SetPadding(FMargin(0.f, 0.f, 2.f * Radius / 17.f, 6.f * Radius / 17.f));
	if (OutText) { *OutText = T; }
	return O;
}

void SaltUI::SetCoinCount(UWidget* Coin, UTextBlock* Text, int32 Count)
{
	if (Coin)
	{
		Coin->SetVisibility(Count >= 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text)
	{
		Text->SetFont(Font(ESaltFace::AlegreyaBold, Count > 9 ? 17.f : 19.f));
		Text->SetText(FText::AsNumber(Count));
	}
}

UOverlay* SaltUI::Burn(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, ETextJustify::Type Justify)
{
	UOverlay* O = Tree->ConstructWidget<UOverlay>();
	const EHorizontalAlignment HAlign = Justify == ETextJustify::Right ? HAlign_Right : Justify == ETextJustify::Center ? HAlign_Center : HAlign_Left;
	// The scorch: outline-only layers of dark brown, widest and faintest first (Slate text cannot blur).
	const float Grow[] = { 0.22f, 0.13f, 0.06f };
	const float Alpha[] = { 0.07f, 0.11f, 0.16f };
	for (int32 I = 0; I < 4; ++I)
	{
		UTextBlock* T = FaceText(Tree, Value, Face, Size, FLinearColor::White, Justify);
		FSlateFontInfo F = T->GetFont();
		if (I < 3)
		{
			F.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(Size * Grow[I]));
			F.OutlineSettings.OutlineColor = SRGB(60, 25, 5, Alpha[I]);
			F.OutlineSettings.bSeparateFillAlpha = true;
			T->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f)));
		}
		else
		{
			// The letters (rgba 36/16/6 at 82 %), their edge a little blurred.
			F.OutlineSettings.OutlineSize = 1;
			F.OutlineSettings.OutlineColor = SRGB(40, 18, 6, 0.35f);
			T->SetColorAndOpacity(FSlateColor(SRGB(36, 16, 6, 0.82f)));
		}
		T->SetFont(F);
		UOverlaySlot* S = O->AddChildToOverlay(T);
		S->SetHorizontalAlignment(HAlign);
		S->SetVerticalAlignment(VAlign_Center);
	}
	return O;
}

void SaltUI::SetLayeredText(UOverlay* Layers, const FText& Value)
{
	for (int32 I = 0; Layers && I < Layers->GetChildrenCount(); ++I)
	{
		if (UTextBlock* T = Cast<UTextBlock>(Layers->GetChildAt(I)))
		{
			T->SetText(Value);
		}
	}
}

UCanvasPanelSlot* SaltUI::At(UCanvasPanel* Canvas, UWidget* W, FVector2D Design, FVector2D Size, FVector2D Alignment)
{
	UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
	S->SetAnchors(FAnchors(0.5f, 0.5f));
	S->SetAlignment(Alignment);
	S->SetPosition(Design - FVector2D(960.f, 540.f));
	if (Size.X > 0.f)
	{
		S->SetSize(Size);
	}
	else
	{
		S->SetAutoSize(true);
	}
	return S;
}

FVector2D SaltUI::DesignPoint(const FGeometry& Geometry, const FVector2D& ScreenPosition)
{
	return Geometry.AbsoluteToLocal(ScreenPosition) - Geometry.GetLocalSize() * 0.5f + FVector2D(960.f, 540.f);
}

void SaltUI::ApplyOpening(UWidget* Contents, UWidget* Backdrop, UBackgroundBlur* Blur, float BlurStrength, float Progress)
{
	const float P = FMath::Clamp(Progress, 0.f, 1.f);
	const float E = 1.f - FMath::Pow(1.f - P, 3.f);
	if (Contents)
	{
		Contents->SetRenderTransformPivot(FVector2D(0.5f, 0.6f));
		Contents->SetRenderOpacity(FMath::Min(P / 0.6f, 1.f));
		Contents->SetRenderTransform(FWidgetTransform(FVector2D(0.f, 36.f * (1.f - E)), FVector2D(0.97f + 0.03f * E), FVector2D::ZeroVector, 0.f));
	}
	if (Backdrop)
	{
		Backdrop->SetRenderOpacity(P);
	}
	if (Blur)
	{
		Blur->SetBlurStrength(BlurStrength * E);
	}
}

UTexture2D* SaltUI::ItemIcon(const FSaltItemDef* Item)
{
	return Item && Item->Icon ? LoadObject<UTexture2D>(nullptr, Item->Icon) : nullptr;
}

UTexture2D* SaltUI::ItemIconShadow(const FSaltItemDef* Item)
{
	if (!Item || !Item->Icon)
	{
		return nullptr;
	}
	const FString Path = FString(Item->Icon).Replace(TEXT("T_Icon_"), TEXT("T_UI_IconShadow_"));
	return LoadObject<UTexture2D>(nullptr, *Path);
}

// Saltbound - the hand-crafted UI style (D50, D55): fonts, ink and paint colours, the generated UI textures as brushes,
// and the small reusable pieces of the design (Docs/Design/UI/UI_Design.md section 3).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UMaterialInterface;
class UOverlay;
class UWidget;
struct FSaltItemDef;

/** The UI typefaces (SIL OFL, Tools/fonts/get_fonts.py): IM Fell English for names, titles, prompts and labels,
 *  Alegreya for descriptions, hints and counts. */
enum class ESaltFace : uint8
{
	Fell,
	FellItalic,
	FellSC,
	Alegreya,
	AlegreyaItalic,
	AlegreyaBold,
};

namespace SaltUI
{
	inline const FLinearColor Ink(0.2f, 0.13f, 0.075f, 1.f);
	inline const FLinearColor InkSoft(0.32f, 0.23f, 0.15f, 1.f);
	inline const FLinearColor ParchmentText(0.95f, 0.88f, 0.72f, 1.f);

	/** A colour from the spec's sRGB value. */
	inline FLinearColor SRGB(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		FLinearColor C = FLinearColor::FromSRGBColor(FColor(R, G, B));
		C.A = A;
		return C;
	}

	/** A UI font at a size in 1080p pixels (UMG DPI 1.0); the faces load on first use. */
	SALTBOUND_API FSlateFontInfo Font(ESaltFace Face, float Size);

	/** A UI texture from /Game/Saltbound/UI/Textures (Tools/textures/gen_ui.py, gen_ui_parts.py, Blender renders). */
	inline UTexture2D* Tex(const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Saltbound/UI/Textures/%s.%s"), Name, Name));
	}

	/** A UI material from /Game/Saltbound/UI/Materials (Tools/ue_scripts/setup_ui_materials.py). */
	SALTBOUND_API UMaterialInterface* Material(const TCHAR* Name);

	/** A 9-slice brush (frames, slots, tabs, keycaps). */
	inline FSlateBrush BoxBrush(UTexture2D* T, FVector2D Size, float Margin = 0.3f)
	{
		FSlateBrush B;
		B.SetResourceObject(T);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Box;
		B.Margin = FMargin(Margin);
		return B;
	}

	inline FSlateBrush ImageBrush(UObject* T, FVector2D Size, bool bTile = false)
	{
		FSlateBrush B;
		B.SetResourceObject(T);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Image;
		B.Tiling = bTile ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
		return B;
	}

	inline UTextBlock* Text(UWidgetTree* Tree, const FText& Value, int32 Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetText(Value);
		return T;
	}

	/** Text in a UI face. */
	SALTBOUND_API UTextBlock* FaceText(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Left);

	/** Ink on paper (#3A2616 at 94 %, its edge a little bled). */
	SALTBOUND_API UTextBlock* InkText(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size,
		ETextJustify::Type Justify = ETextJustify::Left, float Alpha = 0.94f);

	/** Pressed into leather: dark letters with a light lip 1-2 px below. Returns the overlay and the two texts. */
	SALTBOUND_API UOverlay* Deboss(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, float LetterSpacing,
		ETextJustify::Type Justify, UTextBlock** OutDark = nullptr, UTextBlock** OutLip = nullptr);

	/** A small leather key with a brass rim and a cream Alegreya Bold letter (on leather screens). It widens for words. */
	SALTBOUND_API UWidget* Keycap(UWidgetTree* Tree, const FText& Key, float Size = 34.f);

	/** A stack count on a dark-leather coin with a brass rim (shown for 2 or more). Returns the overlay; OutText takes the
	 *  number. */
	SALTBOUND_API UOverlay* CountCoin(UWidgetTree* Tree, UTextBlock** OutText, float Radius = 17.f);
	SALTBOUND_API void SetCoinCount(UWidget* Coin, UTextBlock* Text, int32 Count);

	/** Burned into wood (a store's count): dark brown letters, a little blurred at the edge, in a soft scorch. */
	SALTBOUND_API UOverlay* Burn(UWidgetTree* Tree, const FText& Value, ESaltFace Face, float Size, ETextJustify::Type Justify);
	/** Sets the text of every layer of a layered text (Deboss, Burn). */
	SALTBOUND_API void SetLayeredText(UOverlay* Layers, const FText& Value);

	/** Places a child of a centre-anchored full-screen canvas at 1080p design coordinates (the screen centre is 960, 540).
	 *  A size of zero sizes it to its content. */
	SALTBOUND_API UCanvasPanelSlot* At(UCanvasPanel* Canvas, UWidget* W, FVector2D Design, FVector2D Size, FVector2D Alignment = FVector2D::ZeroVector);

	/** An item's icon (T_Icon_X) and the soft shadow made from it (T_UI_IconShadow_X, Tools/textures/gen_ui_parts.py). */
	SALTBOUND_API UTexture2D* ItemIcon(const FSaltItemDef* Item);
	SALTBOUND_API UTexture2D* ItemIconShadow(const FSaltItemDef* Item);
}

// Saltbound - the hand-crafted UI style (D50): ink and parchment colours, the generated UI textures as brushes, text.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace SaltUI
{
	inline const FLinearColor Ink(0.2f, 0.13f, 0.075f, 1.f);
	inline const FLinearColor InkSoft(0.32f, 0.23f, 0.15f, 1.f);
	inline const FLinearColor ParchmentText(0.95f, 0.88f, 0.72f, 1.f);

	/** A UI texture from /Game/Saltbound/UI/Textures (Tools/textures/gen_ui.py). */
	inline UTexture2D* Tex(const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Saltbound/UI/Textures/%s.%s"), Name, Name));
	}

	/** A 9-slice brush (frames, slots, tabs). */
	inline FSlateBrush BoxBrush(UTexture2D* T, FVector2D Size, float Margin = 0.3f)
	{
		FSlateBrush B;
		B.SetResourceObject(T);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Box;
		B.Margin = FMargin(Margin);
		return B;
	}

	inline FSlateBrush ImageBrush(UTexture2D* T, FVector2D Size, bool bTile = false)
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
}

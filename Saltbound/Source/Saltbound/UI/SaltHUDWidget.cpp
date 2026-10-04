// Saltbound - minimal in-game HUD: focus dot, interaction prompt, station hint.

#include "UI/SaltHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/SaltPlayerCharacter.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor PromptColor(0.96f, 0.92f, 0.84f, 1.f); // warm off-white
	const FText KeyLabel = NSLOCTEXT("Saltbound", "InteractKey", "E");

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 Size)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		Text->SetColorAndOpacity(FSlateColor(PromptColor));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Text->SetJustification(ETextJustify::Center);
		Text->SetRenderOpacity(0.f);
		return Text;
	}
}

TSharedRef<SWidget> USaltHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Dot"));
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.ImageSize = FVector2D(5.f, 5.f);
		Dot->SetBrush(Brush);
		Dot->SetColorAndOpacity(PromptColor);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dot))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetSize(FVector2D(5.f, 5.f));
			S->SetPosition(FVector2D::ZeroVector);
		}

		Prompt = MakeText(WidgetTree, TEXT("Prompt"), 17);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Prompt))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.f));
			S->SetAutoSize(true);
			S->SetPosition(FVector2D(0.f, 36.f));
		}

		Notice = MakeText(WidgetTree, TEXT("Notice"), 16);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Notice))
		{
			S->SetAnchors(FAnchors(0.5f, 0.f));
			S->SetAlignment(FVector2D(0.5f, 0.f));
			S->SetAutoSize(true);
			S->SetPosition(FVector2D(0.f, 48.f));
		}

		Hint = MakeText(WidgetTree, TEXT("Hint"), 14);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Hint))
		{
			S->SetAnchors(FAnchors(0.5f, 1.f));
			S->SetAlignment(FVector2D(0.5f, 1.f));
			S->SetAutoSize(true);
			S->SetPosition(FVector2D(0.f, -56.f));
		}
	}
	return Super::RebuildWidget();
}

void USaltHUDWidget::ShowNotice(const FText& Text)
{
	if (Notice)
	{
		Notice->SetText(Text);
		NoticeTime = 2.5f;
	}
}

void USaltHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Notice)
	{
		NoticeTime = FMath::Max(NoticeTime - InDeltaTime, 0.f);
		Notice->SetRenderOpacity(0.85f * FMath::Clamp(NoticeTime / 0.6f, 0.f, 1.f));
	}

	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const FText PromptText = Player ? Player->GetPromptText() : FText::GetEmpty();
	const FText HintText = Player ? Player->GetHintText() : FText::GetEmpty();
	const bool bAtStation = Player && Player->IsAtStation();
	const bool bFocus = Player && Player->GetFocusedInteractable() != nullptr;

	if (!PromptText.IsEmpty() && !PromptText.EqualTo(LastPrompt))
	{
		LastPrompt = PromptText;
		Prompt->SetText(FText::Format(NSLOCTEXT("Saltbound", "PromptFormat", "[{0}]  {1}"), KeyLabel, PromptText));
	}
	if (!HintText.IsEmpty())
	{
		Hint->SetText(HintText);
	}

	const float Speed = 8.f;
	PromptAlpha = FMath::FInterpTo(PromptAlpha, PromptText.IsEmpty() ? 0.f : 1.f, InDeltaTime, Speed);
	HintAlpha = FMath::FInterpTo(HintAlpha, HintText.IsEmpty() ? 0.f : 0.8f, InDeltaTime, Speed * 0.5f);
	DotAlpha = FMath::FInterpTo(DotAlpha, bAtStation ? 0.f : (bFocus ? 0.95f : 0.3f), InDeltaTime, Speed * 1.5f);

	Prompt->SetRenderOpacity(PromptAlpha);
	Hint->SetRenderOpacity(HintAlpha);
	Dot->SetRenderOpacity(DotAlpha);
	const float DotScale = bFocus ? 1.4f : 1.f;
	Dot->SetRenderScale(FVector2D(DotScale, DotScale));
}

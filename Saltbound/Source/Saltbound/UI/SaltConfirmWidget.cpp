// Saltbound - asking before something can't be undone (D66).

#include "UI/SaltConfirmWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/SaltPlayerController.h"

namespace
{
	// Menus.html: the scrap 44 x 24cqw at (28cqw, 15cqw), turned -0.6 degrees, padded 3 / 3.4cqw; the question in IM Fell
	// English SC 2.4cqw, the text in Alegreya 1.3cqw, the answers at the bottom 3cqw apart.
	const FVector2D AskAt(538.f, 288.f);
	const FVector2D AskSize(845.f, 461.f);
	constexpr float AskPadX = 65.f;
	constexpr float AskPadY = 58.f;
	constexpr float AskKey = 50.f;
}

USaltConfirmWidget::USaltConfirmWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bOverlay = true;
	BackdropKind = EBackdrop::Dim;
	BlurStrength = 6.f;
}

void USaltConfirmWidget::Setup(const FText& InTitle, const FText& InBody, const FText& InYes, const FText& InNo, TFunction<void()> InOnYes)
{
	Title = InTitle;
	Body = InBody;
	Yes = InYes;
	No = InNo;
	OnYes = MoveTemp(InOnYes);
}

void USaltConfirmWidget::Build()
{
	UCanvasPanel* Scrap = WidgetTree->ConstructWidget<UCanvasPanel>();
	SaltUI::At(Contents, Scrap, AskAt, AskSize);
	Scrap->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Scrap->SetRenderTransformAngle(-0.6f);
	UImage* Paper = WidgetTree->ConstructWidget<UImage>();
	Paper->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_MenuScrap")), AskSize + FVector2D(60.f, 60.f)));
	if (UCanvasPanelSlot* S = Scrap->AddChildToCanvas(Paper))
	{
		S->SetPosition(FVector2D(-30.f, -30.f));
		S->SetSize(AskSize + FVector2D(60.f, 60.f));
	}
	UTextBlock* Q = SaltUI::InkText(WidgetTree, Title, ESaltFace::FellSC, 46.f);
	if (UCanvasPanelSlot* S = Scrap->AddChildToCanvas(Q))
	{
		S->SetPosition(FVector2D(AskPadX, SaltMenu::TextTop(AskPadY, 46.f)));
		S->SetAutoSize(true);
	}
	UTextBlock* B = SaltUI::InkText(WidgetTree, Body, ESaltFace::Alegreya, 25.f);
	B->SetAutoWrapText(true);
	if (UCanvasPanelSlot* S = Scrap->AddChildToCanvas(B))
	{
		S->SetPosition(FVector2D(AskPadX, AskPadY + 46.f + 25.f));
		S->SetSize(FVector2D(AskSize.X - 2.f * AskPadX, 160.f));
	}
	// The answers, inked on the scrap: [E] yes   [Esc] no.
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto Answer = [this, Row](const FString& Key, const FText& Word, float Left)
	{
		UHorizontalBoxSlot* K = Row->AddChildToHorizontalBox(SaltMenu::KeyScrap(WidgetTree, Key, AskKey));
		K->SetVerticalAlignment(VAlign_Center);
		K->SetPadding(FMargin(Left, 0.f, 0.f, 0.f));
		UHorizontalBoxSlot* W = Row->AddChildToHorizontalBox(SaltUI::InkText(WidgetTree, Word, ESaltFace::Fell, 27.f));
		W->SetVerticalAlignment(VAlign_Center);
		W->SetPadding(FMargin(13.f, 0.f, 0.f, 0.f));
	};
	Answer(TEXT("E"), Yes, 0.f);
	Answer(TEXT("Esc"), No, 58.f);
	if (UCanvasPanelSlot* S = Scrap->AddChildToCanvas(Row))
	{
		S->SetPosition(FVector2D(AskPadX, AskSize.Y - AskPadY - AskKey));
		S->SetAutoSize(true);
	}
	// Where a click answers yes (the left part of the answers' row).
	YesRight = AskAt.X + AskPadX + AskKey + 13.f + 27.f * 0.5f * FMath::Max(Yes.ToString().Len(), 4) + 29.f;
}

void USaltConfirmWidget::Confirm()
{
	TFunction<void()> Run = MoveTemp(OnYes);
	OnYes = nullptr;
	if (ASaltPlayerController* PC = GetSaltController())
	{
		PC->PopScreen(this);
	}
	if (Run)
	{
		Run();
	}
}

void USaltConfirmWidget::PointerDown(const FVector2D& Design)
{
	// A click on the answers' row: the left part says yes, the right part no; outside the scrap is no.
	const float RowTop = AskAt.Y + AskSize.Y - AskPadY - AskKey - 10.f;
	if (Design.Y >= RowTop && Design.Y <= RowTop + AskKey + 20.f && Design.X >= AskAt.X + AskPadX - 10.f && Design.X <= AskAt.X + AskSize.X)
	{
		if (Design.X <= YesRight)
		{
			Confirm();
		}
		else
		{
			Back();
		}
	}
	else if (Design.X < AskAt.X || Design.X > AskAt.X + AskSize.X || Design.Y < AskAt.Y || Design.Y > AskAt.Y + AskSize.Y)
	{
		Back();
	}
}

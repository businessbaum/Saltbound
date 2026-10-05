// Saltbound - the voyages (D66).

#include "UI/SaltVoyagesWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltPaintedText.h"

namespace
{
	// Menus.html "Voyages": the pages 24cqw wide at 13, 38 and 63cqw, 12cqw down, padded 1.3cqw (1.6 at the bottom); the
	// picture 12.6cqw high; the chosen page lifts 1cqw and turns -1 degree, the others lie a little askew.
	constexpr float VoyCardWidth = 461.f;
	constexpr float VoyCardHeight = 448.f;
	constexpr float VoyCardTop = 230.f;
	constexpr float VoyPad = 25.f;
	constexpr float VoyPictureHeight = 242.f;
	const float VoyCardLeft[] = { 250.f, 730.f, 1210.f };
	const float VoyCardTilt[] = { 0.f, 1.f, -0.6f };
}

USaltVoyagesWidget::USaltVoyagesWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	BackdropKind = EBackdrop::Blurred;
	bShadeLeft = false;
}

void USaltVoyagesWidget::Build()
{
	SaltUI::At(Contents, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltVoyages", "Title", "Voyages"), ESaltFace::Fell, 54.f, ETextJustify::Center),
		FVector2D(960.f, 86.f + 27.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	Cards.Reset();
	for (int32 I = 0; I < USaltSaveSubsystem::VoyageCount; ++I)
	{
		UCanvasPanel* Card = WidgetTree->ConstructWidget<UCanvasPanel>();
		SaltUI::At(Contents, Card, FVector2D(VoyCardLeft[I], VoyCardTop), FVector2D(VoyCardWidth, VoyCardHeight));
		Card->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Cards.Add(Card);
	}
}

void USaltVoyagesWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
	// Start on the voyage in play (or the one the title would continue).
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	ChooseVoyage(Saves && Saves->GetActiveVoyage() != INDEX_NONE ? Saves->GetActiveVoyage() : 0);
}

void USaltVoyagesWidget::Refresh()
{
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	Infos.Reset();
	Pictures.Reset();
	for (int32 I = 0; I < USaltSaveSubsystem::VoyageCount; ++I)
	{
		Infos.Add(Saves ? Saves->GetVoyageInfo(I) : FSaltVoyageInfo());
		Pictures.Add(!Infos[I].Picture.IsEmpty() ? FImageUtils::ImportFileAsTexture2D(Infos[I].Picture) : nullptr);
	}
	for (int32 I = 0; I < Cards.Num(); ++I)
	{
		UCanvasPanel* Card = Cards[I];
		Card->ClearChildren();
		const FSaltVoyageInfo& Info = Infos[I];
		auto Add = [Card](UWidget* W, FVector2D Pos, FVector2D Size)
		{
			UCanvasPanelSlot* S = Card->AddChildToCanvas(W);
			S->SetPosition(Pos);
			if (Size.X > 0.f)
			{
				S->SetSize(Size);
			}
			else
			{
				S->SetAutoSize(true);
			}
			return S;
		};
		// The page: torn parchment (a 9-slice of the 460 x 600 page in its 30 px shadow frame).
		UImage* Paper = WidgetTree->ConstructWidget<UImage>();
		Paper->SetBrush(SaltUI::BoxBrush(SaltUI::Tex(TEXT("T_UI_MenuSlot")), FVector2D(520.f, 660.f), 0.16f));
		Add(Paper, FVector2D(-30.f, -30.f), FVector2D(VoyCardWidth + 60.f, VoyCardHeight + 60.f));

		const FVector2D PicPos(VoyPad, VoyPad);
		const FVector2D PicSize(VoyCardWidth - 2.f * VoyPad, VoyPictureHeight);
		if (Info.bExists && Pictures[I])
		{
			// The print: a little sepia and faded at its edge.
			UImage* Pic = WidgetTree->ConstructWidget<UImage>();
			FSlateBrush B = SaltUI::ImageBrush(Pictures[I], PicSize);
			B.TintColor = FSlateColor(FLinearColor(1.f, 0.9f, 0.76f));
			Pic->SetBrush(B);
			Add(Pic, PicPos, PicSize);
			UImage* Edge = WidgetTree->ConstructWidget<UImage>();
			FSlateBrush E = SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_Vignette")), PicSize);
			E.TintColor = FSlateColor(SaltUI::SRGB(60, 30, 10, 0.7f));
			Edge->SetBrush(E);
			Add(Edge, PicPos, PicSize);
		}
		else
		{
			// No picture (an empty page, or a save from before pictures): a pencilled frame.
			UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
			FSlateBrush B;
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.TintColor = FSlateColor(FLinearColor::Transparent);
			B.OutlineSettings.Color = FSlateColor(SaltUI::SRGB(58, 38, 22, 0.35f));
			B.OutlineSettings.Width = 3.f;
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			B.OutlineSettings.CornerRadii = FVector4(2.f, 2.f, 2.f, 2.f);
			Frame->SetBrush(B);
			Frame->SetHorizontalAlignment(HAlign_Center);
			Frame->SetVerticalAlignment(VAlign_Center);
			UTextBlock* T = SaltUI::FaceText(WidgetTree, Info.bExists ? NSLOCTEXT("SaltVoyages", "NoPicture", "No picture of this one.")
				: NSLOCTEXT("SaltVoyages", "Empty", "An empty page.\nStart a new voyage here."), ESaltFace::FellItalic, 25.f, SaltMenu::FadedInk(), ETextJustify::Center);
			Frame->SetContent(T);
			Add(Frame, PicPos, PicSize);
		}
		float Y = VoyPad + VoyPictureHeight + 15.f;
		UTextBlock* Name = SaltUI::InkText(WidgetTree, FText::Format(NSLOCTEXT("SaltVoyages", "Name", "Voyage {0}"), SaltMenu::Roman(I + 1)), ESaltFace::FellSC, 38.f);
		Name->SetRenderOpacity(Info.bExists ? 1.f : 0.5f);
		Add(Name, FVector2D(VoyPad, SaltMenu::TextTop(Y, 38.f)), FVector2D::ZeroVector);
		Y += 38.f + 15.f;
		FText Line = NSLOCTEXT("SaltVoyages", "NotStarted", "Not started");
		if (Info.bExists)
		{
			Line = SaltMenu::VoyageLine(Info);
		}
		UTextBlock* Where = SaltUI::InkText(WidgetTree, Line, ESaltFace::Alegreya, 21.f);
		Where->SetRenderOpacity(Info.bExists ? 1.f : 0.6f);
		Add(Where, FVector2D(VoyPad, SaltMenu::TextTop(Y, 21.f, 1.35f)), FVector2D::ZeroVector);
		Y += 21.f * 1.35f + 15.f;
		if (Info.bExists)
		{
			UTextBlock* Small = SaltUI::FaceText(WidgetTree, FText::Format(NSLOCTEXT("SaltVoyages", "Small", "{0} · saved {1}"), SaltMenu::PlayTime(Info.PlayTime),
				SaltMenu::When(Info.SavedAt)), ESaltFace::FellItalic, 19.f, SaltMenu::FadedInk());
			Add(Small, FVector2D(VoyPad, SaltMenu::TextTop(Y, 19.f, 1.2f)), FVector2D::ZeroVector);
		}
	}
	Layout();
}

bool USaltVoyagesWidget::CanScrap() const
{
	if (!Infos.IsValidIndex(Voyage) || !Infos[Voyage].bExists)
	{
		return false;
	}
	// The voyage in play stays while it is played.
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	return !(bInPlay && Saves && Saves->GetActiveVoyage() == Voyage);
}

void USaltVoyagesWidget::Layout()
{
	for (int32 I = 0; I < Cards.Num(); ++I)
	{
		const bool bChosen = I == Voyage;
		Cards[I]->SetRenderTransform(FWidgetTransform(FVector2D(0.f, bChosen ? -19.f : 0.f), FVector2D(1.f, 1.f), FVector2D::ZeroVector,
			bChosen ? -1.f : VoyCardTilt[I]));
		Cards[I]->SetRenderOpacity(bChosen ? 1.f : 0.86f);
	}
	TArray<SaltMenu::FHint> H;
	const bool bExists = Infos.IsValidIndex(Voyage) && Infos[Voyage].bExists;
	H.Add({ { TEXT("E") }, bExists ? NSLOCTEXT("SaltVoyages", "HintSail", "sail on") : NSLOCTEXT("SaltVoyages", "HintNew", "start a new voyage") });
	PlaceHints({}, 0.f, true);
	// The controls row, with the soaking X when the page can be scrapped.
	HintsRow->ClearChildren();
	UHorizontalBox* Row = SaltMenu::Hints(WidgetTree, H);
	HintsRow->AddChildToHorizontalBox(Row);
	if (CanScrap())
	{
		SoakMid = nullptr;
		UMaterialInstanceDynamic* Mid = nullptr;
		UHorizontalBoxSlot* KS = HintsRow->AddChildToHorizontalBox(SaltMenu::SoakKey(WidgetTree, this, TEXT("X"), 56.f, Mid));
		KS->SetVerticalAlignment(VAlign_Center);
		KS->SetPadding(FMargin(46.f, 0.f, 0.f, 0.f));
		SoakMid = Mid;
		UHorizontalBoxSlot* LS = HintsRow->AddChildToHorizontalBox(SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltVoyages", "HintScrap", "hold to scrap this voyage"),
			ESaltFace::Fell, 30.f));
		LS->SetVerticalAlignment(VAlign_Center);
		LS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}
	else
	{
		SoakMid = nullptr;
	}
	UHorizontalBox* Back = SaltMenu::Hints(WidgetTree, { { { TEXT("Esc") }, NSLOCTEXT("SaltVoyages", "HintBack", "back") } });
	HintsRow->AddChildToHorizontalBox(Back)->SetPadding(FMargin(46.f, 0.f, 0.f, 0.f));
}

void USaltVoyagesWidget::ChooseVoyage(int32 Index)
{
	if (Index >= 0 && Index < USaltSaveSubsystem::VoyageCount)
	{
		Voyage = Index;
		bHolding = false;
		Hold = 0.f;
		Layout();
	}
}

void USaltVoyagesWidget::SailOn()
{
	if (ASaltPlayerController* PC = GetSaltController())
	{
		PC->SailVoyage(Voyage);
	}
}

bool USaltVoyagesWidget::Scrap()
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (!Saves || !CanScrap())
	{
		return false;
	}
	const bool bOk = Saves->DeleteVoyage(Voyage);
	bHolding = false;
	Hold = 0.f;
	Refresh();
	return bOk;
}

void USaltVoyagesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	if (!SoakMid)
	{
		return;
	}
	// The ink rises while X is held and drains back quickly when it is let go.
	Hold = bHolding ? Hold + InDeltaTime / ScrapHold : FMath::Max(Hold - InDeltaTime / 0.25f, 0.f);
	SoakMid->SetScalarParameterValue(TEXT("Progress"), FMath::Clamp(Hold, 0.f, 1.f));
	SoakMid->SetScalarParameterValue(TEXT("Time"), Time);
	if (Hold >= 1.f)
	{
		Scrap();
	}
}

void USaltVoyagesWidget::Navigate(int32 Dx, int32 Dy)
{
	if (Dx != 0)
	{
		ChooseVoyage(FMath::Clamp(Voyage + Dx, 0, USaltSaveSubsystem::VoyageCount - 1));
	}
}

bool USaltVoyagesWidget::RawKey(const FKey& Key, bool bRepeat)
{
	if (Key == EKeys::X || Key == EKeys::Gamepad_FaceButton_Left)
	{
		if (!bRepeat && CanScrap())
		{
			bHolding = true;
		}
		return true;
	}
	return false;
}

void USaltVoyagesWidget::RawKeyUp(const FKey& Key)
{
	if (Key == EKeys::X || Key == EKeys::Gamepad_FaceButton_Left)
	{
		bHolding = false;
	}
}

int32 USaltVoyagesWidget::CardAt(const FVector2D& Design) const
{
	for (int32 I = 0; I < USaltSaveSubsystem::VoyageCount; ++I)
	{
		if (Design.X >= VoyCardLeft[I] && Design.X <= VoyCardLeft[I] + VoyCardWidth && Design.Y >= VoyCardTop - 20.f && Design.Y <= VoyCardTop + VoyCardHeight)
		{
			return I;
		}
	}
	return INDEX_NONE;
}

void USaltVoyagesWidget::PointerMove(const FVector2D& Design)
{
	const int32 I = CardAt(Design);
	if (I != INDEX_NONE && I != Voyage)
	{
		ChooseVoyage(I);
	}
}

void USaltVoyagesWidget::PointerDown(const FVector2D& Design)
{
	const int32 I = CardAt(Design);
	if (I != INDEX_NONE)
	{
		ChooseVoyage(I);
		SailOn();
	}
}

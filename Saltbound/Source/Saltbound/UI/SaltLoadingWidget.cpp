// Saltbound - the loading screen (D66).

#include "UI/SaltLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "ImageUtils.h"
#include "UI/SaltMenuKit.h"
#include "UI/SaltPaintedText.h"
#include "UI/SaltUIStyle.h"

namespace
{
	// Menus.html "Loading" at 1080p: the name 5cqw in with its line box 8.6cqw above the bottom, the line under it at 5.6cqw;
	// the log scrap 34 x 8.4cqw at (52cqw, 4.4cqw from the bottom), turned -0.8 degrees; the knot 6.6cqw, 4 / 3.6cqw from the
	// bottom-right corner. The knot turns as the save sign: 45 degrees every 0.7 s, settling with a little overshoot.
	const FVector2D LoadLogAt(998.f, 835.f);
	const FVector2D LoadLogSize(653.f, 161.f);
	constexpr float LoadKnotSize = 127.f;
	const FVector2D LoadKnotCentre(1920.f - 77.f - 63.5f, 1080.f - 69.f - 63.5f);
	constexpr float LoadKnotStep = 0.7f;

	/** Lines from an old sailor's log: true things about sailing here. */
	const FText& LogLine(int32 Index)
	{
		static const FText Lines[] = {
			NSLOCTEXT("SaltLog", "Log1", "\"Take in the sail before the beach. In the shallows she runs aground, and no wind will free her.\""),
			NSLOCTEXT("SaltLog", "Log2", "\"Make her fast at a mooring and the day is written in the log. What is not written stays at sea.\""),
			NSLOCTEXT("SaltLog", "Log3", "\"The chart on the mast shows the water round her, and only that. It goes where she goes.\""),
			NSLOCTEXT("SaltLog", "Log4", "\"An island is not on my chart until I have been close enough to see its stones.\""),
			NSLOCTEXT("SaltLog", "Log5", "\"Let the anchor go and she swings to it. Raise it slowly; the windlass will not be hurried.\""),
			NSLOCTEXT("SaltLog", "Log6", "\"The wind comes round in the afternoon. Watch the sail, not the sky.\""),
		};
		return Lines[((Index % UE_ARRAY_COUNT(Lines)) + UE_ARRAY_COUNT(Lines)) % UE_ARRAY_COUNT(Lines)];
	}
}

void USaltLoadingWidget::Begin(const FText& Place, const FText& Line, const FString& InPicture, TFunction<void()> InWork, TFunction<void()> InThen)
{
	PlaceName = Place;
	PlaceLine = Line;
	PicturePath = InPicture;
	Work = MoveTemp(InWork);
	Then = MoveTemp(InThen);
	Phase = EPhase::In;
	Age = 0.f;
	PhaseAge = 0.f;
	WorkFrames = -1;
	if (PlaceText)
	{
		PlaceText->SetText(PlaceName);
		LineText->SetText(PlaceLine);
	}
}

TSharedRef<SWidget> USaltLoadingWidget::RebuildWidget()
{
	SetIsFocusable(true);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		// The sea at night under everything; the picture of the place over it when there is one.
		SaltMenu::FullScreenColor(WidgetTree, Root, SaltUI::SRGB(7, 15, 22));
		PictureTexture = !PicturePath.IsEmpty() ? FImageUtils::ImportFileAsTexture2D(PicturePath) : nullptr;
		if (PictureTexture)
		{
			Picture = SaltMenu::FullScreen(WidgetTree, Root, PictureTexture);
		}
		SaltMenu::FullScreen(WidgetTree, Root, SaltUI::Tex(TEXT("T_UI_Vignette")));
		SaltMenu::FullScreen(WidgetTree, Root, SaltMenu::Gradient(TEXT("LoadBottom"), false,
			{ { 0.f, 0, 0, 0, 0.f }, { 0.55f, 0, 0, 0, 0.f }, { 1.f, 3, 7, 10, 0.85f } }));

		PlaceText = SaltMenu::Paint(WidgetTree, PlaceName, ESaltFace::FellSC, 77.f, ETextJustify::Left, 4.6f);
		SaltUI::At(Root, PlaceText, FVector2D(96.f, 1080.f - 165.f - 0.775f * 77.f), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
		LineText = SaltUI::FaceText(WidgetTree, PlaceLine, ESaltFace::FellItalic, 26.f, SaltUI::SRGB(0xD8, 0xDC, 0xD8));
		LineText->SetShadowOffset(FVector2D(0.f, 2.f));
		LineText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		SaltUI::At(Root, LineText, FVector2D(100.f, 1080.f - 108.f - 0.775f * 26.f), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));

		// The log: a torn scrap with a line from an old log.
		UCanvasPanel* Log = WidgetTree->ConstructWidget<UCanvasPanel>();
		SaltUI::At(Root, Log, LoadLogAt, LoadLogSize);
		Log->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Log->SetRenderTransformAngle(-0.8f);
		UImage* Paper = WidgetTree->ConstructWidget<UImage>();
		Paper->SetBrush(SaltUI::BoxBrush(SaltUI::Tex(TEXT("T_UI_MenuScrap")), FVector2D(905.f, 520.f), 0.12f));
		if (UCanvasPanelSlot* S = Log->AddChildToCanvas(Paper))
		{
			S->SetPosition(FVector2D(-30.f, -30.f));
			S->SetSize(LoadLogSize + FVector2D(60.f, 60.f));
		}
		UTextBlock* Head = SaltUI::InkText(WidgetTree, NSLOCTEXT("SaltLog", "Head", "From an old log"), ESaltFace::FellSC, 18.f, ETextJustify::Left, 0.7f);
		FSlateFontInfo F = Head->GetFont();
		F.LetterSpacing = 150;
		Head->SetFont(F);
		if (UCanvasPanelSlot* S = Log->AddChildToCanvas(Head))
		{
			S->SetPosition(FVector2D(38.f, 22.f));
			S->SetAutoSize(true);
		}
		LogText = SaltUI::InkText(WidgetTree, LogLine(FMath::RandRange(0, 99)), ESaltFace::Alegreya, 25.f);
		LogText->SetAutoWrapText(true);
		if (UCanvasPanelSlot* S = Log->AddChildToCanvas(LogText))
		{
			S->SetPosition(FVector2D(38.f, 50.f));
			S->SetSize(FVector2D(LoadLogSize.X - 76.f, LoadLogSize.Y - 56.f));
		}

		// The knot: the ring still, the rose turning over it.
		auto Layer = [this](const TCHAR* Texture)
		{
			UImage* Img = WidgetTree->ConstructWidget<UImage>();
			Img->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(Texture), FVector2D(LoadKnotSize, LoadKnotSize)));
			SaltUI::At(Root, Img, LoadKnotCentre, FVector2D(LoadKnotSize, LoadKnotSize), FVector2D(0.5f, 0.5f));
			Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			return Img;
		};
		Ring = Layer(TEXT("T_UI_SaveKnot_Ring"));
		Rose = Layer(TEXT("T_UI_SaveKnot_Rose"));
		Rose->SetRenderTransformPivot(SaltUI::RosePivot);
		Root->SetRenderOpacity(0.f);
	}
	return Super::RebuildWidget();
}

void USaltLoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Root)
	{
		return;
	}
	// A hitch while the save is applied must not jump the fades.
	const float Dt = FMath::Min(InDeltaTime, 1.f / 20.f);
	Age += Dt;
	PhaseAge += Dt;
	switch (Phase)
	{
	case EPhase::In:
		Root->SetRenderOpacity(FMath::Clamp(PhaseAge / FadeIn, 0.f, 1.f));
		if (PhaseAge >= FadeIn)
		{
			Phase = EPhase::Hold;
			PhaseAge = 0.f;
			WorkFrames = 0;
		}
		break;
	case EPhase::Hold:
		Root->SetRenderOpacity(1.f);
		// The work one frame after the screen is fully covered (so that frame is drawn), then a few frames for the world to
		// settle behind the veil.
		if (WorkFrames == 1 && Work)
		{
			TFunction<void()> Run = MoveTemp(Work);
			Work = nullptr;
			Run();
		}
		++WorkFrames;
		if (WorkFrames > 6 && Age >= FMath::Max(MinShown, HoldFor))
		{
			// Out first, so what Then opens (the title, or play) gets the keys.
			Phase = EPhase::Out;
			PhaseAge = 0.f;
			if (Then)
			{
				TFunction<void()> Next = MoveTemp(Then);
				Then = nullptr;
				Next();
			}
		}
		break;
	case EPhase::Out:
		Root->SetRenderOpacity(1.f - FMath::Clamp(PhaseAge / FadeOut, 0.f, 1.f));
		if (PhaseAge >= FadeOut)
		{
			Phase = EPhase::Done;
			RemoveFromParent();
		}
		break;
	case EPhase::Done:
		break;
	}
	const float Steps = Age / LoadKnotStep;
	const int32 Whole = FMath::FloorToInt(Steps);
	const float U = Steps - Whole;
	Rose->SetRenderTransformAngle(45.f * (Whole + 1.f - FMath::Exp(-7.f * U) * FMath::Cos(11.f * U)));
}

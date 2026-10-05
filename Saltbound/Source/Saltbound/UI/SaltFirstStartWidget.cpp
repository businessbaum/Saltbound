// Saltbound - the first start: brightness (D66).

#include "UI/SaltFirstStartWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SaltGameUserSettings.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltPaintedText.h"

namespace
{
	// Menus.html "First start" at 1080p on #040608: "Before you sail" at 6cqw, the instruction at 10cqw, three knots 13.6cqw
	// across spread over 64cqw from 18cqw at 19cqw down (#3a3d40, #111518, #06090b), their words in grey italics under them;
	// the rope 40cqw wide at (30cqw, 39cqw).
	constexpr float FirstKnotSize = 262.f;
	constexpr float FirstKnotTop = 365.f;
	const float FirstKnotX[] = { 346.f + 131.f, 960.f, 1575.f - 131.f };
	const FColor FirstKnotColour[] = { FColor(0x3A, 0x3D, 0x40), FColor(0x11, 0x15, 0x18), FColor(0x06, 0x09, 0x0B) };
	const FVector2D FirstRopeAt(576.f, 749.f);
	constexpr float FirstRopeWidth = 700.f;
	constexpr float FirstRopeHeight = 19.f;
	constexpr float FirstKnotHandle = 92.f;
	constexpr float FirstStep = 0.1f;
}

USaltFirstStartWidget::USaltFirstStartWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	BackdropKind = EBackdrop::Cover;
	bPausesGame = true;
}

void USaltFirstStartWidget::Build()
{
	if (const USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		Before = S->Brightness;
	}
	SaltMenu::FullScreenColor(WidgetTree, Backdrop, SaltUI::SRGB(0x04, 0x06, 0x08));
	SaltUI::At(Contents, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltFirst", "Before", "Before you sail"), ESaltFace::FellSC, 25.f, ETextJustify::Center, 7.5f),
		FVector2D(960.f, 115.f + 19.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	SaltUI::At(Contents, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltFirst", "Turn", "Turn the brightness until the middle knot is only just visible."),
		ESaltFace::Fell, 42.f, ETextJustify::Center), FVector2D(960.f, 192.f + 32.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));

	static const FText Words[] = { NSLOCTEXT("SaltFirst", "Clear", "clearly seen"), NSLOCTEXT("SaltFirst", "Just", "only just"),
		NSLOCTEXT("SaltFirst", "NotAtAll", "not at all") };
	for (int32 I = 0; I < 3; ++I)
	{
		// The knot as flat shapes in one colour (the brightness is the display gamma, so the screen itself changes).
		for (const TCHAR* Layer : { TEXT("T_UI_MenuKnotMask_Ring"), TEXT("T_UI_MenuKnotMask_Rose") })
		{
			UImage* Img = WidgetTree->ConstructWidget<UImage>();
			FSlateBrush B = SaltUI::ImageBrush(SaltUI::Tex(Layer), FVector2D(FirstKnotSize, FirstKnotSize));
			B.TintColor = FSlateColor(FLinearColor::FromSRGBColor(FirstKnotColour[I]));
			Img->SetBrush(B);
			SaltUI::At(Contents, Img, FVector2D(FirstKnotX[I], FirstKnotTop), FVector2D(FirstKnotSize, FirstKnotSize), FVector2D(0.5f, 0.f));
		}
		UTextBlock* T = SaltUI::FaceText(WidgetTree, Words[I], ESaltFace::FellItalic, 23.f, SaltUI::SRGB(0x7A, 0x82, 0x86), ETextJustify::Center);
		SaltUI::At(Contents, T, FVector2D(FirstKnotX[I], FirstKnotTop + FirstKnotSize + 19.f + 14.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	}

	Slider = WidgetTree->ConstructWidget<UCanvasPanel>();
	UCanvasPanelSlot* S = Contents->AddChildToCanvas(Slider);
	S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	S->SetOffsets(FMargin(0.f));
	Layout();

	PlaceHints({ { { TEXT("A"), TEXT("D") }, NSLOCTEXT("SaltFirst", "HintBrightness", "brightness") },
		{ { TEXT("E") }, NSLOCTEXT("SaltFirst", "HintRight", "that's right") } }, 0.f, true);
}

void USaltFirstStartWidget::Layout()
{
	if (!Slider)
	{
		return;
	}
	Slider->ClearChildren();
	const USaltGameUserSettings* S = USaltGameUserSettings::Get();
	const float B = S ? S->Brightness : 2.2f;
	const float V = (B - USaltGameUserSettings::BrightnessMin) / (USaltGameUserSettings::BrightnessMax - USaltGameUserSettings::BrightnessMin);
	const float Mid = FirstRopeAt.Y + 19.f;
	UImage* Track = SaltMenu::Rope(WidgetTree, FirstRopeHeight);
	Track->SetRenderOpacity(0.35f);
	SaltUI::At(Slider, Track, FVector2D(FirstRopeAt.X, Mid - FirstRopeHeight * 0.5f), FVector2D(FirstRopeWidth, FirstRopeHeight));
	SaltUI::At(Slider, SaltMenu::Rope(WidgetTree, FirstRopeHeight), FVector2D(FirstRopeAt.X, Mid - FirstRopeHeight * 0.5f),
		FVector2D(FirstRopeWidth * V, FirstRopeHeight));
	SaltUI::At(Slider, SaltMenu::Knot(WidgetTree, FirstKnotHandle), FVector2D(FirstRopeAt.X + FirstRopeWidth * V, Mid - FirstKnotHandle * 0.3f),
		FVector2D(FirstKnotHandle, FirstKnotHandle * 0.5f), FVector2D(0.5f, 0.f));
	// The number: how far from the engine's 2.2, signed.
	const float Offset = B - 2.2f;
	SaltUI::At(Slider, SaltMenu::Paint(WidgetTree, FText::FromString(FString::Printf(TEXT("%s%.1f"), Offset > 0.04f ? TEXT("+") : TEXT(""),
		FMath::Abs(Offset) < 0.04f ? 0.f : Offset)), ESaltFace::Fell, 25.f), FVector2D(FirstRopeAt.X + FirstRopeWidth + 29.f, Mid), FVector2D::ZeroVector,
		FVector2D(0.f, 0.5f));
}

void USaltFirstStartWidget::SetBrightness(float Value)
{
	if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		S->Brightness = FMath::Clamp(FMath::RoundToFloat(Value / FirstStep) * FirstStep, USaltGameUserSettings::BrightnessMin, USaltGameUserSettings::BrightnessMax);
		S->ApplyExtras(this);
	}
	Layout();
}

void USaltFirstStartWidget::Step(int32 Delta)
{
	if (const USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		SetBrightness(S->Brightness + Delta * FirstStep);
	}
}

void USaltFirstStartWidget::Navigate(int32 Dx, int32 Dy)
{
	if (Dx != 0)
	{
		Step(Dx);
	}
}

void USaltFirstStartWidget::PointerMove(const FVector2D& Design)
{
	if (bDragging)
	{
		const float V = FMath::Clamp((Design.X - FirstRopeAt.X) / FirstRopeWidth, 0.f, 1.f);
		SetBrightness(FMath::Lerp(USaltGameUserSettings::BrightnessMin, USaltGameUserSettings::BrightnessMax, V));
	}
}

void USaltFirstStartWidget::PointerDown(const FVector2D& Design)
{
	if (Design.Y >= FirstRopeAt.Y - 30.f && Design.Y <= FirstRopeAt.Y + 70.f && Design.X >= FirstRopeAt.X - 30.f && Design.X <= FirstRopeAt.X + FirstRopeWidth + 30.f)
	{
		bDragging = true;
		PointerMove(Design);
	}
}

void USaltFirstStartWidget::Done()
{
	if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		S->bFirstStartDone = true;
		S->SaveSettings();
	}
	ASaltPlayerController* PC = GetSaltController();
	if (!PC)
	{
		return;
	}
	if (bFromSettings)
	{
		PC->PopScreen(this);
	}
	else
	{
		PC->ShowTitle();
	}
}

void USaltFirstStartWidget::Back()
{
	if (bFromSettings)
	{
		// Esc from the settings: the brightness as it was.
		SetBrightness(Before);
		if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
		{
			S->SaveSettings();
		}
		Super::Back();
	}
	else
	{
		// The first start: Esc keeps what is set, as E.
		Done();
	}
}

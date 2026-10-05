// Saltbound - the tool wheel (D55, UI_Design.md section 4).

#include "UI/SaltToolWheelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Core/SaltTools.h"
#include "Components/HorizontalBox.h"
#include "UI/SaltMenuKit.h"
#include "UI/SaltPaintedText.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

namespace
{
	// The layout at 1080p (section 4; the pictures are Tools/textures/ui_parts.js): the ring's centre and radius, the
	// medallions' radii (70, chosen 84) evenly round it from the top (the design had six; five since the chart moved to
	// the mast, D57), the keys at R + r + 34.
	const FVector2D WheelCentre(960.f, 540.f);
	constexpr float WheelRadius = 262.f;
	constexpr float MedallionR = 70.f;
	constexpr float MedallionSelR = 84.f;

	int32 WheelSlotCount() { return FMath::Max(SaltTools::All().Num(), 1); }
	float SlotAngle(int32 I) { return FMath::DegreesToRadians(-90.f + 360.f / WheelSlotCount() * I); }

	void PlaceAt(UWidget* W, FVector2D Centre, FVector2D Size)
	{
		if (UCanvasPanelSlot* S = W ? Cast<UCanvasPanelSlot>(W->Slot) : nullptr)
		{
			S->SetPosition(Centre - FVector2D(960.f, 540.f));
			if (Size.X > 0.f)
			{
				S->SetSize(Size);
			}
		}
	}
}

TSharedRef<SWidget> USaltToolWheelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetVisibility(ESlateVisibility::Collapsed);
		WidgetTree->RootWidget = Root;
		auto FullScreen = [](UCanvasPanel* Into, UWidget* W)
		{
			UCanvasPanelSlot* S = Into->AddChildToCanvas(W);
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		};
		// A light blur and darkening of the world (it keeps running). The design's 2 px blur reads like Slate's strength 4.
		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
		Blur->SetBlurStrength(4.f);
		FullScreen(Root, Blur);
		UImage* Dark = WidgetTree->ConstructWidget<UImage>();
		Dark->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.22f));
		FullScreen(Root, Dark);
		UImage* Vignette = WidgetTree->ConstructWidget<UImage>();
		Vignette->SetBrush(ImageBrush(Tex(TEXT("T_UI_Vignette")), FVector2D(256.f, 144.f)));
		Vignette->SetRenderOpacity(0.65f / 0.7f);
		FullScreen(Root, Vignette);

		// The wheel itself scales from 0.9 when it opens.
		Group = WidgetTree->ConstructWidget<UCanvasPanel>();
		Group->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		FullScreen(Root, Group);
		auto Picture = [this](const TCHAR* Texture, FVector2D Size, FVector2D Centre)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>();
			if (Texture)
			{
				I->SetBrush(ImageBrush(Tex(Texture), Size));
			}
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			I->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			At(Group, I, Centre, Size, FVector2D(0.5f, 0.5f));
			return I;
		};
		Picture(TEXT("T_UI_WheelRing"), FVector2D(720.f, 720.f), WheelCentre);
		Lashings.Reset();
		for (int32 I = 0; I < WheelSlotCount() * 2; ++I)
		{
			Lashings.Add(Picture(TEXT("T_UI_WheelLashing"), FVector2D(80.f, 48.f), WheelCentre));
		}
		Halos.Reset();
		Medallions.Reset();
		Pictures.Reset();
		Rims.Reset();
		Keys.Reset();
		const TArray<FSaltToolDef>& Tools = SaltTools::All();
		for (int32 I = 0; I < WheelSlotCount(); ++I)
		{
			Halos.Add(Picture(TEXT("T_UI_WheelHalo"), FVector2D(360.f, 360.f), WheelCentre));
		}
		for (int32 I = 0; I < WheelSlotCount(); ++I)
		{
			Medallions.Add(Picture(TEXT("T_UI_WheelMedallion"), FVector2D(240.f, 240.f), WheelCentre));
			Pictures.Add(Picture(Tools.IsValidIndex(I) ? Tools[I].Picture : nullptr, FVector2D(180.f, 180.f), WheelCentre));
			Rims.Add(Picture(TEXT("T_UI_WheelRim"), FVector2D(240.f, 240.f), WheelCentre));
			UWidget* Key = Keycap(WidgetTree, FText::AsNumber(I + 1), 30.f);
			Key->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			At(Group, Key, WheelCentre, FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
			Keys.Add(Key);
		}
		// The middle: the tool's name (painted, 64 px, baseline at the centre - 6), its hint (italic 28 px), a divider with
		// a small turquoise diamond, "let go of Q to take it" (22 px). Widgets centre on the line: baseline - 0.271 em for
		// IM Fell English, - 0.389 em for Alegreya.
		NameText = CreateWidget<USaltPaintedText>(this, USaltPaintedText::StaticClass());
		NameText->Setup(ESaltFace::Fell, 64.f, ETextJustify::Center);
		At(Group, NameText, FVector2D(960.f, 534.f - 0.271f * 64.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
		HintText = CreateWidget<USaltPaintedText>(this, USaltPaintedText::StaticClass());
		HintText->Setup(ESaltFace::FellItalic, 28.f, ETextJustify::Center);
		HintText->SetPlainColor(SRGB(0xE3, 0xD3, 0xB2));
		At(Group, HintText, FVector2D(960.f, 578.f - 0.271f * 28.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
		Picture(TEXT("T_UI_WheelDivider"), FVector2D(240.f, 20.f), FVector2D(960.f, 602.f));
		LetGoText = CreateWidget<USaltPaintedText>(this, USaltPaintedText::StaticClass());
		LetGoText->Setup(ESaltFace::Alegreya, 22.f, ETextJustify::Center);
		LetGoText->SetPlainColor(SRGB(0xCD, 0xBF, 0x9F));
		At(Group, LetGoText, FVector2D(960.f, 640.f - 0.389f * 22.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
		for (USaltPaintedText* T : { NameText.Get(), HintText.Get(), LetGoText.Get() })
		{
			T->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		// Along the bottom: Q "hold, aim, let go" . 1 - 5 "pick directly".
		UImage* Hints = WidgetTree->ConstructWidget<UImage>();
		Hints->SetBrush(ImageBrush(Tex(TEXT("T_UI_WheelHints")), FVector2D(720.f, 90.f)));
		Hints->SetVisibility(ESlateVisibility::HitTestInvisible);
		At(Group, Hints, FVector2D(600.f, 950.f), FVector2D(720.f, 90.f));
		HintsPicture = Hints;
		SelAlpha.Init(0.f, WheelSlotCount());
		Owned.Init(true, WheelSlotCount());
	}
	return Super::RebuildWidget();
}

void USaltToolWheelWidget::RefreshKeys()
{
	const FString Key = SaltMenu::ShownKey(TEXT("Q"));
	if (Key == HintsKey || !HintsPicture)
	{
		return;
	}
	HintsKey = Key;
	if (HintsLive)
	{
		HintsLive->RemoveFromParent();
		HintsLive = nullptr;
	}
	const bool bPainted = Key == TEXT("Q");
	HintsPicture->SetVisibility(bPainted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bPainted)
	{
		HintsLive = SaltMenu::KeycapHints(WidgetTree, { { { Key }, NSLOCTEXT("Saltbound", "WheelHintHold", "hold, aim, let go") },
			{ { TEXT("1"), TEXT("-"), TEXT("5") }, NSLOCTEXT("Saltbound", "WheelHintPick", "pick directly") } });
		At(Group, HintsLive, FVector2D(960.f, 995.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	}
}

void USaltToolWheelWidget::Open(int32 InSelected, const TArray<bool>& InOwned)
{
	RefreshKeys();
	Owned = InOwned;
	Owned.SetNum(WheelSlotCount());
	for (int32 I = 0; I < Pictures.Num(); ++I)
	{
		// A tool not owned yet: the medallion stays empty.
		Pictures[I]->SetVisibility(Owned[I] ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (!bOpen)
	{
		OpenTime = 0.f;
		SelAlpha.Init(0.f, WheelSlotCount());
	}
	bOpen = true;
	Selected = FMath::Clamp(InSelected, 0, WheelSlotCount() - 1);
	if (SelAlpha.IsValidIndex(Selected))
	{
		SelAlpha[Selected] = 1.f;
	}
	if (Root)
	{
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	UpdateText();
	Layout();
}

void USaltToolWheelWidget::Close()
{
	bOpen = false;
}

void USaltToolWheelWidget::SetSelected(int32 Index)
{
	if (Index >= 0 && Index < WheelSlotCount() && Index != Selected)
	{
		Selected = Index;
		UpdateText();
	}
}

void USaltToolWheelWidget::UpdateText()
{
	const TArray<FSaltToolDef>& Tools = SaltTools::All();
	if (!NameText || !Tools.IsValidIndex(Selected))
	{
		return;
	}
	const bool bHave = Owned.IsValidIndex(Selected) && Owned[Selected];
	NameText->SetText(Tools[Selected].Name);
	HintText->SetText(bHave ? Tools[Selected].Hint : NSLOCTEXT("Saltbound", "ToolNotFound", "Not found yet"));
	LetGoText->SetText(bHave ? FText::Format(NSLOCTEXT("Saltbound", "ToolLetGoKey", "let go of {0} to take it"), FText::FromString(SaltMenu::ShownKey(TEXT("Q"))))
		: FText::GetEmpty());
}

void USaltToolWheelWidget::Layout()
{
	for (int32 I = 0; I < WheelSlotCount() && I < Medallions.Num(); ++I)
	{
		const float A = SlotAngle(I);
		const float E = FMath::InterpEaseInOut(0.f, 1.f, SelAlpha[I], 2.f);
		const float R = FMath::Lerp(MedallionR, MedallionSelR, E);
		const FVector2D C = WheelCentre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * WheelRadius;
		const float K = R / MedallionSelR;
		// They pop in 20 ms apart as the wheel opens.
		const float Pop = FMath::Clamp((OpenTime - 0.02f * I) / 0.1f, 0.f, 1.f);
		const float PopScale = FMath::Lerp(0.7f, 1.f, 1.f - FMath::Square(1.f - Pop));
		PlaceAt(Medallions[I], C, FVector2D(240.f, 240.f) * K);
		PlaceAt(Pictures[I], C, FVector2D(180.f, 180.f) * K);
		PlaceAt(Rims[I], C, FVector2D(240.f, 240.f) * K);
		PlaceAt(Halos[I], C, FVector2D(360.f, 360.f) * K);
		for (UImage* W : { Medallions[I].Get(), Pictures[I].Get(), Rims[I].Get() })
		{
			W->SetRenderScale(FVector2D(PopScale, PopScale));
		}
		Medallions[I]->SetRenderOpacity(Pop);
		Pictures[I]->SetRenderOpacity(Pop);
		Rims[I]->SetRenderOpacity(E * Pop);
		Halos[I]->SetRenderOpacity(E * Pop);
		// Its two lashings on the rope, just beyond its rim.
		for (int32 S = 0; S < 2; ++S)
		{
			const float LA = A + (S == 0 ? -1.f : 1.f) * (R + 12.f) / WheelRadius;
			UImage* L = Lashings[I * 2 + S];
			PlaceAt(L, WheelCentre + FVector2D(FMath::Cos(LA), FMath::Sin(LA)) * WheelRadius, FVector2D::ZeroVector);
			L->SetRenderTransformAngle(FMath::RadiansToDegrees(LA));
		}
		PlaceAt(Keys[I], WheelCentre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (WheelRadius + R + 34.f), FVector2D::ZeroVector);
		Keys[I]->SetRenderOpacity(Pop);
	}
}

void USaltToolWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Root || (!bOpen && Alpha <= 0.f))
	{
		return;
	}
	OpenTime += InDeltaTime;
	Alpha = FMath::FInterpConstantTo(Alpha, bOpen ? 1.f : 0.f, InDeltaTime, 1.f / 0.12f);
	for (int32 I = 0; I < SelAlpha.Num(); ++I)
	{
		SelAlpha[I] = FMath::FInterpConstantTo(SelAlpha[I], I == Selected ? 1.f : 0.f, InDeltaTime, 1.f / 0.1f);
	}
	const float E = 1.f - FMath::Square(1.f - Alpha);
	Root->SetRenderOpacity(Alpha);
	Group->SetRenderScale(FVector2D(1.f, 1.f) * FMath::Lerp(0.9f, 1.f, E));
	Layout();
	if (!bOpen && Alpha <= 0.f)
	{
		Root->SetVisibility(ESlateVisibility::Collapsed);
	}
}

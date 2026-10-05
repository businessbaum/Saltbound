// Saltbound - a close look at the chart on the mast (D57).

#include "UI/SaltChartWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Boat/SaltBoat.h"
#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SaltPlaces.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

namespace
{
	// The sheet at 1080p: 640 x 813 (the mast sheet's 0.26 x 0.33 m), centred.
	const FVector2D ChartSheetSize(USaltChartWidget::SheetWidth, USaltChartWidget::SheetWidth * SaltChart::SheetAspect);
	const FVector2D ChartSheetTopLeft = FVector2D(960.f, 520.f) - ChartSheetSize * 0.5f;
	constexpr float ScaleMetres = 200.f;
}

USaltChartWidget::USaltChartWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> USaltChartWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;
		auto FullScreen = [Canvas](UWidget* W)
		{
			UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		};
		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
		Blur->SetBlurStrength(6.f);
		FullScreen(Blur);
		UImage* Dark = WidgetTree->ConstructWidget<UImage>();
		Dark->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		FullScreen(Dark);
		UImage* Vignette = WidgetTree->ConstructWidget<UImage>();
		Vignette->SetBrush(ImageBrush(Tex(TEXT("T_UI_Vignette")), FVector2D(256.f, 144.f)));
		FullScreen(Vignette);

		// The sheet's shadow, then the sheet: the live chart (M_UI_Chart, the same as on the mast).
		UImage* Shadow = WidgetTree->ConstructWidget<UImage>();
		Shadow->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		At(Canvas, Shadow, ChartSheetTopLeft + FVector2D(10.f, 16.f), ChartSheetSize);
		Sheet = WidgetTree->ConstructWidget<UImage>();
		if (UMaterialInterface* M = Material(TEXT("M_UI_Chart")))
		{
			Sheet->SetBrushFromMaterial(M);
			SheetMaterial = Sheet->GetDynamicMaterial();
		}
		At(Canvas, Sheet, ChartSheetTopLeft, ChartSheetSize);

		// The places, named in ink where they lie (shown only while on the sheet).
		PlaceLabels.Reset();
		for (const FSaltPlace& P : SaltPlaces::All())
		{
			UTextBlock* L = InkText(WidgetTree, P.Name, ESaltFace::FellSC, 24.f, ETextJustify::Center);
			L->SetVisibility(ESlateVisibility::Collapsed);
			At(Canvas, L, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(0.5f, 0.f));
			PlaceLabels.Add(L);
		}
		// North at the top, the scale at the bottom left.
		UTextBlock* North = InkText(WidgetTree, NSLOCTEXT("Saltbound", "ChartNorth", "N"), ESaltFace::FellSC, 30.f, ETextJustify::Center);
		At(Canvas, North, FVector2D(960.f, ChartSheetTopLeft.Y + 22.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f));
		const float Bar = ChartSheetSize.X * ScaleMetres / SaltChart::WindowM;
		const FVector2D BarAt(ChartSheetTopLeft.X + 44.f, ChartSheetTopLeft.Y + ChartSheetSize.Y - 52.f);
		UImage* Rule = WidgetTree->ConstructWidget<UImage>();
		Rule->SetColorAndOpacity(SRGB(0x3A, 0x26, 0x16, 0.9f));
		At(Canvas, Rule, BarAt, FVector2D(Bar, 3.f));
		for (const float X : { 0.f, Bar - 2.f })
		{
			UImage* Tick = WidgetTree->ConstructWidget<UImage>();
			Tick->SetColorAndOpacity(SRGB(0x3A, 0x26, 0x16, 0.9f));
			At(Canvas, Tick, BarAt + FVector2D(X, -6.f), FVector2D(2.f, 15.f));
		}
		UTextBlock* ScaleText = InkText(WidgetTree, FText::Format(NSLOCTEXT("Saltbound", "ChartScale", "{0} m"), FText::AsNumber((int32)ScaleMetres)),
			ESaltFace::FellItalic, 22.f);
		At(Canvas, ScaleText, BarAt + FVector2D(Bar + 12.f, -16.f), FVector2D::ZeroVector);

		// Along the bottom: [E] put it back.
		UHorizontalBox* Hint = WidgetTree->ConstructWidget<UHorizontalBox>();
		Hint->AddChildToHorizontalBox(Keycap(WidgetTree, FText::FromString(TEXT("E")), 38.f))->SetVerticalAlignment(VAlign_Center);
		UTextBlock* HintText = FaceText(WidgetTree, NSLOCTEXT("Saltbound", "ChartPutBack", "put the chart back"), ESaltFace::Alegreya, 27.f,
			SRGB(0xF0, 0xE2, 0xC4));
		HintText->SetShadowOffset(FVector2D(0.f, 2.f));
		HintText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		if (UHorizontalBoxSlot* S = Hint->AddChildToHorizontalBox(HintText))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(12.f, 0.f, 0.f, 4.f));
		}
		At(Canvas, Hint, FVector2D(960.f, 1012.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	}
	return Super::RebuildWidget();
}

void USaltChartWidget::Update()
{
	const ASaltBoat* B = Boat.Get();
	if (!B || !SheetMaterial)
	{
		return;
	}
	Centre = B->GetChartCentre();
	SheetMaterial->SetScalarParameterValue(TEXT("CentreU"), (float)Centre.X);
	SheetMaterial->SetScalarParameterValue(TEXT("CentreV"), (float)Centre.Y);
	SheetMaterial->SetScalarParameterValue(TEXT("Heading"), FMath::DegreesToRadians(B->GetYawDeg()));
	// Each place where it lies on the sheet: (its chart uv - the centre) / the window, in sheet pixels.
	const float Span = SaltChart::WindowM / SaltChart::SizeM;
	const TArray<FSaltPlace>& Places = SaltPlaces::All();
	for (int32 I = 0; I < PlaceLabels.Num() && I < Places.Num(); ++I)
	{
		const FVector2D UV = SaltChart::ToUV(FVector(Places[I].CentreM.X * 100.0, Places[I].CentreM.Y * 100.0, 0.0));
		const FVector2D Q = (UV - Centre) / Span;                                // sheet widths from the middle
		const FVector2D P = FVector2D(960.f, 520.f) + FVector2D(Q.X, Q.Y) * ChartSheetSize.X + FVector2D(0.f, 18.f);
		const bool bOn = FMath::Abs(P.X - 960.f) < ChartSheetSize.X * 0.5f - 70.f && FMath::Abs(P.Y - 520.f) < ChartSheetSize.Y * 0.5f - 50.f;
		PlaceLabels[I]->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(PlaceLabels[I]->Slot))
		{
			S->SetPosition(P - FVector2D(960.f, 540.f));
		}
	}
}

TArray<FName> USaltChartWidget::GetNamedPlaces() const
{
	TArray<FName> Out;
	const TArray<FSaltPlace>& Places = SaltPlaces::All();
	for (int32 I = 0; I < PlaceLabels.Num() && I < Places.Num(); ++I)
	{
		if (PlaceLabels[I]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Out.Add(Places[I].Id);
		}
	}
	return Out;
}

void USaltChartWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Update();
}

FReply USaltChartWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::E || Key == EKeys::Escape || Key == EKeys::F10 || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_FaceButton_Left)
	{
		if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
		{
			PC->CloseChart();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// Saltbound - a close look at the chart on the mast (D57; D58: the eyes glide to the sheet, the marks are laid on it).

#include "UI/SaltChartWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Boat/SaltBoat.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SaltPlaces.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltKeyPrompt.h"
#include "UI/SaltMenuKit.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

namespace
{
	// The marks are laid out on the sheet in its own frame, 640 x 813 (the mast sheet's 0.26 x 0.33 m), and that frame is
	// laid over the sheet where it is on screen.
	const FVector2D ChartSheetSize(USaltChartWidget::SheetWidth, USaltChartWidget::SheetWidth * SaltChart::SheetAspect);
	const FVector2D ChartSheetMiddle = ChartSheetSize * 0.5f;
	constexpr float ScaleMetres = 200.f;
	constexpr float ChartInkTime = 0.3f;

	UCanvasPanelSlot* OnSheet(UCanvasPanel* Sheet, UWidget* W, FVector2D At, FVector2D Size, FVector2D Alignment = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* S = Sheet->AddChildToCanvas(W);
		S->SetAlignment(Alignment);
		S->SetPosition(At);
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

		// The sheet's marks, in the sheet's frame (moved and scaled onto the sheet every frame).
		SheetLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
		SheetLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
		SheetLayer->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SheetLayer->SetRenderOpacity(0.f);
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(SheetLayer))
		{
			S->SetAnchors(FAnchors(0.f, 0.f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetSize(ChartSheetSize);
		}

		// The places, named in ink where they lie (shown only while on the sheet).
		PlaceLabels.Reset();
		for (const FSaltPlace& P : SaltPlaces::All())
		{
			UTextBlock* L = InkText(WidgetTree, P.Name, ESaltFace::FellSC, 24.f, ETextJustify::Center);
			L->SetVisibility(ESlateVisibility::Collapsed);
			OnSheet(SheetLayer, L, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(0.5f, 0.f));
			PlaceLabels.Add(L);
		}
		// North at the top, the scale at the bottom left.
		UTextBlock* North = InkText(WidgetTree, NSLOCTEXT("Saltbound", "ChartNorth", "N"), ESaltFace::FellSC, 30.f, ETextJustify::Center);
		OnSheet(SheetLayer, North, FVector2D(ChartSheetMiddle.X, 22.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f));
		const float Bar = ChartSheetSize.X * ScaleMetres / SaltChart::WindowM;
		const FVector2D BarAt(44.f, ChartSheetSize.Y - 52.f);
		UImage* Rule = WidgetTree->ConstructWidget<UImage>();
		Rule->SetColorAndOpacity(SRGB(0x3A, 0x26, 0x16, 0.9f));
		OnSheet(SheetLayer, Rule, BarAt, FVector2D(Bar, 3.f));
		for (const float X : { 0.f, Bar - 2.f })
		{
			UImage* Tick = WidgetTree->ConstructWidget<UImage>();
			Tick->SetColorAndOpacity(SRGB(0x3A, 0x26, 0x16, 0.9f));
			OnSheet(SheetLayer, Tick, BarAt + FVector2D(X, -6.f), FVector2D(2.f, 15.f));
		}
		UTextBlock* ScaleText = InkText(WidgetTree, FText::Format(NSLOCTEXT("Saltbound", "ChartScale", "{0} m"), FText::AsNumber((int32)ScaleMetres)),
			ESaltFace::FellItalic, 22.f);
		OnSheet(SheetLayer, ScaleText, BarAt + FVector2D(Bar + 12.f, -16.f), FVector2D::ZeroVector);

		// Along the bottom, the default prompt (owner, D58): E on its scrap, "Put the chart back" in painted lettering.
		PutBack = CreateWidget<USaltKeyPrompt>(this, USaltKeyPrompt::StaticClass());
		PutBack->Setup({ TEXT("E") }, NSLOCTEXT("Saltbound", "ChartPutBack", "Put the chart back"));
		PutBack->SetVisibility(ESlateVisibility::Collapsed);
		At(Canvas, PutBack, FVector2D(960.f, 1000.f), FVector2D(1.f, 1.f));
	}
	return Super::RebuildWidget();
}

void USaltChartWidget::Update()
{
	const ASaltBoat* B = Boat.Get();
	if (!B)
	{
		return;
	}
	Centre = B->GetChartCentre();
	// Each place where it lies on the sheet: (its chart uv - the centre) / the window, in sheet widths from the middle.
	const float Span = SaltChart::WindowM / SaltChart::SizeM;
	const TArray<FSaltPlace>& Places = SaltPlaces::All();
	for (int32 I = 0; I < PlaceLabels.Num() && I < Places.Num(); ++I)
	{
		const FVector2D UV = SaltChart::ToUV(FVector(Places[I].CentreM.X * 100.0, Places[I].CentreM.Y * 100.0, 0.0));
		const FVector2D Q = (UV - Centre) / Span;
		const FVector2D P = ChartSheetMiddle + Q * ChartSheetSize.X + FVector2D(0.f, 18.f);
		const bool bOn = FMath::Abs(P.X - ChartSheetMiddle.X) < ChartSheetMiddle.X - 70.f && FMath::Abs(P.Y - ChartSheetMiddle.Y) < ChartSheetMiddle.Y - 50.f;
		PlaceLabels[I]->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(PlaceLabels[I]->Slot))
		{
			S->SetPosition(P);
		}
	}
}

void USaltChartWidget::PlaceOnSheet()
{
	const ASaltBoat* B = Boat.Get();
	APlayerController* PC = GetOwningPlayer();
	UCanvasPanelSlot* S = SheetLayer ? Cast<UCanvasPanelSlot>(SheetLayer->Slot) : nullptr;
	if (!B || !B->Hull || !PC || !S)
	{
		return;
	}
	// The sheet's middle and its left and right edges on screen: where the frame goes, how large and how turned it is.
	const FTransform Face = B->GetChartFaceLocal() * B->Hull->GetComponentTransform();
	const FVector Half = Face.GetUnitAxis(EAxis::Y) * (SaltChart::SheetWidthM * 50.f);
	FVector2D Mid, Left, Right;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Face.GetLocation(), Mid, true)
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Face.GetLocation() - Half, Left, true)
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Face.GetLocation() + Half, Right, true))
	{
		const FVector2D Across = Right - Left;
		const float Scale = (float)Across.Size() / ChartSheetSize.X;
		S->SetPosition(Mid);
		SheetLayer->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Scale, Scale), FVector2D::ZeroVector,
			FMath::RadiansToDegrees(FMath::Atan2((float)Across.Y, (float)Across.X))));
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
	PlaceOnSheet();
	// The marks ink in once the eyes have arrived at the sheet, with the prompt.
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const bool bThere = !Player || Player->GetViewFocusAlpha() > 0.97f;
	Ink = bThere ? FMath::Min(Ink + InDeltaTime / ChartInkTime, 1.f) : 0.f;
	if (SheetLayer)
	{
		SheetLayer->SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, Ink));
	}
	if (PutBack && bThere && !bPromptShown)
	{
		bPromptShown = true;
		PutBack->SetVisibility(ESlateVisibility::HitTestInvisible);
		PutBack->PlayIn();
	}
}

FReply USaltChartWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (SaltMenu::IsActionKey(Key, TEXT("Interact")) || Key == EKeys::Escape || Key == EKeys::F10 || Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Gamepad_FaceButton_Left)
	{
		// Not the E that opened it, still repeating while held.
		if (!InKeyEvent.IsRepeat())
		{
			if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
			{
				PC->CloseChart();
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

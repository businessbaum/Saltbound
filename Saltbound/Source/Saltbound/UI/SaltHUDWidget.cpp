// Saltbound - the in-game HUD (D55): focus dot, key-scrap prompt and hold, save sign, station hint, notices.

#include "UI/SaltHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SaltPlaces.h"
#include "Core/SaltSaveSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/SaltPlayerCharacter.h"
#include "Styling/CoreStyle.h"
#include "UI/SaltLandfallWidget.h"
#include "UI/SaltPaintedText.h"
#include "UI/SaltUIStyle.h"

namespace
{
	const FLinearColor PromptColor(0.96f, 0.92f, 0.84f, 1.f); // warm off-white (dot, hint, notices)

	// Layout at 1080p (UI_Design.md section 7): the prompt row is centred 66 px under the screen centre; the 58 px key
	// scrap (in a 120 px frame), 18 px, then the action in IM Fell English 36 px with its baseline 12 px under the key's
	// centre. IM Fell English: ascent 0.905 em, line 1.268 em, so the text's centre sits 12 - 0.271 * 36 px under it.
	constexpr float RowY = 66.f;
	constexpr float KeySize = 58.f;
	constexpr float KeyFrame = 120.f;
	constexpr float KeyGap = 18.f;
	constexpr float PromptSize = 36.f;
	constexpr float TextCentreY = 12.f - 0.271f * PromptSize;
	constexpr float FadeIn = 0.15f;
	constexpr float FadeOut = 0.1f;

	// The save sign (section 14): 96 px, centre 88 px from the right and the bottom edge; one 45 degree step every 0.7 s.
	constexpr float SignSize = 96.f;
	constexpr float SignStep = 0.7f;
	constexpr float SignMin = 1.4f;

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<FString> CVarDevPrompt(TEXT("sb.Dev.Prompt"), TEXT(""),
		TEXT("Show this prompt for captures (empty = the real one)."), ECVF_Cheat);
	TAutoConsoleVariable<float> CVarDevHold(TEXT("sb.Dev.Hold"), -1.f,
		TEXT("sb.Dev.Hold <0..1>: show the hold prompt with this much ink for captures (-1 = off)."), ECVF_Cheat);
	TAutoConsoleVariable<int32> CVarDevSaveSign(TEXT("sb.Dev.SaveSign"), 0,
		TEXT("sb.Dev.SaveSign 1: keep the save sign turning for captures (0 = only while saving)."), ECVF_Cheat);
#endif

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

	/** Ease out (cubic). */
	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Pow(1.f - T, 3.f);
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

		// Two prompt rows (they cross-fade when the target changes).
		RowRoots.Reset();
		RowScraps.Reset();
		RowMaterials.Reset();
		RowLabels.Reset();
		for (int32 I = 0; I < 2; ++I)
		{
			UCanvasPanel* RowRoot = WidgetTree->ConstructWidget<UCanvasPanel>();
			if (UCanvasPanelSlot* S = Root->AddChildToCanvas(RowRoot))
			{
				S->SetAnchors(FAnchors(0.5f, 0.5f));
				S->SetAlignment(FVector2D(0.f, 0.f));
				S->SetPosition(FVector2D(0.f, RowY));
				S->SetSize(FVector2D(1.f, 1.f));
			}
			RowRoot->SetRenderOpacity(0.f);
			RowRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
			UImage* Scrap = WidgetTree->ConstructWidget<UImage>();
			UMaterialInstanceDynamic* Mid = nullptr;
			if (UMaterialInterface* Base = SaltUI::Material(TEXT("M_UI_KeySoak")))
			{
				Mid = UMaterialInstanceDynamic::Create(Base, this);
				Scrap->SetBrush(SaltUI::ImageBrush(Mid, FVector2D(KeyFrame, KeyFrame)));
			}
			if (UCanvasPanelSlot* S = RowRoot->AddChildToCanvas(Scrap))
			{
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetSize(FVector2D(KeyFrame, KeyFrame));
			}
			Scrap->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			USaltPaintedText* Label = WidgetTree->ConstructWidget<USaltPaintedText>(USaltPaintedText::StaticClass());
			Label->Setup(ESaltFace::Fell, PromptSize);
			if (UCanvasPanelSlot* S = RowRoot->AddChildToCanvas(Label))
			{
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetAutoSize(true);
			}
			RowRoots.Add(RowRoot);
			RowScraps.Add(Scrap);
			RowMaterials.Add(Mid);
			RowLabels.Add(Label);
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

		// The save sign: the knotted ring stays still, the rose turns over it (same 256 px frame, pivot in the middle).
		auto SignLayer = [&](const TCHAR* Texture)
		{
			UImage* Img = WidgetTree->ConstructWidget<UImage>();
			Img->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(Texture), FVector2D(SignSize, SignSize)));
			if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Img))
			{
				S->SetAnchors(FAnchors(1.f, 1.f));
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetSize(FVector2D(SignSize, SignSize));
				S->SetPosition(FVector2D(-88.f, -88.f));
			}
			Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Img->SetRenderOpacity(0.f);
			Img->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Img;
		};
		SignRing = SignLayer(TEXT("T_UI_SaveKnot_Ring"));
		SignRose = SignLayer(TEXT("T_UI_SaveKnot_Rose"));

		// The landfall banner over the whole screen (it places itself at 1080p design coordinates).
		Landfall = CreateWidget<USaltLandfallWidget>(this, USaltLandfallWidget::StaticClass());
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Landfall))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}
		Landfall->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void USaltHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		SaveStartedHandle = Saves->OnSaveStarted.AddUObject(this, &USaltHUDWidget::HandleSaveStarted);
		SaveFinishedHandle = Saves->OnSaveFinished.AddUObject(this, &USaltHUDWidget::HandleSaveFinished);
	}
	if (USaltLandfallSubsystem* Landfalls = GetWorld() ? GetWorld()->GetSubsystem<USaltLandfallSubsystem>() : nullptr)
	{
		LandfallHandle = Landfalls->OnLandfall.AddUObject(this, &USaltHUDWidget::HandleLandfall);
	}
}

void USaltHUDWidget::HandleLandfall(FName PlaceId)
{
	if (Landfall)
	{
		Landfall->Show(PlaceId);
	}
}

void USaltHUDWidget::NativeDestruct()
{
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		Saves->OnSaveStarted.Remove(SaveStartedHandle);
		Saves->OnSaveFinished.Remove(SaveFinishedHandle);
	}
	if (USaltLandfallSubsystem* Landfalls = GetWorld() ? GetWorld()->GetSubsystem<USaltLandfallSubsystem>() : nullptr)
	{
		Landfalls->OnLandfall.Remove(LandfallHandle);
	}
	Super::NativeDestruct();
}

void USaltHUDWidget::ShowNotice(const FText& Text)
{
	if (Notice)
	{
		Notice->SetText(Text);
		NoticeTime = 2.5f;
	}
}

FText USaltHUDWidget::GetShownPrompt() const
{
	const FPromptRow& Row = Rows[ActiveRow];
	return Row.bShowing ? Row.Text : FText::GetEmpty();
}

void USaltHUDWidget::LayoutRow(int32 Index)
{
	if (!RowLabels.IsValidIndex(Index))
	{
		return;
	}
	USaltPaintedText* Label = RowLabels[Index];
	Label->SetText(Rows[Index].Text);
	const float TextW = Label->MeasureWidth();
	const float X0 = -(KeySize + KeyGap + TextW) * 0.5f;
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RowScraps[Index]->Slot))
	{
		S->SetPosition(FVector2D(X0 + KeySize * 0.5f, 0.f));
	}
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Label->Slot))
	{
		S->SetPosition(FVector2D(X0 + KeySize + KeyGap + TextW * 0.5f, TextCentreY));
	}
}

void USaltHUDWidget::TickPrompt(float DeltaSeconds)
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	FText Want = Player ? Player->GetPromptText() : FText::GetEmpty();
	float Hold = 0.f;
	float Pulse = 1.f;
	if (Player)
	{
		const USaltInteractableComponent* Target = Player->GetHoldTarget();
		if (Target && Target == Player->GetFocusedInteractable())
		{
			Hold = Player->GetHoldProgress();
		}
		// A hold just fired: the scrap soaks through with a short pulse, then the prompt hides for a moment.
		const float Since = Player->GetTimeSinceHoldFired();
		if (Since < 0.15f)
		{
			Hold = 1.f;
			Pulse = FMath::Lerp(1.06f, 1.f, Since / 0.15f);
			Want = Rows[ActiveRow].Text;
		}
		else if (Since < 0.6f)
		{
			Want = FText::GetEmpty();
		}
	}
#if !UE_BUILD_SHIPPING
	if (CVarDevHold.GetValueOnGameThread() >= 0.f)
	{
		Want = NSLOCTEXT("Saltbound", "BoatRaiseAnchor", "Hold to raise the anchor");
		Hold = FMath::Clamp(CVarDevHold.GetValueOnGameThread(), 0.f, 1.f);
	}
	else if (!CVarDevPrompt.GetValueOnGameThread().IsEmpty())
	{
		Want = FText::FromString(CVarDevPrompt.GetValueOnGameThread());
	}
#endif

	// A new target: the old row fades out while the new one fades in.
	FPromptRow& Active = Rows[ActiveRow];
	if (!Want.EqualTo(Active.Text) || (!Want.IsEmpty() && !Active.bShowing))
	{
		Active.bShowing = false;
		if (!Want.IsEmpty())
		{
			ActiveRow = 1 - ActiveRow;
			FPromptRow& Next = Rows[ActiveRow];
			Next.Text = Want;
			Next.bShowing = true;
			Next.Age = 0.f;
			Next.Alpha = 0.f;
			LayoutRow(ActiveRow);
		}
		else
		{
			Active.Text = FText::GetEmpty();
		}
	}

	for (int32 I = 0; I < 2; ++I)
	{
		FPromptRow& Row = Rows[I];
		Row.Age += DeltaSeconds;
		Row.Alpha = Row.bShowing ? FMath::Min(Row.Alpha + DeltaSeconds / FadeIn, 1.f) : FMath::Max(Row.Alpha - DeltaSeconds / FadeOut, 0.f);
		if (!RowRoots.IsValidIndex(I))
		{
			continue;
		}
		// Fade in and rise 6 px in 0.15 s while the scrap's tilt settles from -6 to -3 degrees.
		const float In = EaseOut(Row.Age / FadeIn);
		RowRoots[I]->SetRenderOpacity(Row.Alpha);
		RowRoots[I]->SetRenderTranslation(FVector2D(0.f, Row.bShowing ? 6.f * (1.f - In) : 0.f));
		const bool bActive = I == ActiveRow && Row.bShowing;
		const float Scale = bActive ? Pulse : 1.f;
		RowScraps[I]->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Scale, Scale), FVector2D::ZeroVector,
			Row.bShowing ? FMath::Lerp(-6.f, -3.f, In) : -3.f));
		if (UMaterialInstanceDynamic* Mid = RowMaterials[I])
		{
			Mid->SetScalarParameterValue(TEXT("Progress"), bActive ? Hold : 0.f);
			Mid->SetScalarParameterValue(TEXT("Time"), Clock);
		}
	}
	ShownHold = Rows[ActiveRow].bShowing ? Hold : 0.f;
}

void USaltHUDWidget::HandleSaveStarted()
{
	bSaving = true;
	if (!bSignOn)
	{
		bSignOn = true;
		SignTime = 0.f;
	}
}

void USaltHUDWidget::HandleSaveFinished()
{
	bSaving = false;
}

void USaltHUDWidget::TickSaveSign(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (CVarDevSaveSign.GetValueOnGameThread() != 0 && !bSignOn)
	{
		bSignOn = true;
		SignTime = 0.f;
	}
	const bool bForced = CVarDevSaveSign.GetValueOnGameThread() != 0;
#else
	const bool bForced = false;
#endif
	if (!bSignOn)
	{
		SignAlpha = FMath::Max(SignAlpha - DeltaSeconds / 0.3f, 0.f);
	}
	else
	{
		const float Before = SignTime;
		SignTime += DeltaSeconds;
		SignAlpha = FMath::Min(SignAlpha + DeltaSeconds / 0.2f, 1.f);
		// Stay at least 1.4 s; when the save is done, finish the current step, then fade out.
		const bool bStepEnded = FMath::FloorToInt(SignTime / SignStep) != FMath::FloorToInt(Before / SignStep);
		if (!bSaving && !bForced && SignTime >= SignMin && bStepEnded)
		{
			bSignOn = false;
		}
	}
	// "Seeking north": one 45 degree step every 0.7 s, each settling with a damped spring and a little overshoot.
	const float Steps = SignTime / SignStep;
	const int32 Whole = FMath::FloorToInt(Steps);
	const float U = Steps - Whole;
	const float Settle = 1.f - FMath::Exp(-7.f * U) * FMath::Cos(11.f * U);
	const float Angle = 45.f * (Whole + (bSignOn || SignAlpha > 0.f ? Settle : 0.f));
	if (SignRose && SignRing)
	{
		SignRose->SetRenderTransformAngle(Angle);
		SignRose->SetRenderOpacity(SignAlpha);
		SignRing->SetRenderOpacity(SignAlpha);
	}
}

void USaltHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	if (Notice)
	{
		NoticeTime = FMath::Max(NoticeTime - InDeltaTime, 0.f);
		Notice->SetRenderOpacity(0.85f * FMath::Clamp(NoticeTime / 0.6f, 0.f, 1.f));
	}

	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const FText HintText = Player ? Player->GetHintText() : FText::GetEmpty();
	const bool bAtStation = Player && Player->IsAtStation();
	const bool bFocus = Player && Player->GetFocusedInteractable() != nullptr;
	if (!HintText.IsEmpty())
	{
		Hint->SetText(HintText);
	}
	const float Speed = 8.f;
	HintAlpha = FMath::FInterpTo(HintAlpha, HintText.IsEmpty() ? 0.f : 0.8f, InDeltaTime, Speed * 0.5f);
	DotAlpha = FMath::FInterpTo(DotAlpha, bAtStation ? 0.f : (bFocus ? 0.95f : 0.3f), InDeltaTime, Speed * 1.5f);
	Hint->SetRenderOpacity(HintAlpha);
	Dot->SetRenderOpacity(DotAlpha);
	const float DotScale = bFocus ? 1.4f : 1.f;
	Dot->SetRenderScale(FVector2D(DotScale, DotScale));

	TickPrompt(InDeltaTime);
	TickSaveSign(InDeltaTime);
}

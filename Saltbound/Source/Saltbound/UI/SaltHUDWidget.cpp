// Saltbound - the in-game HUD (D55): focus dot, key-scrap prompt and hold (on the object or low, D60), save sign, station
// hint, notices.

#include "UI/SaltHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Core/SaltGameUserSettings.h"
#include "Core/SaltPlaces.h"
#include "Core/SaltSaveSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/SaltHeldTools.h"
#include "Player/SaltPlayerCharacter.h"
#include "Styling/CoreStyle.h"
#include "UI/SaltKeyPrompt.h"
#include "UI/SaltLandfallWidget.h"
#include "UI/SaltMenuKit.h"
#include "UI/SaltPaintedText.h"
#include "UI/SaltToolWheelWidget.h"
#include "UI/SaltUIStyle.h"

namespace
{
	const FLinearColor PromptColor(0.96f, 0.92f, 0.84f, 1.f); // warm off-white (dot, hint, notices)

	// Layout at 1080p (UI_Design.md section 7): the 58 px key scrap (in a 120 px frame), 18 px, then the action in IM
	// Fell English 36 px with its baseline 12 px under the key's centre. IM Fell English: ascent 0.905 em, line 1.268 em,
	// so the text's centre sits 12 - 0.271 * 36 px under it. Where the row goes (owner, D60): its middle on the object's
	// prompt point, or 180 px above the bottom edge in the middle (where subtitles go, above the controls row).
	constexpr float RowLowUp = 180.f;
	constexpr float RowEdge = 40.f;
	constexpr float RowKeySize = 58.f;
	constexpr float RowKeyFrame = 120.f;
	constexpr float RowKeyGap = 18.f;
	constexpr float PromptSize = 36.f;
	constexpr float TextCentreY = 12.f - 0.271f * PromptSize;
	constexpr float RowFadeIn = 0.15f;
	constexpr float RowFadeOut = 0.1f;

	// D58: the controls row (key scraps and action) and the status line under it, above the bottom edge.
	constexpr float ControlsRowUp = 112.f;
	constexpr float ControlsStatusUp = 50.f;

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
	FAutoConsoleCommand CmdPromptOnObject(TEXT("sb.PromptOnObject"),
		TEXT("sb.PromptOnObject 1|0: the interaction prompt on its object or low on the screen (the pause menu's Prompt position)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
			{
				if (Args.Num() > 0)
				{
					S->bPromptOnObject = FCString::Atoi(*Args[0]) != 0;
					S->SaveSettings();
				}
				UE_LOG(LogTemp, Display, TEXT("SB_PROMPT on the object=%d"), S->bPromptOnObject ? 1 : 0);
			}
		}));
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
		HudRoot = Root;

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
				// Placed every frame (PlaceRow): the row's middle at the slot's position.
				S->SetAnchors(FAnchors(0.f, 0.f));
				S->SetAlignment(FVector2D(0.f, 0.f));
				S->SetSize(FVector2D(1.f, 1.f));
			}
			RowRoot->SetRenderOpacity(0.f);
			RowRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
			UImage* Scrap = WidgetTree->ConstructWidget<UImage>();
			UMaterialInstanceDynamic* Mid = nullptr;
			if (UMaterialInterface* Base = SaltUI::Material(TEXT("M_UI_KeySoak")))
			{
				Mid = UMaterialInstanceDynamic::Create(Base, this);
				Scrap->SetBrush(SaltUI::ImageBrush(Mid, FVector2D(RowKeyFrame, RowKeyFrame)));
			}
			if (UCanvasPanelSlot* S = RowRoot->AddChildToCanvas(Scrap))
			{
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetSize(FVector2D(RowKeyFrame, RowKeyFrame));
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

		// D58: the status line under the controls row (the row itself is made for its keys, TickControls): plain lettering
		// with the soft halo, as the tool wheel's smaller lines.
		StatusLine = WidgetTree->ConstructWidget<USaltPaintedText>(USaltPaintedText::StaticClass());
		StatusLine->Setup(ESaltFace::AlegreyaItalic, 26.f, ETextJustify::Center);
		StatusLine->SetPlainColor(SaltUI::SRGB(0xE3, 0xD3, 0xB2));
		StatusLine->SetRenderOpacity(0.f);
		StatusLine->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(StatusLine))
		{
			S->SetAnchors(FAnchors(0.5f, 1.f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetAutoSize(true);
			S->SetPosition(FVector2D(0.f, -ControlsStatusUp));
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

		// Looking through the spyglass: dark round the view, a brass rim (section 4). Under the banners and the wheel.
		SpyglassMask = WidgetTree->ConstructWidget<UImage>();
		SpyglassMask->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_SpyglassMask")), FVector2D(960.f, 540.f)));
		SpyglassMask->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(SpyglassMask))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}

		// The landfall banner over the whole screen (it places itself at 1080p design coordinates).
		Landfall = CreateWidget<USaltLandfallWidget>(this, USaltLandfallWidget::StaticClass());
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Landfall))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}
		Landfall->SetVisibility(ESlateVisibility::HitTestInvisible);

		// The tool wheel over everything else of the HUD.
		ToolWheel = CreateWidget<USaltToolWheelWidget>(this, USaltToolWheelWidget::StaticClass());
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(ToolWheel))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}
		ToolWheel->SetVisibility(ESlateVisibility::HitTestInvisible);
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

FText USaltHUDWidget::GetNoticeText() const
{
	return Notice && NoticeTime > 0.f ? Notice->GetText() : FText::GetEmpty();
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
	Rows[Index].Width = RowKeySize + RowKeyGap + TextW;
	const float X0 = -Rows[Index].Width * 0.5f;
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RowScraps[Index]->Slot))
	{
		S->SetPosition(FVector2D(X0 + RowKeySize * 0.5f, 0.f));
	}
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Label->Slot))
	{
		S->SetPosition(FVector2D(X0 + RowKeySize + RowKeyGap + TextW * 0.5f, TextCentreY));
	}
}

void USaltHUDWidget::TickPrompt(float DeltaSeconds)
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	FText Want = Player ? Player->GetPromptText() : FText::GetEmpty();
	// The object the prompt belongs to (none at a station or on a ladder: leaving them is the player's own act).
	const USaltInteractableComponent* WantTarget = Player ? Player->GetFocusedInteractable() : nullptr;
	const USaltGameUserSettings* Settings = USaltGameUserSettings::Get();
	const bool bOnObject = !Settings || Settings->bPromptOnObject;
	float Hold = 0.f;
	float Pulse = 1.f;
	if (Player)
	{
		const USaltInteractableComponent* Held = Player->GetHoldTarget();
		if (Held && Held == WantTarget)
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
			WantTarget = Rows[ActiveRow].Target.Get();
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

	// A new target: the old row fades out while the new one fades in (on the objects, also when the words are the same).
	FPromptRow& Active = Rows[ActiveRow];
	const bool bNewObject = bOnObject && !Want.IsEmpty() && Active.Target.Get() != WantTarget;
	if (!Want.EqualTo(Active.Text) || (!Want.IsEmpty() && !Active.bShowing) || bNewObject)
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
			Next.Target = WantTarget;
			Next.bPinned = false;
			Next.At = FVector2D(ViewSize.X * 0.5f, ViewSize.Y - RowLowUp);
			LayoutRow(ActiveRow);
		}
		else
		{
			Active.Text = FText::GetEmpty();
		}
	}

	// D66: the scrap shows the player's Use key (Keys and buttons).
	PromptKeyAge -= DeltaSeconds;
	if (PromptKeyAge <= 0.f)
	{
		PromptKeyAge = 0.25f;
		const FString Key = SaltMenu::ShownKey(TEXT("E"));
		if (Key != PromptKey)
		{
			PromptKey = Key;
			for (UMaterialInstanceDynamic* Mid : RowMaterials)
			{
				SaltMenu::SetSoakKey(Mid, Key);
			}
		}
	}

	for (int32 I = 0; I < 2; ++I)
	{
		FPromptRow& Row = Rows[I];
		Row.Age += DeltaSeconds;
		Row.Alpha = Row.bShowing ? FMath::Min(Row.Alpha + DeltaSeconds / RowFadeIn, 1.f) : FMath::Max(Row.Alpha - DeltaSeconds / RowFadeOut, 0.f);
		if (!RowRoots.IsValidIndex(I))
		{
			continue;
		}
		PlaceRow(I, bOnObject);
		// Fade in and rise 6 px in 0.15 s while the scrap's tilt settles from -6 to -3 degrees.
		const float In = EaseOut(Row.Age / RowFadeIn);
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

void USaltHUDWidget::PlaceRow(int32 Index, bool bOnObject)
{
	FPromptRow& Row = Rows[Index];
	const USaltInteractableComponent* Target = Row.Target.Get();
	FVector2D P;
	if (bOnObject && Target && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Target->GetPromptLocation(), P, true))
	{
		// The row's middle on the object's prompt point, kept whole on the screen.
		const float HalfW = Row.Width * 0.5f + RowEdge;
		const float HalfH = RowKeyFrame * 0.5f;
		Row.At = FVector2D(FMath::Clamp(P.X, HalfW, FMath::Max(ViewSize.X - HalfW, HalfW)),
			FMath::Clamp(P.Y, HalfH, FMath::Max(ViewSize.Y - HalfH, HalfH)));
		Row.bPinned = true;
	}
	else if (!bOnObject || !Row.bPinned)
	{
		Row.At = FVector2D(ViewSize.X * 0.5f, ViewSize.Y - RowLowUp);
		Row.bPinned = false;
	}
	// (A pinned row whose object is gone, a pickup just taken, fades out where it was.)
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RowRoots[Index]->Slot))
	{
		S->SetPosition(Row.At);
	}
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

float USaltHUDWidget::GetSpyglassMaskOpacity() const
{
	return SpyglassMask && SpyglassMask->GetVisibility() != ESlateVisibility::Collapsed ? SpyglassMask->GetRenderOpacity() : 0.f;
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
	if (SpyglassMask)
	{
		const float Spy = Player && Player->GetHeldTools() ? Player->GetHeldTools()->GetSpyglass() : 0.f;
		SpyglassMask->SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, Spy));
		SpyglassMask->SetVisibility(Spy > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// No dot at a station, or while the eyes are on something close (the chart, D58).
	const bool bNoDot = Player && (Player->IsAtStation() || Player->GetViewFocusAlpha() > 0.f);
	const bool bFocus = Player && Player->GetFocusedInteractable() != nullptr;
	const float Speed = 8.f;
	DotAlpha = FMath::FInterpTo(DotAlpha, bNoDot ? 0.f : (bFocus ? 0.95f : 0.3f), InDeltaTime, Speed * 1.5f);
	Dot->SetRenderOpacity(DotAlpha);
	const float DotScale = bFocus ? 1.4f : 1.f;
	Dot->SetRenderScale(FVector2D(DotScale, DotScale));

	const FVector2D Size = MyGeometry.GetLocalSize();
	if (Size.X > 1.f && Size.Y > 1.f)
	{
		ViewSize = Size;
	}
	TickPrompt(InDeltaTime);
	TickControls(InDeltaTime);
	TickSaveSign(InDeltaTime);
}

void USaltHUDWidget::TickControls(float DeltaSeconds)
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	TArray<FSaltControlHint> Controls;
	FText Status;
	const bool bAny = Player && Player->GetControlsHint(Controls, Status);
	// A new set of keys makes new rows (each key is a scrap with its letter), side by side ("W S Sail   A D Steer").
	FString Keys;
	for (const FSaltControlHint& C : Controls)
	{
		Keys += FString::Join(C.Keys, TEXT("")) + TEXT("|");
	}
	const bool bRows = bAny && Controls.Num() > 0;
	if (bRows && (ControlsRows.Num() != Controls.Num() || Keys != ControlKeys) && HudRoot)
	{
		for (USaltKeyPrompt* Row : ControlsRows)
		{
			Row->RemoveFromParent();
		}
		ControlsRows.Reset();
		ControlKeys = Keys;
		for (const FSaltControlHint& C : Controls)
		{
			USaltKeyPrompt* Row = CreateWidget<USaltKeyPrompt>(this, USaltKeyPrompt::StaticClass());
			Row->Setup(C.Keys, C.Action);
			if (UCanvasPanelSlot* S = HudRoot->AddChildToCanvas(Row))
			{
				S->SetAnchors(FAnchors(0.5f, 1.f));
				S->SetAlignment(FVector2D::ZeroVector);
				S->SetSize(FVector2D(1.f, 1.f));
			}
			ControlsRows.Add(Row);
		}
		bControlsShowing = false;
	}
	if (bRows)
	{
		// Centred as a whole, 56 px between the groups.
		constexpr float Gap = 56.f;
		float Total = -Gap;
		for (int32 I = 0; I < ControlsRows.Num(); ++I)
		{
			ControlsRows[I]->SetAction(Controls[I].Action);
			Total += ControlsRows[I]->GetRowWidth() + Gap;
		}
		float X = -Total * 0.5f;
		for (USaltKeyPrompt* Row : ControlsRows)
		{
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Row->Slot))
			{
				S->SetPosition(FVector2D(X + Row->GetRowWidth() * 0.5f, -ControlsRowUp));
			}
			X += Row->GetRowWidth() + Gap;
			if (!bControlsShowing)
			{
				Row->PlayIn();
			}
		}
	}
	bControlsShowing = bRows;
	if (bAny && !Status.EqualTo(StatusLine->GetText()))
	{
		StatusLine->SetText(Status);
	}
	ControlsAlpha = FMath::FInterpTo(ControlsAlpha, bAny ? 1.f : 0.f, DeltaSeconds, 10.f);
	for (USaltKeyPrompt* Row : ControlsRows)
	{
		Row->SetRenderOpacity(bRows ? 1.f : ControlsAlpha);
		Row->SetVisibility(ControlsAlpha > 0.01f || bRows ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	StatusLine->SetRenderOpacity(0.9f * ControlsAlpha * (Status.IsEmpty() && bAny ? 0.f : 1.f));
}

FText USaltHUDWidget::GetShownControls() const
{
	TArray<FString> Actions;
	for (const USaltKeyPrompt* Row : ControlsRows)
	{
		Actions.Add(Row->GetAction().ToString());
	}
	return bControlsShowing ? FText::FromString(FString::Join(Actions, TEXT(", "))) : FText::GetEmpty();
}

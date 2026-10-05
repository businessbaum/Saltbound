// Saltbound - the settings (D66).

#include "UI/SaltSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SaltGameUserSettings.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltPaintedText.h"

namespace
{
	// Menus.html "Settings" at 1080p: the title at (6cqw, 5.5cqw), the pages from (6cqw, 12cqw), the rows 45cqw wide from
	// (25cqw, 11cqw) with a 20cqw value column, the description 21cqw wide 4.5cqw from the right edge.
	constexpr float SetLeft = 115.f;
	constexpr float SetTabsTop = 230.f;
	constexpr float SetTabPitch = 55.f;
	constexpr float SetRowsLeft = 480.f;
	constexpr float SetRowsTop = 211.f;
	constexpr float SetRowsWidth = 864.f;
	constexpr float SetRowHeight = 63.f;
	constexpr float SetRowPitch = 66.f;
	constexpr float SetRowPadX = 23.f;
	constexpr float SetValueWidth = 384.f;
	constexpr float SetValueLeft = SetRowsLeft + SetRowsWidth - SetRowPadX - SetValueWidth;
	constexpr float SetLabelSize = 27.f;
	constexpr float SetValueSize = 26.f;
	constexpr float SetArrowSize = 33.f;
	constexpr float SetRopeHeight = 17.f;
	constexpr float SetTrackWidth = 300.f;
	constexpr float SetKnotWidth = 84.f;
	constexpr float SetDetailLeft = 1431.f;
	constexpr float SetDetailTop = 221.f;
	constexpr float SetDetailWidth = 403.f;

	struct FSettingText
	{
		FText Label;
		FText Detail;
	};

	const FSettingText& TextOf(ESaltSetting S)
	{
		static const TMap<ESaltSetting, FSettingText> Map = {
			{ ESaltSetting::Prompts, { NSLOCTEXT("SaltSettings", "Prompts", "Prompts"),
				NSLOCTEXT("SaltSettings", "PromptsD", "Where the key and what it does show: on the thing you look at, or low on the screen where subtitles go.") } },
			{ ESaltSetting::Defaults, { NSLOCTEXT("SaltSettings", "Defaults", "Every setting"),
				NSLOCTEXT("SaltSettings", "DefaultsD", "Puts the display, the graphics, the sound, the mouse and the keys back to how the game came.") } },
			{ ESaltSetting::Window, { NSLOCTEXT("SaltSettings", "Window", "Window"),
				NSLOCTEXT("SaltSettings", "WindowD", "Fullscreen, a borderless window over the whole screen, or a window.") } },
			{ ESaltSetting::FrameCap, { NSLOCTEXT("SaltSettings", "FrameCap", "Frame cap"),
				NSLOCTEXT("SaltSettings", "FrameCapD", "The most frames per second the game draws. Set it to your screen's refresh rate for the smoothest sailing.") } },
			{ ESaltSetting::VSync, { NSLOCTEXT("SaltSettings", "VSync", "Vsync"),
				NSLOCTEXT("SaltSettings", "VSyncD", "Waits for the screen before showing a frame. Off is smoother with a frame cap; on avoids tearing.") } },
			{ ESaltSetting::ResolutionScale, { NSLOCTEXT("SaltSettings", "ResolutionScale", "Resolution scale"),
				NSLOCTEXT("SaltSettings", "ResolutionScaleD", "Draws fewer pixels and sharpens them back. Lower runs faster; the interface keeps its size.") } },
			{ ESaltSetting::Brightness, { NSLOCTEXT("SaltSettings", "Brightness", "Brightness"),
				NSLOCTEXT("SaltSettings", "BrightnessD", "Opens the knot test from the first start.") } },
			{ ESaltSetting::Quality, { NSLOCTEXT("SaltSettings", "Quality", "Quality"),
				NSLOCTEXT("SaltSettings", "QualityD", "Four sea states. Calm runs on older computers, Gale turns everything on. Fresh is right for most machines.") } },
			{ ESaltSetting::Shadows, { NSLOCTEXT("SaltSettings", "Shadows", "Shadows"),
				NSLOCTEXT("SaltSettings", "ShadowsD", "How fine the shadows are and how far they reach.") } },
			{ ESaltSetting::ViewDistance, { NSLOCTEXT("SaltSettings", "ViewDistance", "View distance"),
				NSLOCTEXT("SaltSettings", "ViewDistanceD", "How far away small things are still drawn.") } },
			{ ESaltSetting::FieldOfView, { NSLOCTEXT("SaltSettings", "FieldOfView", "Field of view"),
				NSLOCTEXT("SaltSettings", "FieldOfViewD", "How wide you see. Wider feels faster on the boat; narrower is calmer.") } },
			{ ESaltSetting::Volume, { NSLOCTEXT("SaltSettings", "Volume", "Volume"),
				NSLOCTEXT("SaltSettings", "VolumeD", "Everything the game plays: the sea, the wind, the boat and the rest.") } },
			{ ESaltSetting::MouseSensitivity, { NSLOCTEXT("SaltSettings", "Mouse", "Mouse sensitivity"),
				NSLOCTEXT("SaltSettings", "MouseD", "How fast the view turns with the mouse.") } },
			{ ESaltSetting::InvertLook, { NSLOCTEXT("SaltSettings", "Invert", "Invert look"),
				NSLOCTEXT("SaltSettings", "InvertD", "Moving the mouse or the stick up looks down.") } },
			{ ESaltSetting::Keys, { NSLOCTEXT("SaltSettings", "Keys", "Keys and buttons"),
				NSLOCTEXT("SaltSettings", "KeysD", "Change the keys you sail with.") } },
		};
		return Map.FindChecked(S);
	}

	struct FSettingPage
	{
		FText Name;
		TArray<ESaltSetting> Rows;
	};

	const TArray<FSettingPage>& Pages()
	{
		static const TArray<FSettingPage> P = {
			{ NSLOCTEXT("SaltSettings", "Game", "Game"), { ESaltSetting::Prompts, ESaltSetting::Defaults } },
			{ NSLOCTEXT("SaltSettings", "Display", "Display"), { ESaltSetting::Window, ESaltSetting::FrameCap, ESaltSetting::VSync, ESaltSetting::ResolutionScale, ESaltSetting::Brightness } },
			{ NSLOCTEXT("SaltSettings", "Graphics", "Graphics"), { ESaltSetting::Quality, ESaltSetting::Shadows, ESaltSetting::ViewDistance, ESaltSetting::FieldOfView } },
			{ NSLOCTEXT("SaltSettings", "Audio", "Audio"), { ESaltSetting::Volume } },
			{ NSLOCTEXT("SaltSettings", "Controls", "Controls"), { ESaltSetting::MouseSensitivity, ESaltSetting::InvertLook, ESaltSetting::Keys } },
		};
		return P;
	}

	/** The frame caps offered: the display's refresh rate among them; 0 = unlimited (last). */
	TArray<float> FrameCaps()
	{
		TArray<float> Caps = { 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f };
		Caps.AddUnique(USaltGameUserSettings::DefaultFrameRateLimit());
		Caps.Sort();
		Caps.Add(0.f);
		return Caps;
	}

	int32 NearestCap(const TArray<float>& Caps, float Value)
	{
		if (Value <= 0.f)
		{
			return Caps.Num() - 1;
		}
		int32 Best = 0;
		for (int32 I = 1; I < Caps.Num() - 1; ++I)
		{
			if (FMath::Abs(Caps[I] - Value) < FMath::Abs(Caps[Best] - Value))
			{
				Best = I;
			}
		}
		return Best;
	}

	const EWindowMode::Type WindowModes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	float ResolutionScale(const USaltGameUserSettings* S)
	{
		// 0 = never set (the engine's default): native resolution (r.ScreenPercentage 100, DefaultEngine.ini).
		float Normalized, Value, Min, Max;
		S->GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
		return Value > 1.f ? Value : 100.f;
	}

	/** A number setting's range and step. */
	struct FRange
	{
		float Min, Max, Step;
	};
	FRange RangeOf(ESaltSetting S)
	{
		switch (S)
		{
		case ESaltSetting::ResolutionScale: return { 50.f, 100.f, 5.f };
		case ESaltSetting::FieldOfView: return { 70.f, 110.f, 5.f };
		case ESaltSetting::Volume: return { 0.f, 1.f, 0.05f };
		case ESaltSetting::MouseSensitivity: return { 0.25f, 3.f, 0.25f };
		default: return { 0.f, 1.f, 0.1f };
		}
	}

	const FText& QualityName(int32 Level)
	{
		// The quality presets are named after the sea (owner-approved menu page).
		static const FText Names[] = { NSLOCTEXT("SaltSettings", "Calm", "Calm"), NSLOCTEXT("SaltSettings", "Fair", "Fair"),
			NSLOCTEXT("SaltSettings", "Fresh", "Fresh"), NSLOCTEXT("SaltSettings", "Gale", "Gale") };
		static const FText Custom = NSLOCTEXT("SaltSettings", "Custom", "Custom");
		return Level >= 0 && Level < 4 ? Names[Level] : Custom;
	}

	const FText& LevelName(int32 Level)
	{
		static const FText Names[] = { NSLOCTEXT("SaltSettings", "Low", "Low"), NSLOCTEXT("SaltSettings", "Medium", "Medium"),
			NSLOCTEXT("SaltSettings", "High", "High"), NSLOCTEXT("SaltSettings", "Epic", "Epic") };
		return Names[FMath::Clamp(Level, 0, 3)];
	}

	const FText& DistanceName(int32 Level)
	{
		static const FText Names[] = { NSLOCTEXT("SaltSettings", "Near", "Near"), NSLOCTEXT("SaltSettings", "Mid", "Middling"),
			NSLOCTEXT("SaltSettings", "Far", "Far"), NSLOCTEXT("SaltSettings", "Farthest", "Farthest") };
		return Names[FMath::Clamp(Level, 0, 3)];
	}
}

USaltSettingsWidget::USaltSettingsWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	BackdropKind = EBackdrop::Blurred;
	bShadeLeft = false;
}

USaltSettingsWidget::EKind USaltSettingsWidget::KindOf(ESaltSetting S)
{
	switch (S)
	{
	case ESaltSetting::Prompts:
	case ESaltSetting::Window:
	case ESaltSetting::FrameCap:
	case ESaltSetting::Quality:
	case ESaltSetting::Shadows:
	case ESaltSetting::ViewDistance:
		return EKind::Choice;
	case ESaltSetting::VSync:
	case ESaltSetting::InvertLook:
		return EKind::Switch;
	case ESaltSetting::ResolutionScale:
	case ESaltSetting::FieldOfView:
	case ESaltSetting::Volume:
	case ESaltSetting::MouseSensitivity:
		return EKind::Number;
	default:
		return EKind::Link;
	}
}

const TArray<ESaltSetting>& USaltSettingsWidget::Rows() const
{
	return Pages()[FMath::Clamp(CurrentPage, 0, Pages().Num() - 1)].Rows;
}

int32 USaltSettingsWidget::GetPageCount() const
{
	return Pages().Num();
}

void USaltSettingsWidget::Build()
{
	SaltUI::At(Contents, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltSettings", "Title", "Settings"), ESaltFace::Fell, 50.f),
		FVector2D(SetLeft, 106.f + 25.f), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));

	Tabs.Reset();
	for (int32 I = 0; I < Pages().Num(); ++I)
	{
		USaltMenuItem* Tab = WidgetTree->ConstructWidget<USaltMenuItem>(USaltMenuItem::StaticClass());
		Tab->Setup(Pages()[I].Name, 34.f, 38.f, 54.f);
		Tab->bShowRope = false;
		Tab->UnchosenOpacity = 0.62f;
		SaltUI::At(Contents, Tab, FVector2D(SetLeft, SetTabsTop + I * SetTabPitch), FVector2D(10.f, 10.f));
		Tab->SetChosen(I == CurrentPage, true);
		Tabs.Add(Tab);
	}

	RowsCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	UCanvasPanelSlot* RS = Contents->AddChildToCanvas(RowsCanvas);
	RS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	RS->SetOffsets(FMargin(0.f));

	DetailTitle = SaltMenu::Paint(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 36.f);
	SaltUI::At(Contents, DetailTitle, FVector2D(SetDetailLeft, SetDetailTop + 18.f), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
	DetailText = SaltUI::FaceText(WidgetTree, FText::GetEmpty(), ESaltFace::Alegreya, 22.f, SaltMenu::DetailGrey());
	DetailText->SetAutoWrapText(true);
	DetailText->SetShadowOffset(FVector2D(0.f, 1.f));
	DetailText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	SaltUI::At(Contents, DetailText, FVector2D(SetDetailLeft, SetDetailTop + 36.f + 19.f), FVector2D(SetDetailWidth, 300.f));

	PlaceHints({ { { TEXT("Q"), TEXT("E") }, NSLOCTEXT("SaltSettings", "HintPage", "page") },
		{ { TEXT("W"), TEXT("S") }, NSLOCTEXT("SaltSettings", "HintChoose", "choose") },
		{ { TEXT("A"), TEXT("D") }, NSLOCTEXT("SaltSettings", "HintChange", "change") },
		{ { TEXT("Esc") }, NSLOCTEXT("SaltSettings", "HintBack", "back") } }, SetLeft, false);
	Rebuild();
}

void USaltSettingsWidget::Rebuild()
{
	if (!RowsCanvas)
	{
		return;
	}
	RowsCanvas->ClearChildren();
	const TArray<ESaltSetting>& List = Rows();
	Row = FMath::Clamp(Row, 0, List.Num() - 1);
	auto Place = [this](UWidget* W, FVector2D Design, FVector2D Size, FVector2D Align = FVector2D::ZeroVector)
	{
		return SaltUI::At(RowsCanvas, W, Design, Size, Align);
	};
	for (int32 I = 0; I < List.Num(); ++I)
	{
		const ESaltSetting S = List[I];
		const float Top = SetRowsTop + I * SetRowPitch;
		const float Mid = Top + SetRowHeight * 0.5f;
		if (I == Row)
		{
			// The chosen row: a soft cream wash from the left.
			UImage* Wash = WidgetTree->ConstructWidget<UImage>();
			Wash->SetBrush(SaltUI::ImageBrush(SaltMenu::Gradient(TEXT("RowWash"), true, { { 0.f, 242, 226, 191, 0.13f }, { 1.f, 242, 226, 191, 0.f } }),
				FVector2D(SetRowsWidth, SetRowHeight)));
			Place(Wash, FVector2D(SetRowsLeft, Top), FVector2D(SetRowsWidth, SetRowHeight));
		}
		Place(SaltMenu::Paint(WidgetTree, TextOf(S).Label, ESaltFace::Fell, SetLabelSize), FVector2D(SetRowsLeft + SetRowPadX, Mid), FVector2D::ZeroVector,
			FVector2D(0.f, 0.5f));
		switch (KindOf(S))
		{
		case EKind::Choice:
		{
			Place(SaltMenu::Anchor(WidgetTree, SetArrowSize, false), FVector2D(SetValueLeft + SetArrowSize * 0.5f, Mid), FVector2D(SetArrowSize, SetArrowSize),
				FVector2D(0.5f, 0.5f));
			Place(SaltMenu::Paint(WidgetTree, ChoiceOf(S), ESaltFace::Fell, SetValueSize, ETextJustify::Center), FVector2D(SetValueLeft + SetValueWidth * 0.5f, Mid),
				FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
			Place(SaltMenu::Anchor(WidgetTree, SetArrowSize, true), FVector2D(SetValueLeft + SetValueWidth - SetArrowSize * 0.5f, Mid),
				FVector2D(SetArrowSize, SetArrowSize), FVector2D(0.5f, 0.5f));
			break;
		}
		case EKind::Switch:
		{
			// On / Off: the active word inked in full with the rope under it, the other faint.
			const bool bOn = SwitchOf(S);
			float X = SetValueLeft;
			for (int32 W = 0; W < 2; ++W)
			{
				const bool bActive = (W == 0) == bOn;
				USaltPaintedText* Word = SaltMenu::Paint(WidgetTree, W == 0 ? NSLOCTEXT("SaltSettings", "On", "On") : NSLOCTEXT("SaltSettings", "Off", "Off"),
					ESaltFace::Fell, SetValueSize);
				Word->SetRenderOpacity(bActive ? 1.f : 0.45f);
				const float Width = Word->MeasureWidth();
				Place(Word, FVector2D(X, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
				if (bActive)
				{
					Place(SaltMenu::Rope(WidgetTree, SetRopeHeight), FVector2D(X - 2.f, Mid + SetValueSize * 0.5f + 2.f), FVector2D(Width + 4.f, SetRopeHeight));
				}
				X += Width + 27.f;
			}
			break;
		}
		case EKind::Number:
		{
			FText Label;
			const float V = NumberOf(S, &Label);
			// The rope at 35 %, the part up to the value in full, the knot tied at the value; the number at the end.
			UImage* Track = SaltMenu::Rope(WidgetTree, SetRopeHeight);
			Track->SetRenderOpacity(0.35f);
			Place(Track, FVector2D(SetValueLeft, Mid - SetRopeHeight * 0.5f), FVector2D(SetTrackWidth, SetRopeHeight));
			Place(SaltMenu::Rope(WidgetTree, SetRopeHeight), FVector2D(SetValueLeft, Mid - SetRopeHeight * 0.5f), FVector2D(SetTrackWidth * V, SetRopeHeight));
			// The knot's rope runs at 0.6 of its height.
			Place(SaltMenu::Knot(WidgetTree, SetKnotWidth), FVector2D(SetValueLeft + SetTrackWidth * V, Mid - SetKnotWidth * 0.5f * 0.6f),
				FVector2D(SetKnotWidth, SetKnotWidth * 0.5f), FVector2D(0.5f, 0.f));
			Place(SaltMenu::Paint(WidgetTree, Label, ESaltFace::Fell, 23.f, ETextJustify::Right), FVector2D(SetValueLeft + SetValueWidth, Mid), FVector2D::ZeroVector,
				FVector2D(1.f, 0.5f));
			break;
		}
		case EKind::Link:
		{
			const FText Link = S == ESaltSetting::Defaults ? NSLOCTEXT("SaltSettings", "Restore", "Restore ›")
				: S == ESaltSetting::Brightness ? NSLOCTEXT("SaltSettings", "Adjust", "Adjust ›") : NSLOCTEXT("SaltSettings", "Change", "Change ›");
			Place(SaltMenu::Paint(WidgetTree, Link, ESaltFace::Fell, SetValueSize), FVector2D(SetValueLeft, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
			break;
		}
		}
	}
	for (int32 I = 0; I < Tabs.Num(); ++I)
	{
		Tabs[I]->SetChosen(I == CurrentPage);
	}
	if (List.IsValidIndex(Row))
	{
		DetailTitle->SetText(TextOf(List[Row]).Label);
		FText Detail = TextOf(List[Row]).Detail;
		if (GIsEditor && (List[Row] == ESaltSetting::Window || List[Row] == ESaltSetting::VSync))
		{
			Detail = FText::Format(NSLOCTEXT("SaltSettings", "GameOnly", "{0} (In the editor this applies to the game only.)"), Detail);
		}
		DetailText->SetText(Detail);
	}
}

void USaltSettingsWidget::SetPage(int32 InPage)
{
	const int32 N = Pages().Num();
	CurrentPage = ((InPage % N) + N) % N;
	Row = 0;
	Rebuild();
}

void USaltSettingsWidget::ChooseRow(int32 InRow)
{
	if (Rows().IsValidIndex(InRow) && InRow != Row)
	{
		Row = InRow;
		Rebuild();
	}
}

void USaltSettingsWidget::ShowSetting(ESaltSetting Setting)
{
	for (int32 P = 0; P < Pages().Num(); ++P)
	{
		const int32 R = Pages()[P].Rows.IndexOfByKey(Setting);
		if (R != INDEX_NONE)
		{
			CurrentPage = P;
			Row = R;
			Rebuild();
			return;
		}
	}
}

FText USaltSettingsWidget::ChoiceOf(ESaltSetting Setting) const
{
	const USaltGameUserSettings* S = USaltGameUserSettings::Get();
	if (!S)
	{
		return FText::FromString(TEXT("-"));
	}
	switch (Setting)
	{
	case ESaltSetting::Prompts:
		return S->bPromptOnObject ? NSLOCTEXT("SaltSettings", "OnObject", "On the object") : NSLOCTEXT("SaltSettings", "Low", "Low on the screen");
	case ESaltSetting::Window:
	{
		const EWindowMode::Type Mode = S->GetFullscreenMode();
		return Mode == EWindowMode::Fullscreen ? NSLOCTEXT("SaltSettings", "Fullscreen", "Fullscreen")
			: Mode == EWindowMode::WindowedFullscreen ? NSLOCTEXT("SaltSettings", "Borderless", "Borderless") : NSLOCTEXT("SaltSettings", "Windowed", "Windowed");
	}
	case ESaltSetting::FrameCap:
		return S->GetFrameRateLimit() <= 0.f ? NSLOCTEXT("SaltSettings", "Unlimited", "Unlimited") : FText::AsNumber(FMath::RoundToInt(S->GetFrameRateLimit()));
	case ESaltSetting::Quality:
		return QualityName(S->GetOverallScalabilityLevel());
	case ESaltSetting::Shadows:
		return LevelName(S->GetShadowQuality());
	case ESaltSetting::ViewDistance:
		return DistanceName(S->GetViewDistanceQuality());
	default:
		return FText::GetEmpty();
	}
}

bool USaltSettingsWidget::SwitchOf(ESaltSetting Setting) const
{
	const USaltGameUserSettings* S = USaltGameUserSettings::Get();
	return S && (Setting == ESaltSetting::VSync ? S->IsVSyncEnabled() : Setting == ESaltSetting::InvertLook ? S->bInvertLookY : false);
}

float USaltSettingsWidget::NumberOf(ESaltSetting Setting, FText* OutLabel) const
{
	const USaltGameUserSettings* S = USaltGameUserSettings::Get();
	if (!S)
	{
		return 0.f;
	}
	float Value = 0.f;
	FString Label;
	switch (Setting)
	{
	case ESaltSetting::ResolutionScale: Value = ResolutionScale(S); Label = FString::Printf(TEXT("%.0f %%"), Value); break;
	case ESaltSetting::FieldOfView: Value = S->FieldOfView; Label = FString::Printf(TEXT("%.0f°"), Value); break;
	case ESaltSetting::Volume: Value = S->MasterVolume; Label = FString::Printf(TEXT("%.0f"), Value * 100.f); break;
	case ESaltSetting::MouseSensitivity: Value = S->MouseSensitivity; Label = FString::Printf(TEXT("%.2f"), Value); break;
	default: break;
	}
	if (OutLabel)
	{
		*OutLabel = FText::FromString(Label);
	}
	const FRange R = RangeOf(Setting);
	return FMath::Clamp((Value - R.Min) / (R.Max - R.Min), 0.f, 1.f);
}

void USaltSettingsWidget::SetNumber(ESaltSetting Setting, float Normalized)
{
	USaltGameUserSettings* S = USaltGameUserSettings::Get();
	if (!S)
	{
		return;
	}
	const FRange R = RangeOf(Setting);
	const float Value = FMath::Clamp(R.Min + FMath::RoundToFloat(FMath::Clamp(Normalized, 0.f, 1.f) * (R.Max - R.Min) / R.Step) * R.Step, R.Min, R.Max);
	switch (Setting)
	{
	case ESaltSetting::ResolutionScale: S->SetResolutionScaleValueEx(Value); break;
	case ESaltSetting::FieldOfView: S->FieldOfView = Value; break;
	case ESaltSetting::Volume: S->MasterVolume = Value; break;
	case ESaltSetting::MouseSensitivity: S->MouseSensitivity = Value; break;
	default: return;
	}
	Apply(Setting);
}

void USaltSettingsWidget::Apply(ESaltSetting Setting)
{
	USaltGameUserSettings* S = USaltGameUserSettings::Get();
	if (!S)
	{
		return;
	}
	switch (Setting)
	{
	case ESaltSetting::Window:
	case ESaltSetting::FrameCap:
	case ESaltSetting::VSync:
	case ESaltSetting::ResolutionScale:
	case ESaltSetting::Quality:
	case ESaltSetting::Shadows:
	case ESaltSetting::ViewDistance:
		S->ApplyDisplay();
		break;
	case ESaltSetting::Volume:
		S->ApplyExtras();
		S->SaveSettings();
		break;
	default:
		S->SaveSettings();
		if (ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
		{
			Player->ApplyUserSettings();
		}
		break;
	}
	Rebuild();
}

void USaltSettingsWidget::Change(ESaltSetting Setting, int32 Delta)
{
	USaltGameUserSettings* S = USaltGameUserSettings::Get();
	ASaltPlayerController* PC = GetSaltController();
	if (!S)
	{
		return;
	}
	auto Wrap = [](int32 V, int32 N) { return ((V % N) + N) % N; };
	switch (KindOf(Setting))
	{
	case EKind::Number:
	{
		const FRange R = RangeOf(Setting);
		SetNumber(Setting, NumberOf(Setting) + Delta * R.Step / (R.Max - R.Min));
		return;
	}
	case EKind::Switch:
		if (Setting == ESaltSetting::VSync)
		{
			S->SetVSyncEnabled(!S->IsVSyncEnabled());
		}
		else
		{
			S->bInvertLookY = !S->bInvertLookY;
		}
		Apply(Setting);
		return;
	case EKind::Link:
		if (!PC)
		{
			return;
		}
		if (Setting == ESaltSetting::Keys)
		{
			PC->OpenKeys();
		}
		else if (Setting == ESaltSetting::Brightness)
		{
			PC->OpenBrightness(true);
		}
		else if (Setting == ESaltSetting::Defaults)
		{
			TWeakObjectPtr<USaltSettingsWidget> Self(this);
			PC->Ask(NSLOCTEXT("SaltSettings", "AskDefaults", "Restore every setting?"),
				NSLOCTEXT("SaltSettings", "AskDefaultsBody", "The display, the graphics, the sound, the mouse and the keys go back to how the game came. Your brightness too."),
				NSLOCTEXT("SaltSettings", "AskDefaultsYes", "restore"), NSLOCTEXT("SaltSettings", "AskDefaultsNo", "keep them"),
				[Self, PC]()
				{
					if (USaltGameUserSettings* Settings = USaltGameUserSettings::Get())
					{
						Settings->SetToDefaults();
						Settings->ApplyDisplay();
						Settings->ApplyExtras();
						if (ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(PC->GetPawn()))
						{
							Player->ApplyUserSettings();
						}
						PC->RebuildInputMappings();
					}
					if (Self.IsValid())
					{
						Self->Rebuild();
					}
				});
		}
		return;
	case EKind::Choice:
		break;
	}
	if (Delta == 0)
	{
		return;
	}
	switch (Setting)
	{
	case ESaltSetting::Prompts:
		S->bPromptOnObject = !S->bPromptOnObject;
		break;
	case ESaltSetting::Window:
	{
		int32 I = 0;
		while (I < 2 && WindowModes[I] != S->GetFullscreenMode())
		{
			++I;
		}
		S->SetFullscreenMode(WindowModes[Wrap(I + Delta, 3)]);
		break;
	}
	case ESaltSetting::FrameCap:
	{
		const TArray<float> Caps = FrameCaps();
		S->SetFrameRateLimit(Caps[Wrap(NearestCap(Caps, S->GetFrameRateLimit()) + Delta, Caps.Num())]);
		break;
	}
	case ESaltSetting::Quality:
	{
		const int32 Level = S->GetOverallScalabilityLevel();
		S->SetOverallScalabilityLevel(Level < 0 ? 2 : Wrap(Level + Delta, 4));
		break;
	}
	case ESaltSetting::Shadows:
		S->SetShadowQuality(Wrap(FMath::Clamp(S->GetShadowQuality(), 0, 3) + Delta, 4));
		break;
	case ESaltSetting::ViewDistance:
		S->SetViewDistanceQuality(Wrap(FMath::Clamp(S->GetViewDistanceQuality(), 0, 3) + Delta, 4));
		break;
	default:
		return;
	}
	Apply(Setting);
}

FString USaltSettingsWidget::Describe(ESaltSetting Setting) const
{
	switch (KindOf(Setting))
	{
	case EKind::Choice: return ChoiceOf(Setting).ToString();
	case EKind::Switch: return SwitchOf(Setting) ? TEXT("On") : TEXT("Off");
	case EKind::Number: { FText L; NumberOf(Setting, &L); return L.ToString(); }
	default: return FString();
	}
}

void USaltSettingsWidget::Navigate(int32 Dx, int32 Dy)
{
	const TArray<ESaltSetting>& List = Rows();
	if (Dy != 0)
	{
		ChooseRow((Row + Dy + List.Num()) % List.Num());
	}
	else if (Dx != 0 && List.IsValidIndex(Row))
	{
		// A link opens with D / right, as with E.
		if (KindOf(List[Row]) != EKind::Link || Dx > 0)
		{
			Change(List[Row], Dx);
		}
	}
}

void USaltSettingsWidget::Accept()
{
	if (Rows().IsValidIndex(Row))
	{
		Change(Rows()[Row], 1);
	}
}

bool USaltSettingsWidget::RawKey(const FKey& Key, bool bRepeat)
{
	// Q / E turn the pages here (the menu page's keys); Enter and Space use a row.
	if (Key == EKeys::Q || Key == EKeys::E)
	{
		if (!bRepeat)
		{
			Page(Key == EKeys::Q ? -1 : 1);
		}
		return true;
	}
	return false;
}

int32 USaltSettingsWidget::RowAt(const FVector2D& Design) const
{
	if (Design.X < SetRowsLeft || Design.X > SetRowsLeft + SetRowsWidth)
	{
		return INDEX_NONE;
	}
	const int32 R = FMath::FloorToInt((Design.Y - SetRowsTop) / SetRowPitch);
	return Design.Y >= SetRowsTop && Rows().IsValidIndex(R) ? R : INDEX_NONE;
}

void USaltSettingsWidget::PointerMove(const FVector2D& Design)
{
	if (DragRow != INDEX_NONE && Rows().IsValidIndex(DragRow))
	{
		SetNumber(Rows()[DragRow], (Design.X - SetValueLeft) / SetTrackWidth);
		return;
	}
	const int32 R = RowAt(Design);
	if (R != INDEX_NONE)
	{
		ChooseRow(R);
	}
}

void USaltSettingsWidget::PointerDown(const FVector2D& Design)
{
	// The pages.
	for (int32 I = 0; I < Tabs.Num(); ++I)
	{
		const float Top = SetTabsTop + I * SetTabPitch;
		if (Design.X >= SetLeft - 10.f && Design.X <= SetLeft + 300.f && Design.Y >= Top - 10.f && Design.Y <= Top + SetTabPitch - 10.f)
		{
			if (I != CurrentPage)
			{
				SetPage(I);
			}
			return;
		}
	}
	const int32 R = RowAt(Design);
	if (R == INDEX_NONE)
	{
		return;
	}
	ChooseRow(R);
	const ESaltSetting S = Rows()[R];
	switch (KindOf(S))
	{
	case EKind::Choice:
		if (Design.X >= SetValueLeft - 10.f)
		{
			Change(S, Design.X < SetValueLeft + SetValueWidth * 0.5f ? -1 : 1);
		}
		break;
	case EKind::Number:
		if (Design.X >= SetValueLeft - 20.f && Design.X <= SetValueLeft + SetTrackWidth + 20.f)
		{
			DragRow = R;
			SetNumber(S, (Design.X - SetValueLeft) / SetTrackWidth);
		}
		break;
	default:
		Change(S, 1);
		break;
	}
}

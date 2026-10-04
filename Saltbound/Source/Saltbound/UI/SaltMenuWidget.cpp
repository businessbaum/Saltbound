// Saltbound - pause menu with the settings (display and look), built in code like the HUD.

#include "UI/SaltMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/SaltGameUserSettings.h"
#include "Core/SaltSaveSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor TextBright(0.96f, 0.92f, 0.84f, 1.f);   // the HUD's warm off-white
	const FLinearColor TextDim(0.78f, 0.74f, 0.66f, 1.f);
	const FLinearColor RowHighlight(0.85f, 0.62f, 0.30f, 0.22f);
	const FLinearColor PanelColor(0.055f, 0.045f, 0.035f, 0.88f);

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f));
		return Text;
	}

	FSlateBrush RoundedBrush(float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	/** Option lists. The frame caps include the display's refresh rate; 0 = unlimited. */
	TArray<float> FrameCaps()
	{
		TArray<float> Caps = { 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f };
		Caps.AddUnique(USaltGameUserSettings::DefaultFrameRateLimit());
		Caps.Sort();
		Caps.Add(0.f);
		return Caps;
	}
	TArray<float> Steps(float From, float To, float Step)
	{
		TArray<float> Out;
		for (float V = From; V <= To + Step * 0.01f; V += Step)
		{
			Out.Add(V);
		}
		return Out;
	}
	int32 Nearest(const TArray<float>& Options, float Value)
	{
		int32 Best = 0;
		for (int32 I = 1; I < Options.Num(); ++I)
		{
			if (FMath::Abs(Options[I] - Value) < FMath::Abs(Options[Best] - Value))
			{
				Best = I;
			}
		}
		return Best;
	}
	float StepOption(const TArray<float>& Options, float Value, int32 Delta)
	{
		const int32 N = Options.Num();
		return Options[((Nearest(Options, Value) + Delta) % N + N) % N];
	}
	const EWindowMode::Type WindowModes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	float ResolutionScale(const USaltGameUserSettings* S)
	{
		// 0 = never set (the engine's default): native resolution (r.ScreenPercentage 100, DefaultEngine.ini).
		float Normalized, Value, Min, Max;
		S->GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
		return Value > 1.f ? Value : 100.f;
	}
}

// --- Row -------------------------------------------------------------------------

void USaltMenuRow::Setup(USaltMenuWidget* InMenu, int32 InIndex, const FText& InLabel)
{
	Menu = InMenu;
	Index = InIndex;
	Label = InLabel;
}

TSharedRef<SWidget> USaltMenuRow::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Button = WidgetTree->ConstructWidget<USaltMenuButton>(USaltMenuButton::StaticClass(), TEXT("Button"));
		FButtonStyle Style;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
		Style.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		Button->SetStyle(Style);
		Button->OnClicked.AddDynamic(this, &USaltMenuRow::HandleClicked);
		Button->OnHovered.AddDynamic(this, &USaltMenuRow::HandleHovered);
		WidgetTree->RootWidget = Button;

		Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrush(RoundedBrush(6.f));
		Background->SetBrushColor(FLinearColor::Transparent);
		Background->SetPadding(FMargin(16.f, 7.f));
		Button->AddChild(Background);

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Line"));
		Background->SetContent(Line);
		LabelText = MakeText(WidgetTree, TEXT("Label"), 17, TextDim);
		LabelText->SetText(Label);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(LabelText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		ValueText = MakeText(WidgetTree, TEXT("Value"), 17, TextDim);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(ValueText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			S->SetVerticalAlignment(VAlign_Center);
		}
	}
	return Super::RebuildWidget();
}

void USaltMenuRow::SetValue(const FText& Value)
{
	if (ValueText)
	{
		ValueText->SetText(Value);
	}
}

void USaltMenuRow::SetSelected(bool bSelected)
{
	if (Background)
	{
		Background->SetBrushColor(bSelected ? RowHighlight : FLinearColor::Transparent);
		LabelText->SetColorAndOpacity(FSlateColor(bSelected ? TextBright : TextDim));
		ValueText->SetColorAndOpacity(FSlateColor(bSelected ? TextBright : TextDim));
	}
}

void USaltMenuRow::HandleClicked()
{
	if (USaltMenuWidget* M = Menu.Get())
	{
		M->SelectRow(Index);
		M->ActivateRow(Index);
	}
}

void USaltMenuRow::HandleHovered()
{
	if (USaltMenuWidget* M = Menu.Get())
	{
		M->SelectRow(Index);
	}
}

// --- Menu ------------------------------------------------------------------------

TSharedRef<SWidget> USaltMenuWidget::RebuildWidget()
{
	SetIsFocusable(true);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Rows = { ERow::Resume, ERow::Save, ERow::Load, ERow::VSync, ERow::FrameCap, ERow::ResolutionScale, ERow::WindowMode, ERow::FieldOfView,
			ERow::MouseSensitivity, ERow::InvertLook, ERow::Defaults, ERow::Quit };

		// Dimmed game behind a small dark panel in the middle.
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Backdrop;

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size"));
		Size->SetWidthOverride(560.f);
		Backdrop->SetContent(Size);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrush(RoundedBrush(12.f));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(26.f, 22.f));
		Size->SetContent(Panel);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
		Panel->SetContent(Box);
		UTextBlock* Title = MakeText(WidgetTree, TEXT("Title"), 28, TextBright);
		Title->SetText(NSLOCTEXT("Saltbound", "MenuTitle", "Paused"));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Title))
		{
			S->SetPadding(FMargin(16.f, 0.f, 0.f, 14.f));
		}

		static const TCHAR* Labels[] = { TEXT("Resume"), TEXT("Save game"), TEXT("Load last save"), TEXT("V-Sync"), TEXT("Frame rate limit"), TEXT("Resolution scale"),
			TEXT("Window mode"), TEXT("Field of view"), TEXT("Mouse sensitivity"), TEXT("Invert look"), TEXT("Restore defaults"),
			TEXT("Quit to desktop") };
		RowWidgets.Reset();
		for (int32 I = 0; I < Rows.Num(); ++I)
		{
			USaltMenuRow* Row = CreateWidget<USaltMenuRow>(this, USaltMenuRow::StaticClass());
			Row->Setup(this, I, FText::FromString(Labels[(int32)Rows[I]]));
			RowWidgets.Add(Row);
			if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row))
			{
				// A little air between the actions and the settings.
				const bool bGap = Rows[I] == ERow::VSync || Rows[I] == ERow::Defaults;
				S->SetPadding(FMargin(0.f, bGap ? 10.f : 1.f, 0.f, 1.f));
			}
		}

		Footer = MakeText(WidgetTree, TEXT("Footer"), 13, TextDim);
		Footer->SetText(NSLOCTEXT("Saltbound", "MenuFooter", "W / S  select     A / D  change     Enter  use     Esc / F10  close"));
		Footer->SetRenderOpacity(0.7f);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Footer))
		{
			S->SetPadding(FMargin(16.f, 16.f, 0.f, 0.f));
		}
	}
	return Super::RebuildWidget();
}

void USaltMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Selected = 0;
	Refresh();
}

void USaltMenuWidget::Refresh()
{
	for (int32 I = 0; I < RowWidgets.Num(); ++I)
	{
		const FText Value = ValueOf(Rows[I]);
		const bool bOption = IsOption(Rows[I]) && !Value.IsEmpty();
		RowWidgets[I]->SetValue(bOption && I == Selected ? FText::Format(NSLOCTEXT("Saltbound", "MenuValueSelected", "<   {0}   >"), Value) : Value);
		RowWidgets[I]->SetSelected(I == Selected);
	}
}

FText USaltMenuWidget::ValueOf(ERow Row) const
{
	if (Row == ERow::Save || Row == ERow::Load)
	{
		// When the game was last saved; loading needs a save.
		const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
		if (!Saves || !Saves->HasSave())
		{
			return Row == ERow::Load ? NSLOCTEXT("Saltbound", "NoSave", "no save yet") : FText::GetEmpty();
		}
		return FText::FromString(Saves->GetSaveTime().ToString(TEXT("%d.%m. %H:%M")));
	}
	const USaltGameUserSettings* S = USaltGameUserSettings::Get();
	if (!S)
	{
		return Row == ERow::Resume || Row == ERow::Defaults || Row == ERow::Quit ? FText::GetEmpty() : FText::FromString(TEXT("-"));
	}
	auto OnOff = [](bool b) { return b ? NSLOCTEXT("Saltbound", "On", "On") : NSLOCTEXT("Saltbound", "Off", "Off"); };
	switch (Row)
	{
	case ERow::VSync:
		// The editor keeps its own vsync off ([SystemSettingsEditor] r.VSync=0 in BaseEngine.ini).
		return GIsEditor ? FText::Format(NSLOCTEXT("Saltbound", "GameOnly", "{0} (game only)"), OnOff(S->IsVSyncEnabled()))
			: OnOff(S->IsVSyncEnabled());
	case ERow::FrameCap:
		return S->GetFrameRateLimit() <= 0.f ? NSLOCTEXT("Saltbound", "Unlimited", "Unlimited")
			: FText::FromString(FString::Printf(TEXT("%.0f fps"), S->GetFrameRateLimit()));
	case ERow::ResolutionScale:
		return FText::FromString(FString::Printf(TEXT("%.0f %%"), ResolutionScale(S)));
	case ERow::WindowMode:
	{
		const EWindowMode::Type Mode = S->GetFullscreenMode();
		FString Name = Mode == EWindowMode::Fullscreen ? TEXT("Fullscreen") : Mode == EWindowMode::WindowedFullscreen ? TEXT("Borderless") : TEXT("Windowed");
		if (GIsEditor)
		{
			Name += TEXT(" (game only)");
		}
		return FText::FromString(Name);
	}
	case ERow::FieldOfView:
		return FText::FromString(FString::Printf(TEXT("%.0f deg"), S->FieldOfView));
	case ERow::MouseSensitivity:
		return FText::FromString(FString::Printf(TEXT("%.2f"), S->MouseSensitivity));
	case ERow::InvertLook:
		return OnOff(S->bInvertLookY);
	default:
		return FText::GetEmpty();
	}
}

void USaltMenuWidget::SelectRow(int32 Row)
{
	if (Rows.IsValidIndex(Row) && Row != Selected)
	{
		Selected = Row;
		Refresh();
	}
}

void USaltMenuWidget::ChangeRow(int32 Row, int32 Delta)
{
	USaltGameUserSettings* S = USaltGameUserSettings::Get();
	if (!S || !Rows.IsValidIndex(Row) || Delta == 0)
	{
		return;
	}
	bool bDisplay = true;
	switch (Rows[Row])
	{
	case ERow::VSync:
		S->SetVSyncEnabled(!S->IsVSyncEnabled());
		break;
	case ERow::FrameCap:
	{
		// Unlimited (0) is the last entry; the others are matched to the nearest cap.
		const TArray<float> Caps = FrameCaps();
		const int32 N = Caps.Num();
		const int32 Current = S->GetFrameRateLimit() <= 0.f ? N - 1 : Nearest(TArray<float>(Caps.GetData(), N - 1), S->GetFrameRateLimit());
		S->SetFrameRateLimit(Caps[((Current + Delta) % N + N) % N]);
		break;
	}
	case ERow::ResolutionScale:
		S->SetResolutionScaleValueEx(StepOption(Steps(50.f, 100.f, 5.f), ResolutionScale(S), Delta));
		break;
	case ERow::WindowMode:
	{
		int32 I = 0;
		while (I < 2 && WindowModes[I] != S->GetFullscreenMode())
		{
			++I;
		}
		S->SetFullscreenMode(WindowModes[((I + Delta) % 3 + 3) % 3]);
		break;
	}
	case ERow::FieldOfView:
		S->FieldOfView = StepOption(Steps(70.f, 110.f, 5.f), S->FieldOfView, Delta);
		bDisplay = false;
		break;
	case ERow::MouseSensitivity:
		S->MouseSensitivity = StepOption(Steps(0.25f, 3.f, 0.25f), S->MouseSensitivity, Delta);
		bDisplay = false;
		break;
	case ERow::InvertLook:
		S->bInvertLookY = !S->bInvertLookY;
		bDisplay = false;
		break;
	default:
		return;
	}
	if (bDisplay)
	{
		S->ApplyDisplay();
	}
	else
	{
		S->SaveSettings();
		if (ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
		{
			Player->ApplyUserSettings();
		}
	}
	Refresh();
}

void USaltMenuWidget::ActivateRow(int32 Row)
{
	if (!Rows.IsValidIndex(Row))
	{
		return;
	}
	switch (Rows[Row])
	{
	case ERow::Resume:
		RequestClose();
		break;
	case ERow::Save:
		if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
		{
			Saves->SaveGame(GetWorld(), TEXT("menu"));
			Refresh();
		}
		break;
	case ERow::Load:
		if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
		{
			if (Saves->HasSave())
			{
				RequestClose();
				Saves->LoadGame(GetWorld());
			}
		}
		break;
	case ERow::Defaults:
		if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
		{
			S->SetToDefaults();
			S->ApplyDisplay();
			if (ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
			{
				Player->ApplyUserSettings();
			}
			Refresh();
		}
		break;
	case ERow::Quit:
		if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
		{
			Saves->Autosave(GetWorld(), TEXT("quit"));
		}
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	default:
		ChangeRow(Row, 1);
		break;
	}
}

FString USaltMenuWidget::DescribeRow(int32 Row) const
{
	return Rows.IsValidIndex(Row) ? ValueOf(Rows[Row]).ToString() : FString();
}

void USaltMenuWidget::RequestClose()
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
	{
		PC->SetMenuOpen(false);
	}
}

FReply USaltMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	auto Is = [&Key](std::initializer_list<FKey> Keys)
	{
		for (const FKey& K : Keys)
		{
			if (Key == K)
			{
				return true;
			}
		}
		return false;
	};
	if (Is({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up }))
	{
		SelectRow((Selected + Rows.Num() - 1) % Rows.Num());
	}
	else if (Is({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down }))
	{
		SelectRow((Selected + 1) % Rows.Num());
	}
	else if (Is({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left }))
	{
		ChangeRow(Selected, -1);
	}
	else if (Is({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right }))
	{
		ChangeRow(Selected, 1);
	}
	else if (Is({ EKeys::Enter, EKeys::SpaceBar, EKeys::E, EKeys::Gamepad_FaceButton_Bottom }))
	{
		ActivateRow(Selected);
	}
	else if (Is({ EKeys::Escape, EKeys::F10, EKeys::Gamepad_Special_Right, EKeys::Gamepad_FaceButton_Right }))
	{
		RequestClose();
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

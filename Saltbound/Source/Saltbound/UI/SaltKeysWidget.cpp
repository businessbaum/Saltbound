// Saltbound - keys and buttons (D66).

#include "UI/SaltKeysWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SaltGameUserSettings.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltPaintedText.h"

namespace
{
	// Menus.html "Keys and buttons" at 1080p: the title at (8cqw, 5.5cqw); the columns action / keyboard / gamepad at 8, 32 and
	// 59.9cqw; the rows a little tighter than the page's (ten actions and the reset have to fit); the scrap for a clash 52cqw wide
	// at the bottom, turned -0.5 degrees.
	constexpr float KeysLeft = 154.f;
	constexpr float KeysColKeys = 614.f;
	constexpr float KeysColPad = 1151.f;
	constexpr float KeysHeadY = 205.f;
	constexpr float KeysTop = 236.f;
	constexpr float KeysPitch = 58.f;
	constexpr float KeysRowHeight = 56.f;
	constexpr float KeysScrap = 46.f;
	constexpr float KeysText = 32.f;
	const FVector2D KeysNoteAt(154.f, 935.f);
	const FVector2D KeysNoteSize(998.f, 100.f);
	constexpr float KeysNoteShown = 2.6f;
}

USaltKeysWidget::USaltKeysWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	BackdropKind = EBackdrop::Blurred;
	bShadeLeft = false;
}

int32 USaltKeysWidget::RowCount() const
{
	return USaltInputConfig::Rebindables().Num() + 1;
}

void USaltKeysWidget::Build()
{
	SaltUI::At(Contents, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltKeys", "Title", "Keys and buttons"), ESaltFace::Fell, 50.f),
		FVector2D(KeysLeft, 106.f + 25.f), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
	const FText Heads[] = { NSLOCTEXT("SaltKeys", "Action", "Action"), NSLOCTEXT("SaltKeys", "Keyboard", "Keyboard"), NSLOCTEXT("SaltKeys", "Gamepad", "Gamepad") };
	const float HeadX[] = { KeysLeft, KeysColKeys, KeysColPad };
	for (int32 I = 0; I < 3; ++I)
	{
		USaltPaintedText* H = SaltMenu::Paint(WidgetTree, Heads[I], ESaltFace::FellSC, 24.f, ETextJustify::Left, 4.8f);
		H->SetRenderOpacity(0.7f);
		SaltUI::At(Contents, H, FVector2D(HeadX[I], KeysHeadY), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
	}
	Table = WidgetTree->ConstructWidget<UCanvasPanel>();
	UCanvasPanelSlot* S = Contents->AddChildToCanvas(Table);
	S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	S->SetOffsets(FMargin(0.f));

	// The scrap for a clash or a note, hidden until needed.
	NoteScrap = WidgetTree->ConstructWidget<UCanvasPanel>();
	SaltUI::At(Contents, NoteScrap, KeysNoteAt, KeysNoteSize);
	NoteScrap->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NoteScrap->SetRenderTransformAngle(-0.5f);
	UImage* Paper = WidgetTree->ConstructWidget<UImage>();
	Paper->SetBrush(SaltUI::BoxBrush(SaltUI::Tex(TEXT("T_UI_MenuScrap")), FVector2D(905.f, 520.f), 0.12f));
	if (UCanvasPanelSlot* PS = NoteScrap->AddChildToCanvas(Paper))
	{
		PS->SetPosition(FVector2D(-30.f, -30.f));
		PS->SetSize(KeysNoteSize + FVector2D(60.f, 60.f));
	}
	NoteText = SaltUI::InkText(WidgetTree, FText::GetEmpty(), ESaltFace::Alegreya, 31.f);
	if (UCanvasPanelSlot* TS = NoteScrap->AddChildToCanvas(NoteText))
	{
		TS->SetAnchors(FAnchors(0.f, 0.5f));
		TS->SetAlignment(FVector2D(0.f, 0.5f));
		TS->SetPosition(FVector2D(38.f, 0.f));
		TS->SetAutoSize(true);
	}
	NoteScrap->SetVisibility(ESlateVisibility::Collapsed);
	Rebuild();
}

void USaltKeysWidget::Rebuild()
{
	if (!Table)
	{
		return;
	}
	Table->ClearChildren();
	Caret = nullptr;
	const TArray<FSaltRebindable>& List = USaltInputConfig::Rebindables();
	for (int32 I = 0; I < RowCount(); ++I)
	{
		const float Top = KeysTop + I * KeysPitch;
		const float Mid = Top + KeysRowHeight * 0.5f;
		if (I == Row)
		{
			UImage* Wash = WidgetTree->ConstructWidget<UImage>();
			Wash->SetBrush(SaltUI::ImageBrush(SaltMenu::Gradient(TEXT("RowWash"), true, { { 0.f, 242, 226, 191, 0.13f }, { 1.f, 242, 226, 191, 0.f } }),
				FVector2D(1440.f, KeysRowHeight)));
			SaltUI::At(Table, Wash, FVector2D(KeysLeft - 20.f, Top), FVector2D(1440.f, KeysRowHeight));
		}
		if (I == List.Num())
		{
			SaltUI::At(Table, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltKeys", "Every", "Every key"), ESaltFace::Fell, KeysText), FVector2D(KeysLeft, Mid),
				FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
			SaltUI::At(Table, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltKeys", "PutBack", "Put back ›"), ESaltFace::Fell, KeysText * 0.85f),
				FVector2D(KeysColKeys, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
			continue;
		}
		const FSaltRebindable& R = List[I];
		SaltUI::At(Table, SaltMenu::Paint(WidgetTree, R.Label, ESaltFace::Fell, KeysText), FVector2D(KeysLeft, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
		if (I == Row && State == EState::Listen)
		{
			// Waiting for the key: the words and a blinking quill caret after them.
			USaltPaintedText* Wait = SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltKeys", "Press", "Press a key or button"), ESaltFace::FellItalic, KeysText);
			SaltUI::At(Table, Wait, FVector2D(KeysColKeys, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
			Caret = WidgetTree->ConstructWidget<UImage>();
			Caret->SetColorAndOpacity(SaltUI::SRGB(0xF2, 0xE2, 0xBF));
			SaltUI::At(Table, Caret, FVector2D(KeysColKeys + Wait->MeasureWidth() + 6.f, Mid), FVector2D(3.f, 36.f), FVector2D(0.f, 0.5f));
			continue;
		}
		// The key (and the keys that always work too, fainter), then the gamepad's button.
		UHorizontalBox* Keys = WidgetTree->ConstructWidget<UHorizontalBox>();
		Keys->AddChildToHorizontalBox(SaltMenu::KeyScrap(WidgetTree, SaltMenu::KeyName(USaltInputConfig::KeyOf(R.Id)), KeysScrap))->SetVerticalAlignment(VAlign_Center);
		for (const FKey& Fixed : R.Fixed)
		{
			UWidget* F = SaltMenu::KeyScrap(WidgetTree, SaltMenu::KeyName(Fixed), KeysScrap);
			F->SetRenderOpacity(0.75f);
			UHorizontalBoxSlot* FS = Keys->AddChildToHorizontalBox(F);
			FS->SetVerticalAlignment(VAlign_Center);
			FS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		SaltUI::At(Table, Keys, FVector2D(KeysColKeys, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
		const FString Pad = R.Pad.ToString();
		if (Pad.Len() <= 4)
		{
			SaltUI::At(Table, SaltMenu::KeyScrap(WidgetTree, Pad, KeysScrap), FVector2D(KeysColPad, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
		}
		else
		{
			SaltUI::At(Table, SaltMenu::Paint(WidgetTree, R.Pad, ESaltFace::FellItalic, 26.f), FVector2D(KeysColPad, Mid), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));
		}
	}

	const bool bNote = State == EState::Clash || NoteAge > 0.f;
	NoteScrap->SetVisibility(bNote ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (State == EState::Listen)
	{
		PlaceHints({ { { TEXT("Esc") }, NSLOCTEXT("SaltKeys", "HintKeep", "keep the old key") } }, KeysLeft, false);
	}
	else
	{
		PlaceHints({ { { TEXT("W"), TEXT("S") }, NSLOCTEXT("SaltKeys", "HintChoose", "choose") }, { { TEXT("E") }, NSLOCTEXT("SaltKeys", "HintChange", "change") },
			{ { TEXT("Esc") }, NSLOCTEXT("SaltKeys", "HintBack", "back") } }, KeysLeft, false);
	}
	HintsRow->SetVisibility(bNote ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void USaltKeysWidget::ChooseRow(int32 InRow)
{
	if (InRow >= 0 && InRow < RowCount() && InRow != Row)
	{
		Row = InRow;
		Rebuild();
	}
}

void USaltKeysWidget::Listen()
{
	if (Row == USaltInputConfig::Rebindables().Num())
	{
		// The last row: every key back.
		ASaltPlayerController* PC = GetSaltController();
		if (!PC)
		{
			return;
		}
		TWeakObjectPtr<USaltKeysWidget> Self(this);
		PC->Ask(NSLOCTEXT("SaltKeys", "AskReset", "Put every key back?"), NSLOCTEXT("SaltKeys", "AskResetBody", "Every action goes back to the key it came with."),
			NSLOCTEXT("SaltKeys", "AskResetYes", "put them back"), NSLOCTEXT("SaltKeys", "AskResetNo", "keep mine"),
			[Self, PC]()
			{
				if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
				{
					S->KeyBindings.Reset();
					S->SaveSettings();
				}
				PC->RebuildInputMappings();
				if (Self.IsValid())
				{
					Self->Rebuild();
				}
			});
		return;
	}
	State = EState::Listen;
	NoteAge = 0.f;
	Blink = 0.f;
	Rebuild();
}

void USaltKeysWidget::ShowNote(const FText& Text)
{
	NoteText->SetText(Text);
	NoteAge = KeysNoteShown;
}

void USaltKeysWidget::Offer(const FKey& Key)
{
	const TArray<FSaltRebindable>& List = USaltInputConfig::Rebindables();
	if (State != EState::Listen || !List.IsValidIndex(Row))
	{
		return;
	}
	const FSaltRebindable& R = List[Row];
	if (USaltInputConfig::IsReserved(Key))
	{
		State = EState::Browse;
		ShowNote(Key == EKeys::Escape || Key == EKeys::F10
				? FText::Format(NSLOCTEXT("SaltKeys", "NoteMenu", "{0} is kept for the pause menu."), FText::FromString(SaltMenu::KeyName(Key)))
			: Key.IsMouseButton() ? NSLOCTEXT("SaltKeys", "NoteMouse", "The mouse buttons are not changed here.")
			: Key.IsGamepadKey() ? NSLOCTEXT("SaltKeys", "NotePad", "The gamepad's buttons are not changed here.")
			: NSLOCTEXT("SaltKeys", "NoteTools", "The keys 1 to 5 take the tools."));
		Rebuild();
		return;
	}
	if (Key == USaltInputConfig::KeyOf(R.Id) || R.Fixed.Contains(Key))
	{
		Commit(R.Id, Key);
		return;
	}
	const FName Other = USaltInputConfig::UsedBy(Key, R.Id);
	if (Other.IsNone())
	{
		Commit(R.Id, Key);
		return;
	}
	const FSaltRebindable* O = USaltInputConfig::FindRebindable(Other);
	Pending = Key;
	ClashWith = Other;
	bClashFixed = O && O->Fixed.Contains(Key);
	State = EState::Clash;
	NoteAge = 0.f;
	NoteText->SetText(bClashFixed
		? FText::Format(NSLOCTEXT("SaltKeys", "ClashFixed", "{0} also works for {1}.  E take it anyway · Esc choose another"),
			FText::FromString(SaltMenu::KeyName(Key)), O->Label)
		: FText::Format(NSLOCTEXT("SaltKeys", "Clash", "{0} is already used by {1}.  E swap them · Esc keep both"),
			FText::FromString(SaltMenu::KeyName(Key)), O ? O->Label : FText::GetEmpty()));
	Rebuild();
}

void USaltKeysWidget::Resolve(bool bSwap)
{
	const TArray<FSaltRebindable>& List = USaltInputConfig::Rebindables();
	if (State != EState::Clash || !List.IsValidIndex(Row))
	{
		return;
	}
	const FSaltRebindable& R = List[Row];
	if (bClashFixed)
	{
		if (bSwap)
		{
			Commit(R.Id, Pending);
		}
		else
		{
			State = EState::Listen;
			Rebuild();
		}
		return;
	}
	if (bSwap)
	{
		// The other action takes this one's old key.
		const FKey Old = USaltInputConfig::KeyOf(R.Id);
		if (const FSaltRebindable* O = USaltInputConfig::FindRebindable(ClashWith))
		{
			if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
			{
				S->SetKey(O->Id, Old, O->Default);
			}
		}
	}
	Commit(R.Id, Pending);
}

void USaltKeysWidget::Commit(FName Id, const FKey& Key)
{
	if (USaltGameUserSettings* S = USaltGameUserSettings::Get())
	{
		if (const FSaltRebindable* R = USaltInputConfig::FindRebindable(Id))
		{
			S->SetKey(Id, Key, R->Default);
		}
		S->SaveSettings();
	}
	if (ASaltPlayerController* PC = GetSaltController())
	{
		PC->RebuildInputMappings();
	}
	State = EState::Browse;
	Rebuild();
}

void USaltKeysWidget::Back()
{
	if (State == EState::Listen)
	{
		State = EState::Browse;
		Rebuild();
	}
	else if (State == EState::Clash)
	{
		Resolve(false);
	}
	else
	{
		Super::Back();
	}
}

void USaltKeysWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Blink += InDeltaTime;
	if (Caret)
	{
		Caret->SetRenderOpacity(FMath::Fmod(Blink, 1.f) < 0.5f ? 1.f : 0.f);
	}
	if (NoteAge > 0.f)
	{
		NoteAge -= InDeltaTime;
		if (NoteAge <= 0.f)
		{
			NoteAge = 0.f;
			Rebuild();
		}
	}
}

void USaltKeysWidget::Navigate(int32 Dx, int32 Dy)
{
	if (State == EState::Browse && Dy != 0)
	{
		ChooseRow((Row + Dy + RowCount()) % RowCount());
	}
}

void USaltKeysWidget::Accept()
{
	if (State == EState::Browse)
	{
		Listen();
	}
}

bool USaltKeysWidget::RawKey(const FKey& Key, bool bRepeat)
{
	if (bRepeat && State != EState::Browse)
	{
		return true;
	}
	if (State == EState::Listen)
	{
		if (Key == EKeys::Escape)
		{
			Back();
		}
		else
		{
			Offer(Key);
		}
		return true;
	}
	if (State == EState::Clash)
	{
		if (Key == EKeys::E || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			Resolve(true);
		}
		else if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
		{
			Resolve(false);
		}
		return true;
	}
	return false;
}

int32 USaltKeysWidget::RowAt(const FVector2D& Design) const
{
	const int32 R = FMath::FloorToInt((Design.Y - KeysTop) / KeysPitch);
	return Design.Y >= KeysTop && Design.X >= KeysLeft - 20.f && Design.X <= KeysLeft + 1420.f && R >= 0 && R < RowCount() ? R : INDEX_NONE;
}

void USaltKeysWidget::PointerMove(const FVector2D& Design)
{
	const int32 R = RowAt(Design);
	if (State == EState::Browse && R != INDEX_NONE)
	{
		ChooseRow(R);
	}
}

void USaltKeysWidget::PointerDown(const FVector2D& Design)
{
	if (State != EState::Browse)
	{
		return;
	}
	const int32 R = RowAt(Design);
	if (R != INDEX_NONE)
	{
		ChooseRow(R);
		Listen();
	}
}

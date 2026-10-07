// Saltbound - a person speaks (FH2, D81).

#include "UI/SaltTalkWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "UI/SaltMenuKit.h"
#include "UI/SaltUIStyle.h"
#include "World/SaltTalk.h"

using namespace SaltUI;

namespace
{
	// The sheet at 1080p (Tools/textures/ui_parts.js): the picture is 1000 x 500 with the paper (960 x 460) at 20, 14; the paper's
	// foot stands at y 1036 (the design's), so the picture's at 1062.
	constexpr float SheetW = 1000.f;
	constexpr float SheetFoot = 1062.f;
	const FMargin SheetPad(112.f, 40.f, 78.f, 40.f);
	constexpr float LineSize = 32.f;
	constexpr float LineWrap = 810.f;
	/** 42 px lines: the faces' own line is about 1.268 em. */
	constexpr float LineHeight = 42.f / (LineSize * 1.268f);
	constexpr float NameSize = 24.f;
	constexpr float ChoiceSize = 29.f;
	constexpr float ChoiceRow = 54.f;
	constexpr float KeySize = 44.f;
	/** The ink writes the line out at this many letters a second, a word at a time. */
	constexpr float WriteRate = 45.f;
	constexpr float OpenTime = 0.18f;
	constexpr float CloseTime = 0.15f;

	// A call in passing: the picture is 540 x 240 with the paper (500 x 200) at 20, 14.
	constexpr float CallW = 540.f;
	const FMargin CallPad(100.f, 40.f, 60.f, 44.f);
	constexpr float CallFade = 0.25f;

	FLinearColor NameInk() { return SRGB(107, 74, 44, 0.95f); }
	FLinearColor ChoiceInk(bool bOn) { return bOn ? SRGB(30, 16, 6, 0.98f) : SRGB(58, 38, 22, 0.85f); }

	UTexture2D* SealTex(FName Seal)
	{
		UTexture2D* T = Tex(*FString::Printf(TEXT("T_UI_TalkSeal_%s"), *Seal.ToString()));
		return T ? T : Tex(TEXT("T_UI_TalkSeal_Anchor"));
	}

	float TextWidth(const FText& Text, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return Text.ToString().Len() * Font.Size * 0.55f;
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font).X;
	}

	/** An answer's key on its scrap (the prompts' scrap, turned -3 degrees). Digits are painted in Alegreya Bold, as the design's:
	 *  the small capitals' old-style 1 reads as an I. */
	UWidget* KeyOnScrap(UWidgetTree* Tree, const FString& Key)
	{
		if (Key.Len() != 1 || !FChar::IsDigit(Key[0]))
		{
			return SaltMenu::KeyScrap(Tree, SaltMenu::ShownKey(Key), KeySize);
		}
		const float K = KeySize / 58.f;
		const FVector2D Frame = FVector2D(120.f, 120.f) * K;
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(KeySize);
		Box->SetHeightOverride(KeySize);
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanel* C = Tree->ConstructWidget<UCanvasPanel>();
		Box->AddChild(C);
		UImage* Img = Tree->ConstructWidget<UImage>();
		Img->SetBrush(ImageBrush(Tex(TEXT("T_UI_KeyScrap")), Frame));
		UCanvasPanelSlot* S = C->AddChildToCanvas(Img);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(Frame);
		UTextBlock* T = InkText(Tree, FText::FromString(Key), ESaltFace::AlegreyaBold, KeySize * 0.55f, ETextJustify::Center);
		UCanvasPanelSlot* TS = C->AddChildToCanvas(T);
		TS->SetAnchors(FAnchors(0.5f, 0.5f));
		TS->SetAlignment(FVector2D(0.5f, 0.5f));
		TS->SetAutoSize(true);
		TS->SetPosition(FVector2D(0.f, 1.f));
		C->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		C->SetRenderTransformAngle(-3.f);
		return Box;
	}

	UTextBlock* Words(UWidgetTree* Tree, ESaltFace Face, float Size, float Wrap, float LineHeightPct)
	{
		UTextBlock* T = InkText(Tree, FText::GetEmpty(), Face, Size);
		T->SetAutoWrapText(false);
		T->SetWrapTextAt(Wrap);
		T->SetLineHeightPercentage(LineHeightPct);
		T->SetApplyLineHeightToBottomLine(false);
		return T;
	}
}

TSharedRef<SWidget> USaltTalkWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	return Super::RebuildWidget();
}

void USaltTalkWidget::Build()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	// --- the sheet ---
	SheetBox = WidgetTree->ConstructWidget<USizeBox>();
	SheetBox->SetWidthOverride(SheetW);
	SheetBox->SetVisibility(ESlateVisibility::Collapsed);
	At(Root, SheetBox, FVector2D(960.f, SheetFoot), FVector2D::ZeroVector, FVector2D(0.5f, 1.f));
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	SheetBox->AddChild(Layers);

	UBorder* Paper = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush PaperBrush = BoxBrush(Tex(TEXT("T_UI_TalkScrap")), FVector2D(SheetW, 500.f));
	PaperBrush.Margin = FMargin(0.06f, 0.2f, 0.06f, 0.2f);
	Paper->SetBrush(PaperBrush);
	Paper->SetPadding(SheetPad);
	Layers->AddChildToOverlay(Paper);

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
	Paper->SetContent(Body);
	Name = InkText(WidgetTree, FText::GetEmpty(), ESaltFace::FellSC, NameSize);
	Name->SetColorAndOpacity(FSlateColor(NameInk()));
	Body->AddChildToVerticalBox(Name);
	NameRule = WidgetTree->ConstructWidget<USizeBox>();
	NameRule->SetHeightOverride(14.f);
	UImage* Rule = WidgetTree->ConstructWidget<UImage>();
	Rule->SetBrush(ImageBrush(Tex(TEXT("T_UI_QuillRule")), FVector2D(400.f, 14.f)));
	NameRule->AddChild(Rule);
	if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(NameRule))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
	// The line twice: the whole of it unseen (it gives the sheet its height from the start) and the words written so far over it.
	UOverlay* LineLayers = WidgetTree->ConstructWidget<UOverlay>();
	LineFull = Words(WidgetTree, ESaltFace::Fell, LineSize, LineWrap, LineHeight);
	LineFull->SetRenderOpacity(0.f);
	LineWritten = Words(WidgetTree, ESaltFace::Fell, LineSize, LineWrap, LineHeight);
	LineLayers->AddChildToOverlay(LineFull);
	LineLayers->AddChildToOverlay(LineWritten);
	if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(LineLayers))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	Choices = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(Choices))
	{
		S->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));
	}

	// The speaker's mark in wax over the sheet's corner (the design's at the paper's 38, 46, radius 34).
	Seal = WidgetTree->ConstructWidget<UImage>();
	Seal->SetBrush(ImageBrush(SealTex(TEXT("Anchor")), FVector2D(100.f, 100.f)));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(Seal))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(8.f, 10.f, 0.f, 0.f));
	}

	// --- a call in passing ---
	CallBox = WidgetTree->ConstructWidget<USizeBox>();
	CallBox->SetWidthOverride(CallW);
	CallBox->SetVisibility(ESlateVisibility::Collapsed);
	CallBox->SetRenderTransformPivot(FVector2D(0.f, 1.f));
	CallBox->SetRenderTransformAngle(1.4f);
	At(Root, CallBox, FVector2D(1220.f, 588.f), FVector2D::ZeroVector, FVector2D(0.f, 1.f));
	UOverlay* CallLayers = WidgetTree->ConstructWidget<UOverlay>();
	CallBox->AddChild(CallLayers);
	UBorder* CallPaper = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush CallBrush = BoxBrush(Tex(TEXT("T_UI_CallScrap")), FVector2D(CallW, 240.f));
	CallBrush.Margin = FMargin(0.1f, 0.25f, 0.1f, 0.25f);
	CallPaper->SetBrush(CallBrush);
	CallPaper->SetPadding(CallPad);
	CallLayers->AddChildToOverlay(CallPaper);
	UVerticalBox* CallBody = WidgetTree->ConstructWidget<UVerticalBox>();
	CallPaper->SetContent(CallBody);
	CallName = InkText(WidgetTree, FText::GetEmpty(), ESaltFace::FellSC, 20.f);
	CallName->SetColorAndOpacity(FSlateColor(NameInk()));
	CallBody->AddChildToVerticalBox(CallName);
	CallLine = Words(WidgetTree, ESaltFace::Fell, 28.f, 380.f, 36.f / (28.f * 1.268f));
	if (UVerticalBoxSlot* S = CallBody->AddChildToVerticalBox(CallLine))
	{
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}
	CallSeal = WidgetTree->ConstructWidget<UImage>();
	CallSeal->SetBrush(ImageBrush(SealTex(TEXT("Anchor")), FVector2D(76.f, 76.f)));
	if (UOverlaySlot* S = CallLayers->AddChildToOverlay(CallSeal))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(14.f, 16.f, 0.f, 0.f));
	}
}

void USaltTalkWidget::ShowLine(const FText& InName, FName InSeal, const FText& Text, const TArray<FSaltShownChoice>& InChoices)
{
	if (!Root)
	{
		TakeWidget();
	}
	if (!Root)
	{
		return;
	}
	const FText Upper = InName.ToUpper();
	Name->SetText(Upper);
	NameRule->SetWidthOverride(FMath::Min(TextWidth(Upper, Name->GetFont()) + 30.f, LineWrap));
	Seal->SetBrush(ImageBrush(SealTex(InSeal), FVector2D(100.f, 100.f)));
	FullText = Text.ToString();
	LineFull->SetText(Text);
	WriteT = 0.f;
	WrittenChars = -1;
	SetWritten(0);

	Choices->ClearChildren();
	ChoiceRows.Reset();
	ChoiceGlows.Reset();
	ChoiceRules.Reset();
	ChoiceTexts.Reset();
	for (const FSaltShownChoice& C : InChoices)
	{
		USizeBox* Row = WidgetTree->ConstructWidget<USizeBox>();
		Row->SetHeightOverride(ChoiceRow);
		UOverlay* O = WidgetTree->ConstructWidget<UOverlay>();
		Row->AddChild(O);
		// The warm wash behind a taken answer (the design's starts 30 px left of the keys).
		UImage* Glow = WidgetTree->ConstructWidget<UImage>();
		Glow->SetBrush(ImageBrush(Tex(TEXT("T_UI_TalkChoiceGlow")), FVector2D(700.f, 52.f)));
		Glow->SetRenderTranslation(FVector2D(-30.f, 0.f));
		Glow->SetRenderOpacity(0.f);
		if (UOverlaySlot* S = O->AddChildToOverlay(Glow))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetVerticalAlignment(VAlign_Center);
		}
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		O->AddChildToOverlay(H);
		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>();
		KeyBox->SetWidthOverride(62.f);
		KeyBox->AddChild(KeyOnScrap(WidgetTree, C.Key));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(KeyBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* T = InkText(WidgetTree, C.Text, ESaltFace::Fell, ChoiceSize);
		T->SetColorAndOpacity(FSlateColor(ChoiceInk(false)));
		TextCol->AddChildToVerticalBox(T);
		USizeBox* RuleBox = WidgetTree->ConstructWidget<USizeBox>();
		RuleBox->SetHeightOverride(8.f);
		RuleBox->SetWidthOverride(TextWidth(C.Text, T->GetFont()));
		UImage* R = WidgetTree->ConstructWidget<UImage>();
		R->SetBrush(ImageBrush(Tex(TEXT("T_UI_QuillRule")), FVector2D(400.f, 8.f)));
		RuleBox->AddChild(R);
		RuleBox->SetRenderOpacity(0.f);
		if (UVerticalBoxSlot* S = TextCol->AddChildToVerticalBox(RuleBox))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetPadding(FMargin(0.f, -4.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(TextCol))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Choices->AddChildToVerticalBox(Row);
		ChoiceRows.Add(Row);
		ChoiceGlows.Add(Glow);
		ChoiceRules.Add(RuleBox);
		ChoiceTexts.Add(T);
	}
	Choices->SetRenderOpacity(0.f);
	ChoicesAge = -1.f;

	if (!bLine || HideAge >= 0.f)
	{
		LineAge = 0.f;
	}
	bLine = true;
	HideAge = -1.f;
	SheetBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USaltTalkWidget::HideLine()
{
	if (bLine && HideAge < 0.f)
	{
		HideAge = 0.f;
	}
	bLine = false;
}

bool USaltTalkWidget::IsWriting() const
{
	return bLine && WrittenChars < FullText.Len();
}

void USaltTalkWidget::FinishWriting()
{
	SetWritten(FullText.Len());
}

void USaltTalkWidget::SetWritten(int32 Chars)
{
	// A word at a time: a prefix of whole words breaks into lines as the whole line does, so nothing jumps while it is written.
	int32 End = FMath::Clamp(Chars, 0, FullText.Len());
	while (End < FullText.Len() && End > 0 && !FChar::IsWhitespace(FullText[End]))
	{
		--End;
	}
	if (Chars >= FullText.Len())
	{
		End = FullText.Len();
	}
	if (End == WrittenChars)
	{
		return;
	}
	WrittenChars = End;
	if (End >= FullText.Len())
	{
		WrittenChars = FullText.Len();
		WriteT = FullText.Len() / WriteRate;
	}
	LineWritten->SetText(FText::FromString(FullText.Left(WrittenChars)));
}

void USaltTalkWidget::MarkChoice(int32 Shown)
{
	for (int32 I = 0; I < ChoiceRows.Num(); ++I)
	{
		const bool bOn = I == Shown;
		ChoiceGlows[I]->SetRenderOpacity(bOn ? 1.f : 0.f);
		ChoiceRules[I]->SetRenderOpacity(bOn ? 1.f : 0.f);
		ChoiceTexts[I]->SetColorAndOpacity(FSlateColor(ChoiceInk(bOn)));
	}
}

void USaltTalkWidget::ShowCall(const FText& InName, FName InSeal, const FText& Text, const FVector& WorldAt, float Seconds)
{
	if (!Root)
	{
		TakeWidget();
	}
	if (!Root)
	{
		return;
	}
	CallName->SetText(InName.ToUpper());
	CallLine->SetText(Text);
	CallSeal->SetBrush(ImageBrush(SealTex(InSeal), FVector2D(76.f, 76.f)));
	CallAt = WorldAt;
	CallLeft = FMath::Max(Seconds, 0.5f);
	CallAge = 0.f;
	CallBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceCall();
}

void USaltTalkWidget::PlaceCall()
{
	// Pinned beside the speaker (up and to the right of their head); a speaker behind the view or off the screen pins it to the
	// nearer side.
	APlayerController* PC = GetOwningPlayer();
	UCanvasPanelSlot* S = CallBox ? Cast<UCanvasPanelSlot>(CallBox->Slot) : nullptr;
	if (!PC || !S)
	{
		return;
	}
	const FVector2D Size(CallW, 250.f);
	const float Margin = 24.f;
	FVector2D P;
	const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, CallAt, P, true);
	bool bLeftSide = false;
	if (PC->PlayerCameraManager)
	{
		const FVector ToSpeaker = CallAt - PC->PlayerCameraManager->GetCameraLocation();
		bLeftSide = FVector::DotProduct(ToSpeaker, PC->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector()) < 0.0;
	}
	if (!bOnScreen || P.X < 0.f || P.X > ViewSize.X || P.Y < 0.f || P.Y > ViewSize.Y)
	{
		P = FVector2D(bLeftSide ? Margin : ViewSize.X - Size.X - Margin, ViewSize.Y * 0.42f);
	}
	else
	{
		P += FVector2D(50.f, -20.f);
	}
	P.X = FMath::Clamp(P.X, Margin, FMath::Max(ViewSize.X - Size.X - Margin, Margin));
	P.Y = FMath::Clamp(P.Y, Size.Y + Margin, FMath::Max(ViewSize.Y * 0.7f, Size.Y + Margin));
	S->SetPosition(P - ViewSize * 0.5f);
}

void USaltTalkWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const FVector2D Size = MyGeometry.GetLocalSize();
	if (Size.X > 1.f && Size.Y > 1.f)
	{
		ViewSize = Size;
	}

	// The sheet: rises 24 px into place and fades in; the words are written out; the answers come when they are done.
	if (bLine)
	{
		LineAge += InDeltaTime;
		const float U = FMath::Clamp(LineAge / OpenTime, 0.f, 1.f);
		const float Ease = 1.f - FMath::Square(1.f - U);
		SheetBox->SetRenderOpacity(Ease);
		SheetBox->SetRenderTranslation(FVector2D(0.f, (1.f - Ease) * 24.f));
		if (WrittenChars < FullText.Len())
		{
			WriteT += InDeltaTime;
			SetWritten(FMath::FloorToInt32(WriteT * WriteRate));
		}
		else
		{
			ChoicesAge = ChoicesAge < 0.f ? 0.f : ChoicesAge + InDeltaTime;
			Choices->SetRenderOpacity(FMath::Clamp(ChoicesAge / 0.2f, 0.f, 1.f));
		}
	}
	else if (HideAge >= 0.f)
	{
		HideAge += InDeltaTime;
		const float U = FMath::Clamp(HideAge / CloseTime, 0.f, 1.f);
		SheetBox->SetRenderOpacity(1.f - U);
		SheetBox->SetRenderTranslation(FVector2D(0.f, U * 12.f));
		if (U >= 1.f)
		{
			HideAge = -1.f;
			SheetBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (CallLeft > 0.f)
	{
		CallAge += InDeltaTime;
		CallLeft -= InDeltaTime;
		CallBox->SetRenderOpacity(FMath::Min(FMath::Clamp(CallAge / CallFade, 0.f, 1.f), FMath::Clamp(CallLeft / CallFade, 0.f, 1.f)));
		PlaceCall();
		if (CallLeft <= 0.f)
		{
			CallBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FString USaltTalkWidget::GetWrittenText() const
{
	return LineWritten ? LineWritten->GetText().ToString() : FString();
}

FString USaltTalkWidget::GetCallText() const
{
	return CallLine && CallLeft > 0.f ? CallLine->GetText().ToString() : FString();
}

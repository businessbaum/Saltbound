// Saltbound - the key-scrap prompt as a widget of its own (D58).

#include "UI/SaltKeyPrompt.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/SaltMenuKit.h"
#include "UI/SaltPaintedText.h"
#include "UI/SaltUIStyle.h"

namespace
{
	// As the HUD's prompt (UI_Design.md section 7): a 58 px scrap in a 120 px frame, 18 px to the text, the text's baseline
	// 12 px under the key's centre (IM Fell English: its line's centre sits 0.271 em above the baseline).
	constexpr float PromptKeySize = 58.f;
	constexpr float KeyFrame = 120.f;
	constexpr float KeyStep = 68.f;
	constexpr float KeyGap = 18.f;
	constexpr float FadeIn = 0.15f;
}

void USaltKeyPrompt::Setup(const TArray<FString>& InKeys, const FText& InAction, float InTextSize)
{
	Keys = InKeys;
	Action = InAction;
	TextSize = InTextSize;
}

void USaltKeyPrompt::SetAction(const FText& InAction)
{
	if (!InAction.EqualTo(Action))
	{
		Action = InAction;
		Layout();
	}
}

void USaltKeyPrompt::PlayIn()
{
	Age = 0.f;
}

TSharedRef<SWidget> USaltKeyPrompt::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Row = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Row;
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		Scraps.Reset();
		UMaterialInterface* Base = SaltUI::Material(TEXT("M_UI_KeySoak"));
		for (const FString& Key : Keys)
		{
			UImage* Scrap = WidgetTree->ConstructWidget<UImage>();
			if (Base)
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
				Mid->SetScalarParameterValue(TEXT("Progress"), 0.f);
				// D66: the key the player uses now (the prompt names the default; Keys and buttons may have changed it).
				SaltMenu::SetSoakKey(Mid, SaltMenu::ShownKey(Key));
				Scrap->SetBrush(SaltUI::ImageBrush(Mid, FVector2D(KeyFrame, KeyFrame)));
			}
			if (UCanvasPanelSlot* S = Row->AddChildToCanvas(Scrap))
			{
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetSize(FVector2D(KeyFrame, KeyFrame));
			}
			Scrap->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Scraps.Add(Scrap);
		}
		Label = WidgetTree->ConstructWidget<USaltPaintedText>(USaltPaintedText::StaticClass());
		Label->Setup(ESaltFace::Fell, TextSize);
		if (UCanvasPanelSlot* S = Row->AddChildToCanvas(Label))
		{
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetAutoSize(true);
		}
		Layout();
	}
	return Super::RebuildWidget();
}

void USaltKeyPrompt::Layout()
{
	if (!Label)
	{
		return;
	}
	Label->SetText(Action);
	const float TextW = Label->MeasureWidth();
	const float KeysW = Scraps.Num() > 0 ? PromptKeySize + (Scraps.Num() - 1) * KeyStep : 0.f;
	RowWidth = KeysW + (Scraps.Num() > 0 ? KeyGap : 0.f) + TextW;
	const float X0 = -RowWidth * 0.5f;
	for (int32 I = 0; I < Scraps.Num(); ++I)
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Scraps[I]->Slot))
		{
			S->SetPosition(FVector2D(X0 + PromptKeySize * 0.5f + I * KeyStep, 0.f));
		}
	}
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Label->Slot))
	{
		S->SetPosition(FVector2D(X0 + RowWidth - TextW * 0.5f, 12.f - 0.271f * TextSize));
	}
}

void USaltKeyPrompt::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Row)
	{
		return;
	}
	Age += InDeltaTime;
	const float T = FMath::Clamp(Age / FadeIn, 0.f, 1.f);
	const float In = 1.f - FMath::Pow(1.f - T, 3.f);
	Row->SetRenderOpacity(T);
	Row->SetRenderTranslation(FVector2D(0.f, 6.f * (1.f - In)));
	for (UImage* Scrap : Scraps)
	{
		Scrap->SetRenderTransformAngle(FMath::Lerp(-6.f, -3.f, In));
	}
}

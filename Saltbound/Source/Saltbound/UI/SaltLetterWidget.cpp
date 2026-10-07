// Saltbound - the letter in the castaway's pocket (D85).

#include "UI/SaltLetterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

namespace
{
	/** The sheet at 76 % of its design size (820 x 1100), above the middle: the line under it ends above the hints row (keys at
	 *  1080p y 959-1015). */
	const FVector2D LetterSize = FVector2D(820.f, 1100.f) * 0.76f;
	constexpr float LetterCentreY = 476.f;
}

USaltLetterWidget::USaltLetterWidget()
{
	bWantsCursor = false;
	BackdropKind = EBackdrop::Blurred;
	BlurStrength = 10.f;
	bShadeLeft = false;
}

void USaltLetterWidget::Build()
{
	Page = WidgetTree->ConstructWidget<UImage>();
	Page->SetVisibility(ESlateVisibility::HitTestInvisible);
	Page->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Page->SetRenderTransformAngle(-1.2f);
	At(Contents, Page, FVector2D(960.f, LetterCentreY), LetterSize, FVector2D(0.5f, 0.5f));
	Caption = FaceText(WidgetTree, FText::GetEmpty(), ESaltFace::AlegreyaItalic, 26.f, ParchmentText, ETextJustify::Center);
	FSlateFontInfo F = Caption->GetFont();
	F.OutlineSettings.OutlineSize = 2;
	F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);
	Caption->SetFont(F);
	At(Contents, Caption, FVector2D(960.f, LetterCentreY + LetterSize.Y * 0.5f + 8.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f));
	PlaceHints({ { { TEXT("E") }, NSLOCTEXT("SaltLetter", "HintPutAway", "put it away") } }, 0.f, true);
	Apply();
}

void USaltLetterWidget::SetDry(bool bInDry)
{
	bDry = bInDry;
	Apply();
}

void USaltLetterWidget::Apply()
{
	if (!Page)
	{
		return;
	}
	Page->SetBrush(ImageBrush(Tex(bDry ? TEXT("T_UI_Letter_Dry") : TEXT("T_UI_Letter_Wet")), LetterSize));
	Caption->SetText(bDry ? FText::GetEmpty()
		: NSLOCTEXT("SaltLetter", "Wet", "The sea got into it: the ink has run and the folds stick. A fire would dry it."));
}

void USaltLetterWidget::Accept()
{
	Back();
}

bool USaltLetterWidget::RawKey(const FKey& Key, bool bRepeat)
{
	if (Key == EKeys::L && !bRepeat)
	{
		Back();
		return true;
	}
	return false;
}

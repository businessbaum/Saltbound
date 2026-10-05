// Saltbound - the landfall banner (D55, UI_Design.md section 9).

#include "UI/SaltLandfallWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Core/SaltPlaces.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Saltbound.h"
#include "UI/SaltPaintedText.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

namespace
{
	// The layout at 1080p (section 9; the pictures' frame is x 160..1760, y 120..340, Tools/textures/ui_parts.js).
	const FVector2D LandfallFrame(160.f, 120.f);
	const FVector2D LandfallFrameSize(1600.f, 220.f);
	constexpr float BandTop = 150.f;
	constexpr float BandHeight = 90.f;
	constexpr float BandArch = 18.f;
	constexpr float TailReach = 118.f;
	constexpr float NameSize = 66.f;
	constexpr float NameSpacing = 5.f;
	/** D58 (owner: "the island discovery notification is a bit too big"): the whole banner at 75 %, its band's top where the
	 *  design puts it (y 150). */
	constexpr float BannerScale = 0.75f;

	// The motion (s): unroll, seal, ink, line under it, hold, roll up.
	constexpr float UnrollTime = 0.6f;
	constexpr float SealAt = 0.5f;
	constexpr float SealTime = 0.2f;
	constexpr float InkAt = 0.6f;
	constexpr float InkTime = 0.6f;
	constexpr float LineAt = 1.2f;
	constexpr float RollUpAt = 5.6f;
	constexpr float RollUpTime = 0.5f;

	void PlaceCentred(UWidget* W, FVector2D Design, FVector2D Size)
	{
		if (UCanvasPanelSlot* S = W ? Cast<UCanvasPanelSlot>(W->Slot) : nullptr)
		{
			S->SetPosition(Design - FVector2D(960.f, 540.f));
			if (Size.X > 0.f)
			{
				S->SetSize(Size);
			}
		}
	}
}

TSharedRef<SWidget> USaltLandfallWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetVisibility(ESlateVisibility::Collapsed);
		Root->SetRenderTransformPivot(FVector2D(0.5f, BandTop / 1080.f));
		Root->SetRenderScale(FVector2D(BannerScale, BannerScale));
		WidgetTree->RootWidget = Root;
		auto Layer = [this](UMaterialInstanceDynamic*& OutMaterial)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>();
			if (UMaterialInterface* M = Material(TEXT("M_UI_Reveal")))
			{
				I->SetBrushFromMaterial(M);
				OutMaterial = I->GetDynamicMaterial();
			}
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			At(Root, I, LandfallFrame, LandfallFrameSize);
			return I;
		};
		UMaterialInstanceDynamic* BandMID = nullptr;
		UMaterialInstanceDynamic* NameMID = nullptr;
		Band = Layer(BandMID);
		NameImage = Layer(NameMID);
		BandMaterial = BandMID;
		NameMaterial = NameMID;
		Seal = WidgetTree->ConstructWidget<UImage>();
		Seal->SetBrush(ImageBrush(Tex(TEXT("T_UI_LandfallSeal")), FVector2D(120.f, 120.f)));
		Seal->SetVisibility(ESlateVisibility::HitTestInvisible);
		Seal->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		At(Root, Seal, FVector2D::ZeroVector, FVector2D(120.f, 120.f), FVector2D(0.5f, 0.5f));
		// "A new island", small caps 24 px, its baseline 18 px above the band (centred on the line: baseline - 0.271 em).
		Heading = CreateWidget<USaltPaintedText>(this, USaltPaintedText::StaticClass());
		Heading->Setup(ESaltFace::FellSC, 24.f, ETextJustify::Center);
		Heading->SetText(NSLOCTEXT("Saltbound", "LandfallNewIsland", "A new island"));
		Heading->SetVisibility(ESlateVisibility::HitTestInvisible);
		At(Root, Heading, FVector2D(960.f, BandTop - 18.f - 0.271f * 24.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
		Subtitle = CreateWidget<USaltPaintedText>(this, USaltPaintedText::StaticClass());
		Subtitle->Setup(ESaltFace::FellItalic, 28.f, ETextJustify::Center);
		Subtitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		// Its baseline 56 px below the tails' level (the band's end + 1.38 of its height).
		At(Root, Subtitle, FVector2D(960.f, BandTop + BandArch + BandHeight * 1.38f + 56.f - 0.271f * 28.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	}
	return Super::RebuildWidget();
}

void USaltLandfallWidget::Show(FName PlaceId)
{
	if (Current.IsNone())
	{
		Begin(PlaceId);
	}
	else
	{
		Queue.Add(PlaceId);
	}
}

void USaltLandfallWidget::Clear()
{
	Queue.Reset();
	HoldTime = -1.f;
	Current = NAME_None;
	Unroll = Ink = 0.f;
	if (Root)
	{
		Root->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FText USaltLandfallWidget::GetSubtitle() const
{
	return Subtitle ? Subtitle->GetText() : FText::GetEmpty();
}

bool USaltLandfallWidget::Begin(FName PlaceId)
{
	const FSaltPlace* Place = SaltPlaces::Find(PlaceId);
	const FString Id = PlaceId.ToString();
	UTexture2D* BandTex = Tex(*FString::Printf(TEXT("T_UI_Landfall_%s_Band"), *Id));
	UTexture2D* NameTex = Tex(*FString::Printf(TEXT("T_UI_Landfall_%s_Name"), *Id));
	if (!Place || !BandTex || !NameTex || !Root || !BandMaterial || !NameMaterial)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_LANDFALL no banner for %s (place or pictures missing)"), *Id);
		return false;
	}
	BandMaterial->SetTextureParameterValue(TEXT("Tex"), BandTex);
	NameMaterial->SetTextureParameterValue(TEXT("Tex"), NameTex);
	// The band is as wide as the name plus 200 px (at least 600): the name measured letter by letter, as the design does.
	NameWidth = 0.f;
	if (FSlateApplication::IsInitialized())
	{
		const FSlateFontInfo F = Font(ESaltFace::FellSC, NameSize);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FString Name = Place->Name.ToString();
		for (const TCHAR Ch : Name)
		{
			NameWidth += (float)Measure->Measure(FString(1, &Ch), F).X;
		}
		NameWidth += NameSpacing * FMath::Max(Name.Len() - 1, 0);
	}
	HalfWidth = FMath::Max(300.f, NameWidth * 0.5f + 100.f);
	// The seal pins the band's left end: 8 px inside it, 60 % down.
	PlaceCentred(Seal, FVector2D(960.f - HalfWidth + 8.f, BandTop + BandArch + BandHeight * 0.6f), FVector2D::ZeroVector);
	Subtitle->SetText(SaltPlaces::FromHome(*Place));
	Current = PlaceId;
	Time = 0.f;
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	Apply(0.f);
	return true;
}

void USaltLandfallWidget::Apply(float T)
{
	const float RollUp = FMath::Square(FMath::Clamp((T - RollUpAt) / RollUpTime, 0.f, 1.f));
	const float Open = 1.f - FMath::Cube(1.f - FMath::Clamp(T / UnrollTime, 0.f, 1.f));
	Unroll = Open * (1.f - RollUp);
	// The ribbon from its middle out to the tails' tips and their shadows (as a fraction of the frame's half-width).
	const float Extent = (HalfWidth + TailReach + 30.f) / (LandfallFrameSize.X * 0.5f);
	BandMaterial->SetScalarParameterValue(TEXT("Half"), Unroll * (Extent + 0.04f));
	NameMaterial->SetScalarParameterValue(TEXT("Half"), Unroll * (Extent + 0.04f));
	// The name inks in from its left end to its right.
	Ink = FMath::Clamp((T - InkAt) / InkTime, 0.f, 1.f);
	const float From = (960.f - NameWidth * 0.5f - 14.f - LandfallFrame.X) / LandfallFrameSize.X;
	const float To = (960.f + NameWidth * 0.5f + 14.f - LandfallFrame.X) / LandfallFrameSize.X;
	NameMaterial->SetScalarParameterValue(TEXT("Front"), T < InkAt ? -0.1f : FMath::Lerp(From, To, Ink) + (Ink >= 1.f ? 1.f : 0.f));
	// The seal stamps down: 1.4 to 1 with a slight overshoot.
	const float S = FMath::Clamp((T - SealAt) / SealTime, 0.f, 1.f);
	const float Scale = 1.f + 0.4f * FMath::Cos(S * PI * 1.25f) * FMath::Pow(1.f - S, 1.5f);
	Seal->SetRenderScale(FVector2D(Scale, Scale));
	Seal->SetRenderOpacity(T < SealAt ? 0.f : FMath::Clamp(S / 0.3f, 0.f, 1.f) * (1.f - RollUp));
	Heading->SetRenderOpacity(FMath::Clamp(T / 0.4f, 0.f, 1.f) * (1.f - RollUp));
	Subtitle->SetRenderOpacity(FMath::Clamp((T - LineAt) / 0.4f, 0.f, 1.f) * (1.f - RollUp));
}

void USaltLandfallWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Current.IsNone())
	{
		return;
	}
	Time = HoldTime >= 0.f ? HoldTime : Time + InDeltaTime;
	Apply(Time);
	if (Time >= Duration)
	{
		// Done: hide it, then the next one waiting (if any).
		const TArray<FName> Waiting = MoveTemp(Queue);
		Clear();
		Queue = Waiting;
		while (!Queue.IsEmpty() && Current.IsNone())
		{
			const FName Next = Queue[0];
			Queue.RemoveAt(0);
			Begin(Next);
		}
	}
}

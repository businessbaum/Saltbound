// Saltbound - the menus' shared pieces (D66).

#include "UI/SaltMenuKit.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/SaltPlaces.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/FontMeasure.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Player/SaltInputConfig.h"
#include "Rendering/SlateRenderer.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltPaintedText.h"

// --- Pieces ------------------------------------------------------------------------------------------------------------

UTexture2D* SaltMenu::Gradient(FName Key, bool bHorizontal, const TArray<FStop>& Stops)
{
	static TMap<FName, TObjectPtr<UTexture2D>> Cache;
	if (TObjectPtr<UTexture2D>* Found = Cache.Find(Key))
	{
		if (*Found)
		{
			return *Found;
		}
	}
	constexpr int32 N = 256;
	UTexture2D* T = UTexture2D::CreateTransient(bHorizontal ? N : 1, bHorizontal ? 1 : N, PF_B8G8R8A8, *FString::Printf(TEXT("T_UI_Gradient_%s"), *Key.ToString()));
	if (!T || Stops.Num() == 0)
	{
		return nullptr;
	}
	T->SRGB = true;
	T->Filter = TF_Bilinear;
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		const float U = (I + 0.5f) / N;
		int32 K = 0;
		while (K + 1 < Stops.Num() && Stops[K + 1].At <= U)
		{
			++K;
		}
		const FStop& A = Stops[K];
		const FStop& B = Stops[FMath::Min(K + 1, Stops.Num() - 1)];
		const float F = B.At > A.At ? FMath::Clamp((U - A.At) / (B.At - A.At), 0.f, 1.f) : 0.f;
		// As CSS: the stops' sRGB values mixed directly.
		Pixels[I] = FColor(
			(uint8)FMath::RoundToInt(FMath::Lerp((float)A.R, (float)B.R, F)),
			(uint8)FMath::RoundToInt(FMath::Lerp((float)A.G, (float)B.G, F)),
			(uint8)FMath::RoundToInt(FMath::Lerp((float)A.B, (float)B.B, F)),
			(uint8)FMath::RoundToInt(255.f * FMath::Lerp(A.A, B.A, F)));
	}
	FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), N * sizeof(FColor));
	Mip.BulkData.Unlock();
	T->UpdateResource();
	T->AddToRoot();
	Cache.Add(Key, T);
	return T;
}

UImage* SaltMenu::FullScreen(UWidgetTree* Tree, UCanvasPanel* Canvas, UObject* Texture, const FLinearColor& Tint)
{
	UImage* Img = Tree->ConstructWidget<UImage>();
	FSlateBrush B = SaltUI::ImageBrush(Texture, FVector2D(16.f, 16.f));
	B.TintColor = FSlateColor(Tint);
	Img->SetBrush(B);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Img);
	S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	S->SetOffsets(FMargin(0.f));
	return Img;
}

UImage* SaltMenu::FullScreenColor(UWidgetTree* Tree, UCanvasPanel* Canvas, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>();
	Img->SetColorAndOpacity(Color);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Img);
	S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	S->SetOffsets(FMargin(0.f));
	return Img;
}

FSlateBrush SaltMenu::RopeBrush(float Height)
{
	FSlateBrush B = SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_MenuRope")), FVector2D(420.f * Height / 30.f, Height));
	B.Tiling = ESlateBrushTileType::Horizontal;
	return B;
}

UImage* SaltMenu::Rope(UWidgetTree* Tree, float Height)
{
	UImage* Img = Tree->ConstructWidget<UImage>();
	Img->SetBrush(RopeBrush(Height));
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Img;
}

UImage* SaltMenu::Knot(UWidgetTree* Tree, float Width)
{
	UImage* Img = Tree->ConstructWidget<UImage>();
	Img->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_MenuKnot")), FVector2D(Width, Width * 0.5f)));
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Img;
}

UImage* SaltMenu::Anchor(UWidgetTree* Tree, float Size, bool bNext)
{
	UImage* Img = Tree->ConstructWidget<UImage>();
	Img->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_MenuAnchor")), FVector2D(Size, Size)));
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	// Crown down in the picture: a quarter turn puts it at the side the arrow points to.
	Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Img->SetRenderTransformAngle(bNext ? -90.f : 90.f);
	return Img;
}

UImage* SaltMenu::Pointer(UWidgetTree* Tree, float Size)
{
	UImage* Img = Tree->ConstructWidget<UImage>();
	Img->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_SaveKnot_Rose")), FVector2D(Size, Size)));
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Img->SetRenderTransformAngle(90.f);
	return Img;
}

USaltPaintedText* SaltMenu::Paint(UWidgetTree* Tree, const FText& Text, ESaltFace Face, float Size, ETextJustify::Type Justify, float LetterSpacing)
{
	USaltPaintedText* P = Tree->ConstructWidget<USaltPaintedText>(USaltPaintedText::StaticClass());
	P->Setup(Face, Size, Justify, LetterSpacing);
	P->SetText(Text);
	P->SetVisibility(ESlateVisibility::HitTestInvisible);
	return P;
}

FString SaltMenu::KeyName(const FKey& Key)
{
	static const TMap<FKey, FString> Short = {
		{ EKeys::LeftShift, TEXT("Shift") }, { EKeys::RightShift, TEXT("RShift") }, { EKeys::LeftControl, TEXT("Ctrl") },
		{ EKeys::RightControl, TEXT("RCtrl") }, { EKeys::LeftAlt, TEXT("Alt") }, { EKeys::RightAlt, TEXT("AltGr") },
		{ EKeys::SpaceBar, TEXT("Space") }, { EKeys::Escape, TEXT("Esc") }, { EKeys::Enter, TEXT("Enter") }, { EKeys::Tab, TEXT("Tab") },
		{ EKeys::BackSpace, TEXT("Back") }, { EKeys::CapsLock, TEXT("Caps") }, { EKeys::Up, TEXT("Up") }, { EKeys::Down, TEXT("Down") },
		{ EKeys::Left, TEXT("Left") }, { EKeys::Right, TEXT("Right") }, { EKeys::Insert, TEXT("Ins") }, { EKeys::Delete, TEXT("Del") },
		{ EKeys::Home, TEXT("Home") }, { EKeys::End, TEXT("End") }, { EKeys::PageUp, TEXT("PgUp") }, { EKeys::PageDown, TEXT("PgDn") },
		{ EKeys::Zero, TEXT("0") }, { EKeys::One, TEXT("1") }, { EKeys::Two, TEXT("2") }, { EKeys::Three, TEXT("3") }, { EKeys::Four, TEXT("4") },
		{ EKeys::Five, TEXT("5") }, { EKeys::Six, TEXT("6") }, { EKeys::Seven, TEXT("7") }, { EKeys::Eight, TEXT("8") }, { EKeys::Nine, TEXT("9") },
		{ EKeys::Semicolon, TEXT(";") }, { EKeys::Comma, TEXT(",") }, { EKeys::Period, TEXT(".") }, { EKeys::Slash, TEXT("/") },
		{ EKeys::Hyphen, TEXT("-") }, { EKeys::Equals, TEXT("=") }, { EKeys::LeftBracket, TEXT("[") }, { EKeys::RightBracket, TEXT("]") },
		{ EKeys::Apostrophe, TEXT("'") }, { EKeys::Backslash, TEXT("\\") }, { EKeys::Tilde, TEXT("`") },
	};
	if (const FString* S = Short.Find(Key))
	{
		return *S;
	}
	FString Name = Key.GetDisplayName(false).ToString();
	return Name.Len() > 6 ? Name.Left(6) : Name;
}

float SaltMenu::KeyScrapWidth(const FString& Label, float Size)
{
	return Label.Len() > 1 ? Size * 2.f : Size;
}

UWidget* SaltMenu::KeyScrap(UWidgetTree* Tree, const FString& Label, float Size)
{
	const bool bWide = Label.Len() > 1;
	const float K = Size / 58.f;
	const FVector2D Scrap(KeyScrapWidth(Label, Size), Size);
	// The pictures: a 58 px scrap in a 120 px frame, the wide one 116 x 58 in 180 x 120 (Tools/textures/menu_parts.js).
	const FVector2D Frame = bWide ? FVector2D(180.f, 120.f) * K : FVector2D(120.f, 120.f) * K;
	USizeBox* Box = Tree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Scrap.X);
	Box->SetHeightOverride(Scrap.Y);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* C = Tree->ConstructWidget<UCanvasPanel>();
	Box->AddChild(C);
	UImage* Img = Tree->ConstructWidget<UImage>();
	Img->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(bWide ? TEXT("T_UI_MenuKeyWide") : TEXT("T_UI_KeyScrap")), Frame));
	UCanvasPanelSlot* S = C->AddChildToCanvas(Img);
	S->SetAnchors(FAnchors(0.5f, 0.5f));
	S->SetAlignment(FVector2D(0.5f, 0.5f));
	S->SetSize(Frame);
	// The letter in IM Fell English SC, inked (32 px on a 58 px scrap; words a little smaller).
	UTextBlock* T = SaltUI::InkText(Tree, FText::FromString(Label), ESaltFace::FellSC, (bWide ? 25.f : 32.f) * K, ETextJustify::Center);
	UCanvasPanelSlot* TS = C->AddChildToCanvas(T);
	TS->SetAnchors(FAnchors(0.5f, 0.5f));
	TS->SetAlignment(FVector2D(0.5f, 0.5f));
	TS->SetAutoSize(true);
	TS->SetPosition(FVector2D(0.f, 2.f * K));
	C->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	C->SetRenderTransformAngle(-3.f);
	return Box;
}

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarDrawnKeyLetters(TEXT("sb.Dev.DrawnKeyLetters"), 0,
		TEXT("sb.Dev.DrawnKeyLetters 1: draw every key letter at run time, also where a painted one exists (to compare them)."), ECVF_Cheat);
#endif

	/** A key's letter drawn into a render target: IM Fell English SC, 60 px (the painted letters' 32 px at 2x look this big once
	 *  their ink has bled), centred, its
	 *  baseline 22 px under the middle; ink (rgba 48/30/16 at 94 %, a faint bleed) or the light twin (#EFDFB8). The target is
	 *  cleared to the letter's own colour at no opacity, so its edges keep their colour where they fade. */
	UTexture* DrawKeyLetter(const FString& Label, bool bLight)
	{
		if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender())
		{
			return nullptr;
		}
		constexpr int32 Px = 240;
		const FLinearColor Colour = bLight ? SaltUI::SRGB(0xEF, 0xDF, 0xB8) : SaltUI::SRGB(48, 30, 16, 0.94f);
		float Size = 60.f;
		FSlateFontInfo Font = SaltUI::Font(ESaltFace::FellSC, Size);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const float Width = (float)Measure->Measure(Label, Font).X;
		// Words fit the 58 px scrap (96 px of the 116 at 2x).
		if (Width > 96.f)
		{
			Size = FMath::Max(Size * 96.f / Width, 28.f);
			Font = SaltUI::Font(ESaltFace::FellSC, Size);
		}
		if (!bLight)
		{
			Font.OutlineSettings.OutlineSize = 3;
			Font.OutlineSettings.OutlineColor = SaltUI::SRGB(48, 30, 16, 0.22f);
		}
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		Target->ClearColor = FLinearColor(Colour.R, Colour.G, Colour.B, 0.f);
		Target->InitCustomFormat(Px, Px, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
		// IM Fell English's ascent is 0.905 em: the text's top so its baseline lands 22 px under the middle.
		const float Top = Px * 0.5f + 22.f - 0.905f * Size;
		TSharedRef<SWidget> Letter = SNew(SBox)
			.WidthOverride((float)Px)
			.HeightOverride((float)Px)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, Top, 0.f, 0.f))
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(Font).ColorAndOpacity(FSlateColor(Colour))
			];
		// The target is sRGB, so Slate writes linear colour (no gamma of its own) and the target keeps it as the pictures have it.
		FWidgetRenderer* Renderer = new FWidgetRenderer(false, false);
		Renderer->DrawWidget(Target, Letter, FVector2D(Px, Px), 0.f);
		BeginCleanup(Renderer);
		return Target;
	}
}

FString SaltMenu::ShownKey(const FString& DefaultLabel)
{
	for (const FSaltRebindable& R : USaltInputConfig::Rebindables())
	{
		if (KeyName(R.Default) == DefaultLabel)
		{
			return KeyName(USaltInputConfig::KeyOf(R.Id));
		}
	}
	return DefaultLabel;
}

bool SaltMenu::IsActionKey(const FKey& Pressed, FName ActionId)
{
	const FSaltRebindable* R = USaltInputConfig::FindRebindable(ActionId);
	return R && (Pressed == R->Default || Pressed == USaltInputConfig::KeyOf(ActionId));
}

UTexture* SaltMenu::KeyLetter(const FString& Label, bool bLight)
{
	// Weak: the painted letters are assets and may be unloaded (they are found again); the drawn ones are rooted.
	static TMap<FString, TWeakObjectPtr<UTexture>> Cache;
#if !UE_BUILD_SHIPPING
	const bool bDrawAll = CVarDrawnKeyLetters.GetValueOnGameThread() != 0;
#else
	const bool bDrawAll = false;
#endif
	const FString CacheKey = Label + (bLight ? TEXT("|light") : TEXT("|ink")) + (bDrawAll ? TEXT("|drawn") : TEXT(""));
	if (const TWeakObjectPtr<UTexture>* Found = Cache.Find(CacheKey))
	{
		if (UTexture* T = Found->Get())
		{
			return T;
		}
	}
	UTexture* T = nullptr;
	// The painted letters keep the approved look (Tools/textures/ui_parts.js, menu_parts.js).
	bool bName = !Label.IsEmpty() && !bDrawAll;
	for (const TCHAR C : Label)
	{
		bName = bName && FChar::IsAlnum(C);
	}
	if (bName)
	{
		const FString Name = bLight ? FString::Printf(TEXT("T_UI_KeyLetterLight_%s"), *Label) : FString::Printf(TEXT("T_UI_KeyLetter_%s"), *Label);
		if (FPackageName::DoesPackageExist(FString(TEXT("/Game/Saltbound/UI/Textures/")) + Name))
		{
			T = SaltUI::Tex(*Name);
		}
	}
	if (!T)
	{
		T = DrawKeyLetter(Label, bLight);
		if (T)
		{
			T->AddToRoot();
		}
	}
	if (T)
	{
		Cache.Add(CacheKey, T);
	}
	return T;
}

void SaltMenu::SetSoakKey(UMaterialInstanceDynamic* Mid, const FString& Label)
{
	if (!Mid)
	{
		return;
	}
	if (UTexture* L = KeyLetter(Label, false))
	{
		Mid->SetTextureParameterValue(TEXT("Letter"), L);
	}
	if (UTexture* L = KeyLetter(Label, true))
	{
		Mid->SetTextureParameterValue(TEXT("LetterLight"), L);
	}
}

UWidget* SaltMenu::SoakKey(UWidgetTree* Tree, UObject* Outer, const FString& Letter, float Size, UMaterialInstanceDynamic*& OutMid)
{
	const float Frame = 120.f * Size / 58.f;
	USizeBox* Box = Tree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* C = Tree->ConstructWidget<UCanvasPanel>();
	Box->AddChild(C);
	UImage* Img = Tree->ConstructWidget<UImage>();
	OutMid = nullptr;
	if (UMaterialInterface* Base = SaltUI::Material(TEXT("M_UI_KeySoak")))
	{
		OutMid = UMaterialInstanceDynamic::Create(Base, Outer);
		OutMid->SetScalarParameterValue(TEXT("Progress"), 0.f);
		SetSoakKey(OutMid, Letter);
		Img->SetBrush(SaltUI::ImageBrush(OutMid, FVector2D(Frame, Frame)));
	}
	UCanvasPanelSlot* S = C->AddChildToCanvas(Img);
	S->SetAnchors(FAnchors(0.5f, 0.5f));
	S->SetAlignment(FVector2D(0.5f, 0.5f));
	S->SetSize(FVector2D(Frame, Frame));
	C->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	C->SetRenderTransformAngle(-3.f);
	return Box;
}

UHorizontalBox* SaltMenu::Hints(UWidgetTree* Tree, const TArray<FHint>& InHints, float KeySize, float TextSize)
{
	UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (int32 H = 0; H < InHints.Num(); ++H)
	{
		for (int32 K = 0; K < InHints[H].Keys.Num(); ++K)
		{
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(KeyScrap(Tree, InHints[H].Keys[K], KeySize));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(K == 0 ? (H == 0 ? 0.f : 46.f) : 12.f, 0.f, 0.f, 0.f));
		}
		USaltPaintedText* L = Paint(Tree, InHints[H].Label, ESaltFace::Fell, TextSize);
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}
	return Row;
}

UHorizontalBox* SaltMenu::KeycapHints(UWidgetTree* Tree, const TArray<FHint>& InHints, float KeySize, float TextSize)
{
	UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (int32 H = 0; H < InHints.Num(); ++H)
	{
		for (int32 K = 0; K < InHints[H].Keys.Num(); ++K)
		{
			const FString& Key = InHints[H].Keys[K];
			UWidget* W = nullptr;
			if (Key == TEXT("-"))
			{
				USaltPaintedText* Dash = Paint(Tree, FText::FromString(TEXT("\u2013")), ESaltFace::Alegreya, TextSize * 0.8f);
				Dash->SetPlainColor(SaltUI::SRGB(0xE3, 0xD3, 0xB2, 0.5f));
				W = Dash;
			}
			else
			{
				W = SaltUI::Keycap(Tree, FText::FromString(Key), KeySize);
			}
			UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(W);
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(K == 0 ? (H == 0 ? 0.f : 56.f) : 8.f, 0.f, 0.f, 0.f));
		}
		USaltPaintedText* L = Paint(Tree, InHints[H].Label, ESaltFace::Alegreya, TextSize);
		L->SetPlainColor(SaltUI::SRGB(0xE3, 0xD3, 0xB2));
		UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
	}
	return Row;
}

FText SaltMenu::PlayTime(double Seconds)
{
	const int32 Minutes = FMath::FloorToInt(Seconds / 60.0);
	if (Minutes < 1)
	{
		return NSLOCTEXT("SaltMenu", "UnderAMinute", "under a minute");
	}
	if (Minutes < 60)
	{
		return FText::Format(NSLOCTEXT("SaltMenu", "Minutes", "{0} min"), FText::AsNumber(Minutes));
	}
	return FText::Format(NSLOCTEXT("SaltMenu", "HoursMinutes", "{0} h {1} min"), FText::AsNumber(Minutes / 60), FText::AsNumber(Minutes % 60));
}

FText SaltMenu::Ago(const FDateTime& InWhen)
{
	if (InWhen.GetTicks() == 0)
	{
		return NSLOCTEXT("SaltMenu", "Never", "not yet");
	}
	const FTimespan Span = FDateTime::Now() - InWhen;
	const int32 Minutes = FMath::FloorToInt(Span.GetTotalMinutes());
	if (Minutes < 1)
	{
		return NSLOCTEXT("SaltMenu", "JustNow", "just now");
	}
	if (Minutes < 60)
	{
		return Minutes == 1 ? NSLOCTEXT("SaltMenu", "AMinuteAgo", "a minute ago")
			: FText::Format(NSLOCTEXT("SaltMenu", "MinutesAgo", "{0} minutes ago"), FText::AsNumber(Minutes));
	}
	const int32 Hours = Minutes / 60;
	if (Hours < 24)
	{
		return Hours == 1 ? NSLOCTEXT("SaltMenu", "AnHourAgo", "an hour ago")
			: FText::Format(NSLOCTEXT("SaltMenu", "HoursAgo", "{0} hours ago"), FText::AsNumber(Hours));
	}
	return FText::Format(NSLOCTEXT("SaltMenu", "OnDay", "on {0}"), DayMonth(InWhen));
}

FText SaltMenu::DayMonth(const FDateTime& InWhen)
{
	// FDateTime::ToString has no month names: "4 Oct".
	static const TCHAR* Months[] = { TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"), TEXT("Sep"),
		TEXT("Oct"), TEXT("Nov"), TEXT("Dec") };
	return FText::FromString(FString::Printf(TEXT("%d %s"), InWhen.GetDay(), Months[FMath::Clamp(InWhen.GetMonth(), 1, 12) - 1]));
}

FText SaltMenu::When(const FDateTime& InWhen)
{
	return FText::Format(NSLOCTEXT("SaltMenu", "When", "{0}, {1}"), DayMonth(InWhen), FText::FromString(InWhen.ToString(TEXT("%H:%M"))));
}

FText SaltMenu::Roman(int32 Number)
{
	static const TCHAR* R[] = { TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI") };
	return FText::FromString(Number >= 1 && Number <= 6 ? R[Number - 1] : FString::FromInt(Number));
}

FString SaltMenu::CompassPoint(float BearingDeg)
{
	static const TCHAR* Points[] = {
		TEXT("north"), TEXT("north-north-east"), TEXT("north-east"), TEXT("east-north-east"), TEXT("east"), TEXT("east-south-east"),
		TEXT("south-east"), TEXT("south-south-east"), TEXT("south"), TEXT("south-south-west"), TEXT("south-west"), TEXT("west-south-west"),
		TEXT("west"), TEXT("west-north-west"), TEXT("north-west"), TEXT("north-north-west"),
	};
	const float B = FMath::Fmod(FMath::Fmod(BearingDeg, 360.f) + 360.f, 360.f);
	return Points[FMath::RoundToInt(B / 22.5f) % 16];
}

FText SaltMenu::VoyageLine(const FSaltVoyageInfo& Info)
{
	if (!Info.bNearPlace)
	{
		return FText::Format(NSLOCTEXT("SaltMenu", "LineSea", "{0} on the open sea"), Info.Boat);
	}
	const bool bLying = Info.Hold == ESaltBoatHold::Moored || Info.Hold == ESaltBoatHold::Anchored;
	return FText::Format(bLying ? NSLOCTEXT("SaltMenu", "LineOff", "{0} off {1}") : NSLOCTEXT("SaltMenu", "LineNear", "{0} near {1}"), Info.Boat, Info.Place);
}

const FSaltPlace* SaltMenu::PlaceNear(const FVector& WorldCm)
{
	const FVector2D M(WorldCm.X * 0.01, WorldCm.Y * 0.01);
	const FSaltPlace* Best = nullptr;
	float BestD = 1500.f;
	for (const FSaltPlace& P : SaltPlaces::All())
	{
		const float D = SaltPlaces::DistanceToShoreM(P, M);
		if (D < BestD)
		{
			BestD = D;
			Best = &P;
		}
	}
	return Best;
}

// --- Menu line ---------------------------------------------------------------------------------------------------------

namespace
{
	// The design's line (Menus.html .mi): the sub-line 7 px under the label in IM Fell English italic at 0.44 of the label,
	// line 1.2; the rope 1cqw (19 px) high, its bottom 1.15cqw (22 px) under the box; unchosen labels at 72 %.
	constexpr float MenuSubGap = 7.f;
	constexpr float MenuSubScale = 0.44f;
	constexpr float MenuRopeHeight = 19.f;
	constexpr float MenuRopeDrop = 22.f;
	constexpr float MenuChooseTime = 0.35f;
	/** The rose's picture against the rose (T_UI_SaveKnot_Rose: 124 of 256 px), a little under 2 so it reads at menu size. */
	constexpr float MenuRoseFrame = 1.7f;

	/** CSS cubic-bezier(.34, 1.56, .64, 1), the pointer's spring: y for x, by bisection on x. */
	float BackOut(float X)
	{
		auto Bez = [](float T, float P1, float P2) { const float U = 1.f - T; return 3.f * U * U * T * P1 + 3.f * U * T * T * P2 + T * T * T; };
		float Lo = 0.f, Hi = 1.f;
		for (int32 I = 0; I < 18; ++I)
		{
			const float Mid = 0.5f * (Lo + Hi);
			(Bez(Mid, 0.34f, 0.64f) < X ? Lo : Hi) = Mid;
		}
		return Bez(0.5f * (Lo + Hi), 1.56f, 1.f);
	}
}

void USaltMenuItem::Setup(const FText& InLabel, float InSize, float InPointer, float InIndent)
{
	Label = InLabel;
	Size = InSize;
	PointerSize = InPointer;
	Indent = InIndent;
}

void USaltMenuItem::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	if (LabelText)
	{
		LabelText->SetText(Label);
		Layout();
	}
}

void USaltMenuItem::SetSub(const FText& InSub)
{
	Sub = InSub;
	if (SubText)
	{
		SubText->SetText(Sub);
		SubText->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Layout();
	}
}

void USaltMenuItem::SetChosen(bool bInChosen, bool bInstant)
{
	bChosen = bInChosen;
	if (bInstant)
	{
		Progress = bChosen ? 1.f : 0.f;
		ApplyLook();
	}
}

void USaltMenuItem::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	ApplyLook();
}

float USaltMenuItem::GetBoxHeight() const
{
	return Size + (Sub.IsEmpty() ? 0.f : MenuSubGap + Size * MenuSubScale * 1.2f);
}

float USaltMenuItem::GetBoxWidth() const
{
	float W = LabelText ? LabelText->MeasureWidth() : Size * 4.f;
	if (SubText && !Sub.IsEmpty() && FSlateApplication::IsInitialized())
	{
		SubText->ForceLayoutPrepass();
		W = FMath::Max(W, (float)SubText->GetDesiredSize().X);
	}
	return Indent + W;
}

TSharedRef<SWidget> USaltMenuItem::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		SetVisibility(ESlateVisibility::HitTestInvisible);

		Underline = SaltMenu::Rope(WidgetTree, MenuRopeHeight);
		Root->AddChildToCanvas(Underline);
		PointerImage = SaltMenu::Pointer(WidgetTree, PointerSize);
		Root->AddChildToCanvas(PointerImage);
		LabelText = SaltMenu::Paint(WidgetTree, Label, ESaltFace::Fell, Size);
		Root->AddChildToCanvas(LabelText);
		SubText = SaltUI::FaceText(WidgetTree, Sub, ESaltFace::FellItalic, FMath::RoundToFloat(Size * MenuSubScale), SaltMenu::SubGrey());
		SubText->SetShadowOffset(FVector2D(0.f, 2.f));
		SubText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		SubText->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Root->AddChildToCanvas(SubText);
		Layout();
		ApplyLook();
	}
	return Super::RebuildWidget();
}

void USaltMenuItem::Layout()
{
	if (!Root)
	{
		return;
	}
	const float W = LabelText->MeasureWidth();
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(LabelText->Slot))
	{
		// Centred on the CSS line box (the outlines grow the text block evenly on every side).
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(Indent + W * 0.5f, Size * 0.5f));
	}
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(PointerImage->Slot))
	{
		// The rose in its box at the left, 0.15cqw above the line's top. The rose fills about half of its picture (the save
		// sign's ring goes round it), so the picture is drawn larger round the same middle.
		const float Frame = PointerSize * MenuRoseFrame;
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(Frame, Frame));
		S->SetPosition(FVector2D(PointerSize * 0.5f, PointerSize * 0.5f - Size * 0.06f));
	}
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(SubText->Slot))
	{
		const float SubSize = FMath::RoundToFloat(Size * MenuSubScale);
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(Indent, SaltMenu::TextTop(Size + MenuSubGap, SubSize, 1.2f)));
	}
	ApplyLook();
}

void USaltMenuItem::ApplyLook()
{
	if (!Root)
	{
		return;
	}
	const float P = FMath::Clamp(Progress, 0.f, 1.f);
	LabelText->SetRenderOpacity(!bEnabled ? 0.35f : FMath::Lerp(UnchosenOpacity, 1.f, P));
	SubText->SetRenderOpacity(bEnabled ? 1.f : 0.6f);
	// The rose: fades in over 0.2 s and springs from 70 % to full size.
	PointerImage->SetRenderOpacity(FMath::Clamp(P * MenuChooseTime / 0.2f, 0.f, 1.f));
	PointerImage->SetRenderScale(FVector2D(0.7f + 0.3f * BackOut(P)));
	// The rope grows under the label (ease-out).
	const float Grow = 1.f - FMath::Pow(1.f - P, 2.2f);
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Underline->Slot))
	{
		const float Full = GetBoxWidth() - Indent;
		S->SetPosition(FVector2D(Indent, GetBoxHeight() + MenuRopeDrop - MenuRopeHeight));
		S->SetSize(FVector2D(FMath::Max(Full * Grow, 0.f), MenuRopeHeight));
	}
	Underline->SetRenderOpacity(bShowRope && P > 0.001f ? 1.f : 0.f);
}

void USaltMenuItem::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Target = bChosen ? 1.f : 0.f;
	if (Progress != Target)
	{
		// Real time: the menus run while the game is paused.
		Progress = FMath::FInterpConstantTo(Progress, Target, InDeltaTime, 1.f / MenuChooseTime);
		ApplyLook();
	}
}

// --- Screen ------------------------------------------------------------------------------------------------------------

ASaltPlayerController* USaltScreen::GetSaltController() const
{
	return Cast<ASaltPlayerController>(GetOwningPlayer());
}

void USaltScreen::Back()
{
	if (ASaltPlayerController* PC = GetSaltController())
	{
		PC->PopScreen(this);
	}
}

TSharedRef<SWidget> USaltScreen::RebuildWidget()
{
	SetIsFocusable(true);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		Backdrop = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Backdrop"));
		UCanvasPanelSlot* BS = Root->AddChildToCanvas(Backdrop);
		BS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BS->SetOffsets(FMargin(0.f));
		BuildBackdrop();
		Contents = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Contents"));
		UCanvasPanelSlot* CS = Root->AddChildToCanvas(Contents);
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		Build();
		for (int32 I = 0; I < Items.Num(); ++I)
		{
			Items[I]->SetChosen(I == Chosen, true);
		}
	}
	return Super::RebuildWidget();
}

void USaltScreen::BuildBackdrop()
{
	using SaltMenu::FStop;
	switch (BackdropKind)
	{
	case EBackdrop::Cover:
		return;
	case EBackdrop::Title:
		// Night sea (Menus.html #mmB): the world at dusk, shaded blue from above and dark from the left.
		SaltMenu::FullScreenColor(WidgetTree, Backdrop, SaltUI::SRGB(4, 9, 14, 0.38f));
		SaltMenu::FullScreen(WidgetTree, Backdrop, SaltMenu::Gradient(TEXT("TitleBlue"), false,
			{ { 0.f, 18, 40, 58, 0.55f }, { 0.5f, 5, 10, 14, 0.2f }, { 1.f, 5, 10, 14, 0.75f } }));
		SaltMenu::FullScreen(WidgetTree, Backdrop, SaltMenu::Gradient(TEXT("ShadeLeft"), true,
			{ { 0.f, 4, 9, 13, 0.92f }, { 0.32f, 4, 9, 13, 0.7f }, { 0.62f, 4, 9, 13, 0.f }, { 1.f, 4, 9, 13, 0.f } }));
		return;
	case EBackdrop::Dim:
	case EBackdrop::Blurred:
		break;
	}
	Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	Blur->SetBlurStrength(0.f);
	Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* S = Backdrop->AddChildToCanvas(Blur);
	S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	S->SetOffsets(FMargin(0.f));
	if (BackdropKind == EBackdrop::Dim)
	{
		// A question: the world darkened to about 40 % and softly blurred, a vignette round it.
		SaltMenu::FullScreenColor(WidgetTree, Backdrop, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		SaltMenu::FullScreen(WidgetTree, Backdrop, SaltUI::Tex(TEXT("T_UI_Vignette")), FLinearColor(1.f, 1.f, 1.f, 0.8f));
		return;
	}
	// The blurred world at about 38 % (the design's brightness), then the shade.
	SaltMenu::FullScreenColor(WidgetTree, Backdrop, SaltUI::SRGB(4, 9, 13, 0.5f));
	if (bShadeLeft)
	{
		SaltMenu::FullScreen(WidgetTree, Backdrop, SaltMenu::Gradient(TEXT("ShadeLeft"), true,
			{ { 0.f, 4, 9, 13, 0.92f }, { 0.32f, 4, 9, 13, 0.7f }, { 0.62f, 4, 9, 13, 0.f }, { 1.f, 4, 9, 13, 0.f } }));
	}
	else
	{
		SaltMenu::FullScreen(WidgetTree, Backdrop, SaltMenu::Gradient(TEXT("ShadeEven"), true,
			{ { 0.f, 4, 9, 13, 0.9f }, { 0.6f, 4, 9, 13, 0.72f }, { 1.f, 4, 9, 13, 0.82f } }));
	}
}

void USaltScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	OpenTime = 0.f;
	// The title's shades are over the world from the start (the intro hands over to it).
	SaltUI::ApplyOpening(Contents, BackdropKind == EBackdrop::Title ? nullptr : Backdrop.Get(), Blur, BlurStrength, 0.f);
}

void USaltScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (OpenTime < OpenDuration)
	{
		OpenTime = FMath::Min(OpenTime + InDeltaTime, OpenDuration);
		// The title's backdrop is the world itself: no fade.
		SaltUI::ApplyOpening(Contents, BackdropKind == EBackdrop::Title ? nullptr : Backdrop.Get(), Blur, BlurStrength, OpenTime / OpenDuration);
	}
}

USaltMenuItem* USaltScreen::AddItem(const FText& Label, FVector2D Design, float Size, float Pointer, float Indent)
{
	USaltMenuItem* Item = WidgetTree->ConstructWidget<USaltMenuItem>(USaltMenuItem::StaticClass());
	Item->Setup(Label, Size, Pointer, Indent);
	SaltUI::At(Contents, Item, Design, FVector2D(10.f, 10.f));
	Items.Add(Item);
	ItemPos.Add(Design);
	return Item;
}

void USaltScreen::StackItems(FVector2D TopLeft, float Gap)
{
	float Y = TopLeft.Y;
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		if (Items[I]->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		ItemPos[I] = FVector2D(TopLeft.X, Y);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Items[I]->Slot))
		{
			S->SetPosition(ItemPos[I] - FVector2D(960.f, 540.f));
		}
		Y += Items[I]->GetBoxHeight() + Gap;
	}
}

int32 USaltScreen::ItemAt(const FVector2D& Design) const
{
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		if (Items[I]->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		const FVector2D P = Design - ItemPos[I];
		if (P.X >= -10.f && P.X <= Items[I]->GetBoxWidth() + 30.f && P.Y >= -12.f && P.Y <= Items[I]->GetBoxHeight() + 12.f)
		{
			return I;
		}
	}
	return INDEX_NONE;
}

void USaltScreen::ChooseItem(int32 Index, bool bInstant)
{
	if (!Items.IsValidIndex(Index))
	{
		return;
	}
	Chosen = Index;
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		Items[I]->SetChosen(I == Chosen, bInstant);
	}
}

void USaltScreen::UseItem(int32 Index)
{
	if (Items.IsValidIndex(Index) && Items[Index]->IsItemEnabled() && Items[Index]->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ChooseItem(Index);
		ItemUsed(Index);
	}
}

void USaltScreen::Navigate(int32 Dx, int32 Dy)
{
	if (Dy == 0 || Items.Num() == 0)
	{
		return;
	}
	// The next line that shows (disabled lines can be chosen; they say why they are greyed).
	int32 I = Chosen;
	for (int32 Step = 0; Step < Items.Num(); ++Step)
	{
		I = (I + Dy + Items.Num()) % Items.Num();
		if (Items[I]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			ChooseItem(I);
			return;
		}
	}
}

void USaltScreen::Accept()
{
	UseItem(Chosen);
}

void USaltScreen::PointerMove(const FVector2D& Design)
{
	const int32 I = ItemAt(Design);
	if (I != INDEX_NONE && I != Chosen)
	{
		ChooseItem(I);
	}
}

void USaltScreen::PointerDown(const FVector2D& Design)
{
	const int32 I = ItemAt(Design);
	if (I != INDEX_NONE)
	{
		UseItem(I);
	}
}

void USaltScreen::PlaceHints(const TArray<SaltMenu::FHint>& InHints, float X, bool bCentre)
{
	if (HintsRow)
	{
		HintsRow->RemoveFromParent();
	}
	HintsRow = SaltMenu::Hints(WidgetTree, InHints);
	// The row's middle 3.4cqw + half a key above the bottom edge (Menus.html .hints).
	SaltUI::At(Contents, HintsRow, FVector2D(bCentre ? 960.f : X, 1080.f - 65.f - 28.f), FVector2D::ZeroVector, FVector2D(bCentre ? 0.5f : 0.f, 0.5f));
}

FReply USaltScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (RawKey(Key, InKeyEvent.IsRepeat()))
	{
		return FReply::Handled();
	}
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
		Navigate(0, -1);
	}
	else if (Is({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down }))
	{
		Navigate(0, 1);
	}
	else if (Is({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left }))
	{
		Navigate(-1, 0);
	}
	else if (Is({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right }))
	{
		Navigate(1, 0);
	}
	else if (Is({ EKeys::Gamepad_LeftShoulder }))
	{
		Page(-1);
	}
	else if (Is({ EKeys::Gamepad_RightShoulder }))
	{
		Page(1);
	}
	else if (Is({ EKeys::Enter, EKeys::SpaceBar, EKeys::E, EKeys::Gamepad_FaceButton_Bottom }))
	{
		if (!InKeyEvent.IsRepeat())
		{
			Accept();
		}
	}
	else if (Is({ EKeys::Escape, EKeys::F10, EKeys::Gamepad_Special_Right, EKeys::Gamepad_FaceButton_Right, EKeys::BackSpace }))
	{
		if (!InKeyEvent.IsRepeat())
		{
			Back();
		}
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

FReply USaltScreen::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	RawKeyUp(InKeyEvent.GetKey());
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply USaltScreen::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	PointerMove(SaltUI::DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()));
	return FReply::Handled();
}

FReply USaltScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Back();
		return FReply::Handled();
	}
	PointerDown(SaltUI::DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()));
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply USaltScreen::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	PointerUp(SaltUI::DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()));
	return FReply::Handled().ReleaseMouseCapture();
}

FReply USaltScreen::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Navigate(0, InMouseEvent.GetWheelDelta() > 0.f ? -1 : 1);
	return FReply::Handled();
}

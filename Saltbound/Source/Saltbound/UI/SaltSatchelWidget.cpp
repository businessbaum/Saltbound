// Saltbound - the satchel (D50, restyled D55, UI_Design.md section 5) and the map screen.

#include "UI/SaltSatchelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Boat/SaltBoat.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Core/SaltItems.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Styling/CoreStyle.h"
#include "UI/SaltUIStyle.h"
#include "World/SaltPickup.h"

using namespace SaltUI;

namespace
{
	/** The charts (keep in sync with MAPS in Tools/textures/gen_ui.py): centre X / Y and size in metres. */
	struct FChart { const TCHAR* Texture; FVector2D Centre; float Size; const TCHAR* Scale; float ScaleM; FText Name; };
	const FChart& Chart(int32 Index)
	{
		static const FChart Charts[] = {
			{ TEXT("T_Map_Home"), FVector2D(1000.0, 0.0), 3000.f, TEXT("500 m"), 500.f, NSLOCTEXT("Saltbound", "ChartHome", "Home waters") },
			{ TEXT("T_Map_Sea"), FVector2D(3000.0, 0.0), 12000.f, TEXT("2 km"), 2000.f, NSLOCTEXT("Saltbound", "ChartSea", "Open sea") },
		};
		return Charts[FMath::Clamp(Index, 0, 1)];
	}

	struct FPlace { FText Name; FVector2D WorldM; };
	const TArray<FPlace>& Places()
	{
		static const TArray<FPlace> P = {
			{ NSLOCTEXT("Saltbound", "PlaceCove", "Saltbound Cove"), FVector2D(0.0, 0.0) },
			{ NSLOCTEXT("Saltbound", "PlaceSpire", "The Spire"), FVector2D(420.0, 150.0) },
			{ NSLOCTEXT("Saltbound", "PlaceRuinIsle", "Ruin Isle"), FVector2D(1600.0, 600.0) },
			{ NSLOCTEXT("Saltbound", "PlaceMistholm", "Mistholm"), FVector2D(5500.0, 2500.0) },
			{ NSLOCTEXT("Saltbound", "PlaceGullRock", "Gull Rock"), FVector2D(7000.0, -1500.0) },
			{ NSLOCTEXT("Saltbound", "PlaceLongshoal", "Longshoal"), FVector2D(4200.0, -4800.0) },
			{ NSLOCTEXT("Saltbound", "PlaceStorm", "Storm waters"), FVector2D(0.0, -3000.0) },
		};
		return P;
	}

	ASaltBoat* FindBoat(const UObject* Context)
	{
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		for (TActorIterator<ASaltBoat> It(World); World && It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Places a child of a centre-anchored full-screen canvas at 1080p design coordinates (the screen centre is 960, 540). */
	UCanvasPanelSlot* At(UCanvasPanel* Canvas, UWidget* W, FVector2D Design, FVector2D Size, FVector2D Alignment = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(Alignment);
		S->SetPosition(Design - FVector2D(960.f, 540.f));
		if (Size.X > 0.f)
		{
			S->SetSize(Size);
		}
		else
		{
			S->SetAutoSize(true);
		}
		return S;
	}

	UTexture2D* IconOf(const FSaltItemDef* Item)
	{
		return Item && Item->Icon ? LoadObject<UTexture2D>(nullptr, Item->Icon) : nullptr;
	}

	/** The blurred shadow made from the icon (Tools/textures/gen_ui_parts.py): T_Icon_X -> T_UI_IconShadow_X. */
	UTexture2D* ShadowOf(const FSaltItemDef* Item)
	{
		if (!Item || !Item->Icon)
		{
			return nullptr;
		}
		const FString Path = FString(Item->Icon).Replace(TEXT("T_Icon_"), TEXT("T_UI_IconShadow_"));
		return LoadObject<UTexture2D>(nullptr, *Path);
	}

	// Pocket layout at 1080p (section 5): 8 x 3, centres x = 401 + 124 col, y = 616 + 140 row; the tag's hole at (1498, 540).
	FVector2D PocketCentre(int32 I) { return FVector2D(401.f + 124.f * (I % 8), 616.f + 140.f * (I / 8)); }
	const FVector2D TagHole(1498.f, 540.f);
	const FVector2D TagHoleInFrame(190.f, 80.f);   // T_UI_Tag is 400 x 600 with its hole here
	constexpr float TagRest = 4.f;                 // degrees
}

// --- Pocket -------------------------------------------------------------------------------------------------------

void USaltItemSlotWidget::Setup(int32 InIndex, TFunction<void(int32)> InOnClicked, TFunction<void(int32)> InOnHovered, float InRadius)
{
	Index = InIndex;
	OnClickedFn = MoveTemp(InOnClicked);
	OnHoveredFn = MoveTemp(InOnHovered);
	Radius = InRadius;
}

void USaltItemSlotWidget::Place(UWidget* W, FVector2D Centre, FVector2D Size)
{
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
	{
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D(2.f * Radius, 2.f * Radius) + Centre);
		S->SetSize(Size);
	}
}

TSharedRef<SWidget> USaltItemSlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		const float K = Radius / 50.f;
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(4.f * Radius);
		Box->SetHeightOverride(4.f * Radius);
		WidgetTree->RootWidget = Box;
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		Box->AddChild(Root);
		auto Img = [this](const TCHAR* Texture, FVector2D Size)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>();
			if (Texture)
			{
				I->SetBrush(ImageBrush(Tex(Texture), Size));
			}
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			Root->AddChildToCanvas(I);
			return I;
		};
		Glow = Img(TEXT("T_UI_SlotGlow"), FVector2D(200.f, 200.f) * K);
		Place(Glow, FVector2D::ZeroVector, FVector2D(200.f, 200.f) * K);
		Ring = Img(TEXT("T_UI_SlotRing"), FVector2D(200.f, 200.f) * K);
		Place(Ring, FVector2D::ZeroVector, FVector2D(200.f, 200.f) * K);
		Contact = Img(TEXT("T_UI_ContactShadow"), FVector2D(100.f, 50.f) * K);
		Place(Contact, FVector2D(2.f, 20.f) * K, FVector2D(100.f, 50.f) * K);
		ShardGlow = Img(TEXT("T_UI_ShardGlow"), FVector2D(200.f, 200.f) * K);
		IconShadow = Img(nullptr, FVector2D::ZeroVector);
		IconImage = Img(nullptr, FVector2D::ZeroVector);
		IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		IconShadow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UTextBlock* CoinLabel = nullptr;
		Coin = CountCoin(WidgetTree, &CoinLabel, 17.f * FMath::Max(K, 0.9f));
		CoinText = CoinLabel;
		Coin->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Coin))
		{
			S->SetAutoSize(true);
			S->SetAlignment(FVector2D(23.f / 48.f, 22.f / 48.f));
			S->SetPosition(FVector2D(2.f * Radius + 38.f * K, 2.f * Radius + 36.f * K));
		}
		// The pocket itself is the button (hover and click), round as the dish.
		Button = WidgetTree->ConstructWidget<USaltMenuButton>();
		FButtonStyle Style;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
		Button->SetStyle(Style);
		Button->OnClicked.AddDynamic(this, &USaltItemSlotWidget::HandleClicked);
		Button->OnHovered.AddDynamic(this, &USaltItemSlotWidget::HandleHovered);
		Root->AddChildToCanvas(Button);
		Place(Button, FVector2D::ZeroVector, FVector2D(2.f * Radius, 2.f * Radius));
		Show(nullptr, 0, false);
	}
	return Super::RebuildWidget();
}

void USaltItemSlotWidget::Show(const FSaltItemDef* Item, int32 Count, bool bInSelected)
{
	if (!IconImage)
	{
		return;
	}
	bSelected = bInSelected;
	bHasItem = Item != nullptr;
	bAncient = Item && Item->Id == TEXT("AncientShard");
	if (UTexture2D* Icon = IconOf(Item))
	{
		IconImage->SetBrush(ImageBrush(Icon, FVector2D(96.f, 96.f)));
		UTexture2D* Shadow = ShadowOf(Item);
		IconShadow->SetBrush(ImageBrush(Shadow, FVector2D(96.f, 96.f)));
		IconShadow->SetVisibility(Shadow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		Contact->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		IconShadow->SetVisibility(ESlateVisibility::Collapsed);
		Contact->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShardGlow->SetVisibility(bAncient ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetCoinCount(Coin, CoinText, Item ? Count : 0);
}

void USaltItemSlotWidget::Turn()
{
	TurnTime = 0.f;
}

void USaltItemSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IconImage)
	{
		return;
	}
	Clock += InDeltaTime;
	// Selection animates in about 0.12 s: the glow and the gold ring come up, the item grows and lifts.
	SelAlpha = FMath::FInterpConstantTo(SelAlpha, bSelected ? 1.f : 0.f, InDeltaTime, 1.f / 0.12f);
	const float K = Radius / 50.f;
	const float E = FMath::InterpEaseInOut(0.f, 1.f, SelAlpha, 2.f);
	Glow->SetRenderOpacity(E);
	Ring->SetRenderOpacity(E);
	const float Size = FMath::Lerp(86.f, 96.f, E) * K;
	const float Lift = 14.f * K * E;
	const FVector2D IconCentre(0.f, -2.f * K - Lift);
	Place(IconImage, IconCentre, FVector2D(Size, Size));
	Place(IconShadow, IconCentre + FVector2D(3.f, 6.f + 12.f * E) * K, FVector2D(Size, Size));
	IconShadow->SetRenderOpacity(FMath::Lerp(1.f, 0.8f, E));
	// The shard's own slow turquoise glow (about 2.5 s).
	if (bAncient)
	{
		const float G = Size / 96.f * 200.f;
		Place(ShardGlow, IconCentre, FVector2D(G, G));
		ShardGlow->SetRenderOpacity(0.7f + 0.3f * FMath::Sin(Clock * TWO_PI / 2.5f));
	}
	// Turning it in the hand: the icon turns once about its upright axis (its width goes through zero) in 0.9 s.
	if (TurnTime >= 0.f)
	{
		TurnTime += InDeltaTime;
		const float T = FMath::Clamp(TurnTime / 0.9f, 0.f, 1.f);
		const float Sx = FMath::Cos(T * TWO_PI);
		IconImage->SetRenderScale(FVector2D(FMath::Abs(Sx) < 0.05f ? 0.05f : Sx, 1.f));
		IconShadow->SetRenderScale(FVector2D(FMath::Abs(Sx) < 0.05f ? 0.05f : Sx, 1.f));
		if (T >= 1.f)
		{
			TurnTime = -1.f;
			IconImage->SetRenderScale(FVector2D(1.f, 1.f));
			IconShadow->SetRenderScale(FVector2D(1.f, 1.f));
		}
	}
}

void USaltItemSlotWidget::HandleClicked()
{
	if (OnClickedFn)
	{
		OnClickedFn(Index);
	}
}

void USaltItemSlotWidget::HandleHovered()
{
	if (OnHoveredFn)
	{
		OnHoveredFn(Index);
	}
}

// --- Chart button (map screen) ------------------------------------------------------------------------------------

void USaltSatchelTab::Setup(USaltSatchelWidget* InOwner, int32 InPage, const FText& InLabel, bool bInChart)
{
	Owner = InOwner;
	Page = InPage;
	Label = InLabel;
	bChart = bInChart;
}

TSharedRef<SWidget> USaltSatchelTab::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetMinDesiredWidth(bChart ? 150.f : 170.f);
		Size->SetHeightOverride(bChart ? 40.f : 48.f);
		WidgetTree->RootWidget = Size;
		Button = WidgetTree->ConstructWidget<USaltMenuButton>();
		Button->OnClicked.AddDynamic(this, &USaltSatchelTab::HandleClicked);
		Size->SetContent(Button);
		LabelText = Text(WidgetTree, Label, bChart ? 15 : 18, ParchmentText, true);
		LabelText->SetJustification(ETextJustify::Center);
		Button->AddChild(LabelText);
		SetActive(false);
	}
	return Super::RebuildWidget();
}

void USaltSatchelTab::SetActive(bool bActive)
{
	if (!Button)
	{
		return;
	}
	const FSlateBrush B = BoxBrush(Tex(bActive ? TEXT("T_UI_TabActive") : TEXT("T_UI_Tab")), FVector2D(170.f, 48.f));
	FSlateBrush Hover = B;
	Hover.TintColor = FSlateColor(FLinearColor(1.f, 0.93f, 0.85f));
	FButtonStyle Style;
	Style.SetNormal(B).SetHovered(Hover).SetPressed(Hover).SetDisabled(B);
	Style.SetNormalPadding(FMargin(18.f, 6.f)).SetPressedPadding(FMargin(18.f, 7.f, 18.f, 5.f));
	Button->SetStyle(Style);
	LabelText->SetColorAndOpacity(FSlateColor(bActive ? Ink : ParchmentText));
}

void USaltSatchelTab::HandleClicked()
{
	if (USaltSatchelWidget* O = Owner.Get())
	{
		bChart ? O->SetChart(Page) : O->SetPage(Page);
	}
}

// --- Satchel ------------------------------------------------------------------------------------------------------

USaltSatchelWidget::USaltSatchelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> USaltSatchelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Screens = WidgetTree->ConstructWidget<UWidgetSwitcher>();
		WidgetTree->RootWidget = Screens;
		Screens->AddChild(BuildSatchelScreen());
		Screens->AddChild(BuildMapScreen());
	}
	return Super::RebuildWidget();
}

UWidget* USaltSatchelWidget::BuildSatchelScreen()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	auto FullScreen = [Canvas](UWidget* W)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	};
	// The world behind: blurred, darkened, a vignette.
	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	Blur->SetBlurStrength(8.f);
	FullScreen(Blur);
	UImage* Dark = WidgetTree->ConstructWidget<UImage>();
	Dark->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	FullScreen(Dark);
	UImage* Vignette = WidgetTree->ConstructWidget<UImage>();
	Vignette->SetBrush(ImageBrush(Tex(TEXT("T_UI_Vignette")), FVector2D(256.f, 144.f)));
	FullScreen(Vignette);

	// The bag (everything that does not change), then the count, the pockets and the tag.
	UImage* Bag = WidgetTree->ConstructWidget<UImage>();
	Bag->SetBrush(ImageBrush(Tex(TEXT("T_UI_SatchelBag")), FVector2D(1520.f, 900.f)));
	At(Canvas, Bag, FVector2D(200.f, 180.f), FVector2D(1520.f, 900.f));
	// "7 / 24" debossed, right-aligned at x 1328 on the baseline 490 (IM Fell English 38 px: ascent 0.905 em).
	UTextBlock* CountInk = nullptr;
	UTextBlock* CountHighlight = nullptr;
	UOverlay* Count = Deboss(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 38.f, 0.f, ETextJustify::Right, &CountInk, &CountHighlight);
	CountDark = CountInk;
	CountLip = CountHighlight;
	At(Canvas, Count, FVector2D(1328.f, 490.f - 0.905f * 38.f), FVector2D::ZeroVector, FVector2D(1.f, 0.f));

	SlotWidgets.Reset();
	for (int32 I = 0; I < USaltInventoryComponent::Capacity; ++I)
	{
		USaltItemSlotWidget* W = CreateWidget<USaltItemSlotWidget>(this, USaltItemSlotWidget::StaticClass());
		W->Setup(I, [this](int32 SlotIndex) { SelectSlot(SlotIndex); }, [this](int32 SlotIndex) { SelectSlot(SlotIndex); }, 50.f);
		SlotWidgets.Add(W);
		At(Canvas, W, PocketCentre(I), FVector2D(200.f, 200.f), FVector2D(0.5f, 0.5f));
	}

	UWidget* Tag = BuildTag();
	At(Canvas, Tag, TagHole - TagHoleInFrame, FVector2D(400.f, 600.f));
	UImage* Twine = WidgetTree->ConstructWidget<UImage>();
	Twine->SetBrush(ImageBrush(Tex(TEXT("T_UI_TagTwineFront")), FVector2D(80.f, 80.f)));
	Twine->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(Canvas, Twine, TagHole - FVector2D(40.f, 52.f), FVector2D(80.f, 80.f));
	return Canvas;
}

UWidget* USaltSatchelWidget::BuildTag()
{
	// The tag pivots at its hole; everything on it is laid out in its own 400 x 600 frame (hole at 190, 80).
	TagRoot = WidgetTree->ConstructWidget<UCanvasPanel>();
	TagRoot->SetRenderTransformPivot(FVector2D(TagHoleInFrame.X / 400.f, TagHoleInFrame.Y / 600.f));
	TagRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	UImage* Paper = WidgetTree->ConstructWidget<UImage>();
	Paper->SetBrush(ImageBrush(Tex(TEXT("T_UI_Tag")), FVector2D(400.f, 600.f)));
	if (UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(Paper))
	{
		S->SetSize(FVector2D(400.f, 600.f));
	}
	const float L = TagHoleInFrame.X - 150.f + 28.f;   // 28 px inside the tag's left edge
	auto Put = [this, L](UWidget* W, float Top, float Width = 0.f)
	{
		UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(W);
		S->SetPosition(FVector2D(L, Top));
		if (Width > 0.f)
		{
			S->SetSize(FVector2D(Width, 300.f));
			S->SetAutoSize(false);
		}
		else
		{
			S->SetAutoSize(true);
		}
		return S;
	};
	// Name (IM Fell English 44, baseline 60 under the hole), the count line (italic 21, baseline +34), an ink rule (+22).
	TagName = InkText(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 44.f);
	Put(TagName, TagHoleInFrame.Y + 60.f - 0.905f * 44.f);
	TagCount = FaceText(WidgetTree, FText::GetEmpty(), ESaltFace::FellItalic, 21.f, SRGB(90, 62, 36, 0.95f));
	Put(TagCount, TagHoleInFrame.Y + 94.f - 0.905f * 21.f);
	UImage* Rule = WidgetTree->ConstructWidget<UImage>();
	Rule->SetColorAndOpacity(SRGB(58, 38, 22, 0.5f));
	if (UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(Rule))
	{
		S->SetPosition(FVector2D(L, TagHoleInFrame.Y + 116.f));
		S->SetSize(FVector2D(244.f, 1.2f));
	}
	// The description (Alegreya 21 on 29 px lines, wrapped to 244 px) and the actions below it, 46 px apart.
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(Column))
	{
		S->SetPosition(FVector2D(L, TagHoleInFrame.Y + 152.f - 1.123f * 21.f));
		S->SetSize(FVector2D(250.f, 330.f));
	}
	TagDescription = InkText(WidgetTree, FText::GetEmpty(), ESaltFace::Alegreya, 21.f);
	TagDescription->SetAutoWrapText(true);
	TagDescription->SetWrapTextAt(244.f);
	TagDescription->SetLineHeightPercentage(29.f / (1.468f * 21.f));
	Column->AddChildToVerticalBox(TagDescription);
	TagActions = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(TagActions)->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
	auto Action = [this](const TCHAR* Key, const FText& Label)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Keycap(WidgetTree, FText::FromString(Key), 30.f))->SetVerticalAlignment(VAlign_Center);
		UTextBlock* T = InkText(WidgetTree, Label, ESaltFace::Alegreya, 22.f);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(T))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 4.f));
		}
		TagActions->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));
		return T;
	};
	UseText = Action(TEXT("E"), FText::GetEmpty());
	Action(TEXT("R"), NSLOCTEXT("Saltbound", "TagTurn", "Turn it in your hand"));
	Action(TEXT("G"), NSLOCTEXT("Saltbound", "TagDrop", "Drop it"));
	return TagRoot;
}

UWidget* USaltSatchelWidget::BuildMapScreen()
{
	// The map keeps its D50 look (a parchment chart in a wooden frame), now as its own screen on M.
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
	Backdrop->SetHorizontalAlignment(HAlign_Center);
	Backdrop->SetVerticalAlignment(VAlign_Center);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(1060.f);
	Size->SetHeightOverride(690.f);
	Backdrop->SetContent(Size);
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrush(BoxBrush(Tex(TEXT("T_UI_Frame")), FVector2D(256.f, 256.f)));
	Frame->SetPadding(FMargin(30.f));
	Size->SetContent(Frame);
	UBorder* Paper = WidgetTree->ConstructWidget<UBorder>();
	Paper->SetBrush(ImageBrush(Tex(TEXT("T_UI_Parchment")), FVector2D(512.f, 512.f), true));
	Paper->SetPadding(FMargin(22.f, 14.f, 22.f, 18.f));
	Frame->SetContent(Paper);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Paper->SetContent(Column);
	UTextBlock* Hint = Text(WidgetTree, NSLOCTEXT("Saltbound", "MapHint", "M / Esc  close     I  satchel"), 13, InkSoft);
	Hint->SetJustification(ETextJustify::Right);
	Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BuildMapPage()))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	return Backdrop;
}

UWidget* USaltSatchelWidget::BuildMapPage()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	USizeBox* MapBox = WidgetTree->ConstructWidget<USizeBox>();
	MapBox->SetWidthOverride(MapSize);
	MapBox->SetHeightOverride(MapSize);
	Row->AddChildToHorizontalBox(MapBox)->SetVerticalAlignment(VAlign_Top);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	MapBox->SetContent(Layers);
	ChartTextures = { Tex(TEXT("T_Map_Home")), Tex(TEXT("T_Map_Sea")) };
	ChartImage = WidgetTree->ConstructWidget<UImage>();
	Layers->AddChildToOverlay(ChartImage);
	ChartOverlay = WidgetTree->ConstructWidget<UCanvasPanel>();
	if (UOverlaySlot* S = Layers->AddChildToOverlay(ChartOverlay))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	PlaceLabels.Reset();
	for (const FPlace& P : Places())
	{
		UTextBlock* L = Text(WidgetTree, P.Name, 13, Ink, true);
		L->SetShadowOffset(FVector2D(1.f, 1.f));
		L->SetShadowColorAndOpacity(FLinearColor(0.95f, 0.88f, 0.7f, 0.6f));
		if (UCanvasPanelSlot* S = ChartOverlay->AddChildToCanvas(L))
		{
			S->SetAutoSize(true);
			S->SetAlignment(FVector2D(0.5f, 0.f));
		}
		PlaceLabels.Add(L);
	}
	auto Marker = [this](const TCHAR* Name, float Px)
	{
		UImage* M = WidgetTree->ConstructWidget<UImage>();
		M->SetBrush(ImageBrush(Tex(Name), FVector2D(Px, Px)));
		M->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (UCanvasPanelSlot* S = ChartOverlay->AddChildToCanvas(M))
		{
			S->SetSize(FVector2D(Px, Px));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		return M;
	};
	BoatMarker = Marker(TEXT("T_MapMarker_Boat"), 30.f);
	PlayerMarker = Marker(TEXT("T_MapMarker_Player"), 26.f);

	UVerticalBox* Side = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Side))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetPadding(FMargin(26.f, 4.f, 4.f, 0.f));
	}
	ChartTitle = Text(WidgetTree, FText::GetEmpty(), 24, Ink, true);
	Side->AddChildToVerticalBox(ChartTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	ChartButtons.Reset();
	for (int32 I = 0; I < 2; ++I)
	{
		USaltSatchelTab* B = CreateWidget<USaltSatchelTab>(this, USaltSatchelTab::StaticClass());
		B->Setup(this, I, Chart(I).Name, true);
		ChartButtons.Add(B);
		Side->AddChildToVerticalBox(B)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
	auto LegendRow = [this, Side](const TCHAR* Icon, const FText& Label)
	{
		UHorizontalBox* L = WidgetTree->ConstructWidget<UHorizontalBox>();
		UImage* I = WidgetTree->ConstructWidget<UImage>();
		I->SetBrush(ImageBrush(Tex(Icon), FVector2D(22.f, 22.f)));
		L->AddChildToHorizontalBox(I)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* T = Text(WidgetTree, Label, 15, Ink);
		L->AddChildToHorizontalBox(T)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		Side->AddChildToVerticalBox(L)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	};
	Side->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>())->SetPadding(FMargin(0.f, 10.f));
	LegendRow(TEXT("T_MapMarker_Player"), NSLOCTEXT("Saltbound", "LegendYou", "You"));
	LegendRow(TEXT("T_MapMarker_Boat"), NSLOCTEXT("Saltbound", "LegendBoat", "Your boat"));
	BoatDistance = Text(WidgetTree, FText::GetEmpty(), 15, InkSoft);
	Side->AddChildToVerticalBox(BoatDistance)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	UHorizontalBox* Scale = WidgetTree->ConstructWidget<UHorizontalBox>();
	USizeBox* Bar = WidgetTree->ConstructWidget<USizeBox>();
	Bar->SetWidthOverride(MapSize * 500.f / 3000.f);
	Bar->SetHeightOverride(5.f);
	UBorder* BarInk = WidgetTree->ConstructWidget<UBorder>();
	BarInk->SetBrushColor(Ink);
	Bar->SetContent(BarInk);
	Scale->AddChildToHorizontalBox(Bar)->SetVerticalAlignment(VAlign_Center);
	ScaleLabel = Text(WidgetTree, FText::GetEmpty(), 14, Ink);
	Scale->AddChildToHorizontalBox(ScaleLabel)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	Side->AddChildToVerticalBox(Scale)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	return Row;
}

void USaltSatchelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			InventoryChanged = Satchel->OnChanged.AddUObject(this, &USaltSatchelWidget::Refresh);
		}
	}
	SetChart(CurrentChart);
	SetPage(CurrentPage);
}

void USaltSatchelWidget::NativeDestruct()
{
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			Satchel->OnChanged.Remove(InventoryChanged);
		}
	}
	Super::NativeDestruct();
}

void USaltSatchelWidget::SetPage(int32 Page)
{
	CurrentPage = FMath::Clamp(Page, 0, 1);
	if (Screens)
	{
		Screens->SetActiveWidgetIndex(CurrentPage);
	}
	Refresh();
}

void USaltSatchelWidget::SetChart(int32 InChart)
{
	CurrentChart = FMath::Clamp(InChart, 0, 1);
	if (ChartImage && ChartTextures.IsValidIndex(CurrentChart))
	{
		ChartImage->SetBrush(ImageBrush(ChartTextures[CurrentChart], FVector2D(MapSize, MapSize)));
	}
	for (int32 I = 0; I < ChartButtons.Num(); ++I)
	{
		ChartButtons[I]->SetActive(I == CurrentChart);
	}
	if (ChartTitle)
	{
		ChartTitle->SetText(Chart(CurrentChart).Name);
		ScaleLabel->SetText(FText::FromString(Chart(CurrentChart).Scale));
	}
	UpdateMarkers();
}

void USaltSatchelWidget::SelectSlot(int32 Index)
{
	if (Index >= 0 && Index < USaltInventoryComponent::Capacity && Index != Selected)
	{
		Selected = Index;
		// The tag swings: a ~6 degree kick that settles in about 0.8 s.
		TagSwingRate += (Index % 2 ? -1.f : 1.f) * 70.f;
		Refresh();
	}
}

FText USaltSatchelWidget::GetSelectedName() const
{
	return TagName ? TagName->GetText() : FText::GetEmpty();
}

FText USaltSatchelWidget::GetTagLine(int32 Line) const
{
	const UTextBlock* T = Line == 0 ? TagName.Get() : Line == 1 ? TagCount.Get() : TagDescription.Get();
	return T ? T->GetText() : FText::GetEmpty();
}

void USaltSatchelWidget::Refresh()
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	if (!Satchel || !TagName)
	{
		return;
	}
	const TArray<FSaltItemStack>& Slots = Satchel->GetSlots();
	int32 Used = 0;
	for (int32 I = 0; I < SlotWidgets.Num(); ++I)
	{
		const FSaltItemStack Stack = Slots.IsValidIndex(I) ? Slots[I] : FSaltItemStack();
		const FSaltItemDef* Def = Stack.IsEmpty() ? nullptr : SaltItems::Find(Stack.Id);
		Used += Def ? 1 : 0;
		SlotWidgets[I]->Show(Def, Def ? Stack.Count : 0, I == Selected);
	}
	const FText CountText = FText::Format(NSLOCTEXT("Saltbound", "SatchelUsed", "{0} / {1}"), FText::AsNumber(Used),
		FText::AsNumber(USaltInventoryComponent::Capacity));
	CountDark->SetText(CountText);
	CountLip->SetText(CountText);
	const FSaltItemStack Sel = Slots.IsValidIndex(Selected) ? Slots[Selected] : FSaltItemStack();
	const FSaltItemDef* Def = Sel.IsEmpty() ? nullptr : SaltItems::Find(Sel.Id);
	if (Def)
	{
		TagName->SetText(Def->Name);
		TagCount->SetText(FText::Format(NSLOCTEXT("Saltbound", "TagCount", "{0} in this pocket · {1} fit"), FText::AsNumber(Sel.Count),
			FText::AsNumber(Def->MaxStack)));
		TagDescription->SetText(Def->Description);
		// No use works yet: an item's use is shown greyed; one without a use says "(later)".
		UseText->SetText(Def->UseText.IsEmpty() ? NSLOCTEXT("Saltbound", "TagUseLater", "(later)") : Def->UseText);
		UseText->SetColorAndOpacity(FSlateColor(SRGB(0x3A, 0x26, 0x16, 0.45f)));
		TagActions->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		TagName->SetText(NSLOCTEXT("Saltbound", "EmptyPocket", "Empty pocket"));
		TagCount->SetText(FText::GetEmpty());
		TagDescription->SetText(NSLOCTEXT("Saltbound", "EmptyPocketHelp", "Things you find on beaches, in wrecks and among the ruins go here."));
		TagActions->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool USaltSatchelWidget::DropSelected()
{
	ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	UWorld* World = GetWorld();
	if (!Satchel || !World || !Satchel->GetSlots().IsValidIndex(Selected) || Satchel->GetSlots()[Selected].IsEmpty())
	{
		return false;
	}
	const FName Id = Satchel->GetSlots()[Selected].Id;
	if (Satchel->Remove(Id, 1) != 1)
	{
		return false;
	}
	// It lands on whatever is in front of the player's feet.
	const FVector Ahead = Player->GetActorLocation() + Player->GetActorForwardVector() * 70.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltDrop), false, Player);
	FVector Spot = Ahead - FVector(0.f, 0.f, 80.f);
	if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0.f, 0.f, 50.f), Ahead - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
	{
		Spot = Hit.ImpactPoint;
	}
	ASaltPickup* Pickup = World->SpawnActorDeferred<ASaltPickup>(ASaltPickup::StaticClass(), FTransform(FRotator(0.f, Player->GetActorRotation().Yaw, 0.f), Spot));
	if (Pickup)
	{
		Pickup->ItemId = Id;
		Pickup->Count = 1;
		Pickup->FinishSpawning(FTransform(FRotator(0.f, Player->GetActorRotation().Yaw, 0.f), Spot));
		if (Hit.GetComponent())
		{
			Pickup->AttachToComponent(Hit.GetComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	return true;
}

FVector2D USaltSatchelWidget::WorldToChart(const FVector& World) const
{
	const FChart& C = Chart(CurrentChart);
	const double U = 0.5 + (World.Y * 0.01 - C.Centre.Y) / C.Size;    // east = right
	const double V = 0.5 - (World.X * 0.01 - C.Centre.X) / C.Size;    // north (+X) = up
	return FVector2D(U, V) * MapSize;
}

FVector2D USaltSatchelWidget::GetPlayerMarkerPosition() const
{
	const UCanvasPanelSlot* S = PlayerMarker ? Cast<UCanvasPanelSlot>(PlayerMarker->Slot) : nullptr;
	return S ? S->GetPosition() : FVector2D(-1.0, -1.0);
}

void USaltSatchelWidget::UpdateMarkers()
{
	if (!ChartOverlay)
	{
		return;
	}
	auto Place = [](UWidget* W, const FVector2D& P, bool bShow)
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
		{
			S->SetPosition(P);
		}
		W->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	};
	auto Inside = [](const FVector2D& P, float Margin) { return P.X > Margin && P.Y > Margin && P.X < MapSize - Margin && P.Y < MapSize - Margin; };
	const TArray<FPlace>& Ps = Places();
	for (int32 I = 0; I < PlaceLabels.Num(); ++I)
	{
		const FVector2D P = WorldToChart(FVector(Ps[I].WorldM.X * 100.0, Ps[I].WorldM.Y * 100.0, 0.0)) + FVector2D(0.0, 16.0);
		Place(PlaceLabels[I], P, Inside(P, 18.f));
	}
	auto Clamp = [](const FVector2D& P) { return FVector2D(FMath::Clamp(P.X, 14.0, MapSize - 14.0), FMath::Clamp(P.Y, 14.0, MapSize - 14.0)); };
	const APawn* Pawn = GetOwningPlayerPawn();
	const APlayerController* PC = GetOwningPlayer();
	if (Pawn && PC)
	{
		Place(PlayerMarker, Clamp(WorldToChart(Pawn->GetActorLocation())), true);
		PlayerMarker->SetRenderTransformAngle(PC->GetControlRotation().Yaw);
	}
	if (const ASaltBoat* Boat = FindBoat(this))
	{
		Place(BoatMarker, Clamp(WorldToChart(Boat->GetActorLocation())), true);
		BoatMarker->SetRenderTransformAngle(Boat->GetYawDeg());
		const double Dist = Pawn ? FVector::Dist2D(Pawn->GetActorLocation(), Boat->GetActorLocation()) * 0.01 : 0.0;
		BoatDistance->SetText(Dist < 6.0 ? NSLOCTEXT("Saltbound", "BoatAboard", "You are aboard your boat.")
			: FText::Format(NSLOCTEXT("Saltbound", "BoatDistance", "Your boat lies {0} m away."), FText::AsNumber(FMath::RoundToInt(Dist))));
	}
	else
	{
		BoatMarker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USaltSatchelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CurrentPage == Map)
	{
		UpdateMarkers();
	}
	// The tag swings on its twine: a damped spring (it settles in about 0.8 s).
	const float Dt = FMath::Min(InDeltaTime, 0.05f);
	const float Omega = 12.f;
	TagSwingRate += (-Omega * Omega * TagSwing - 2.f * 0.42f * Omega * TagSwingRate) * Dt;
	TagSwing += TagSwingRate * Dt;
	if (TagRoot)
	{
		TagRoot->SetRenderTransformAngle(TagRest + TagSwing);
	}
}

void USaltSatchelWidget::RequestClose()
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
	{
		PC->SetSatchelOpen(false);
	}
}

FReply USaltSatchelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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
	const bool bBag = CurrentPage == Inventory;
	if (Is({ EKeys::Escape, EKeys::F10, EKeys::Gamepad_FaceButton_Right }))
	{
		RequestClose();
	}
	else if (Is({ EKeys::I, EKeys::Tab, EKeys::Gamepad_Special_Left }))
	{
		bBag ? RequestClose() : SetPage(Inventory);
	}
	else if (Is({ EKeys::M }))
	{
		bBag ? SetPage(Map) : RequestClose();
	}
	else if (bBag && Is({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left }))
	{
		SelectSlot(Selected - 1);
	}
	else if (bBag && Is({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right }))
	{
		SelectSlot(Selected + 1);
	}
	else if (bBag && Is({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }))
	{
		SelectSlot(Selected - Columns);
	}
	else if (bBag && Is({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }))
	{
		SelectSlot(Selected + Columns);
	}
	else if (bBag && Is({ EKeys::R, EKeys::Gamepad_FaceButton_Top }))
	{
		if (SlotWidgets.IsValidIndex(Selected))
		{
			SlotWidgets[Selected]->Turn();
		}
	}
	else if (bBag && Is({ EKeys::G, EKeys::Gamepad_FaceButton_Left }))
	{
		DropSelected();
	}
	else if (!bBag && Is({ EKeys::Left, EKeys::Right, EKeys::A, EKeys::D, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right }))
	{
		SetChart(1 - CurrentChart);
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

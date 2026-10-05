// Saltbound - the satchel (D50, restyled D55, UI_Design.md section 5). The map left it (D57: the chart is on the mast).

#include "UI/SaltSatchelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
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
#include "Core/SaltItems.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltMenuKit.h"
#include "Styling/CoreStyle.h"
#include "UI/SaltUIStyle.h"
#include "World/SaltPickup.h"

using namespace SaltUI;

namespace
{
	// Pocket layout at 1080p (section 5): 8 x 3, centres x = 401 + 124 col, y = 616 + 140 row; the tag's hole at (1498, 540).
	FVector2D PocketCentre(int32 I) { return FVector2D(401.f + 124.f * (I % 8), 616.f + 140.f * (I / 8)); }
	const FVector2D TagHole(1498.f, 540.f);
	const FVector2D TagHoleInFrame(190.f, 80.f);   // T_UI_Tag is 400 x 600 with its hole here
	constexpr float TagRest = 4.f;                 // degrees
}

// --- Pocket -------------------------------------------------------------------------------------------------------

void USaltItemSlotWidget::Setup(int32 InIndex, float InRadius)
{
	Index = InIndex;
	Radius = InRadius;
}

void USaltItemSlotWidget::SetLifted(bool bInLifted)
{
	bLifted = bInLifted;
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
		// The screen it lies on takes the mouse (hover, click, drag: D58).
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	if (UTexture2D* Icon = ItemIcon(Item))
	{
		IconImage->SetBrush(ImageBrush(Icon, FVector2D(96.f, 96.f)));
		UTexture2D* Shadow = ItemIconShadow(Item);
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
	// Something put here drops in a little large and settles (0.18 s, D58).
	float Settle = 1.f;
	if (LandTime >= 0.f)
	{
		LandTime += InDeltaTime;
		const float U = FMath::Clamp(LandTime / 0.18f, 0.f, 1.f);
		Settle = 1.f + 0.12f * FMath::Square(1.f - U);
		LandTime = U >= 1.f ? -1.f : LandTime;
	}
	const float Size = FMath::Lerp(86.f, 96.f, E) * K * Settle;
	const float Lift = 14.f * K * E;
	const FVector2D IconCentre(0.f, -2.f * K - Lift);
	Place(IconImage, IconCentre, FVector2D(Size, Size));
	Place(IconShadow, IconCentre + FVector2D(3.f, 6.f + 12.f * E) * K, FVector2D(Size, Size));
	IconShadow->SetRenderOpacity(FMath::Lerp(1.f, 0.8f, E) * (bLifted ? 0.f : 1.f));
	// Being dragged away (D58): a faint trace stays in the dish, the coin goes with the item.
	IconImage->SetRenderOpacity(bLifted ? 0.25f : 1.f);
	Contact->SetRenderOpacity(bLifted ? 0.f : 1.f);
	Coin->SetRenderOpacity(bLifted ? 0.f : 1.f);
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
		WidgetTree->RootWidget = BuildSatchelScreen();
	}
	return Super::RebuildWidget();
}

UWidget* USaltSatchelWidget::BuildSatchelScreen()
{
	UCanvasPanel* Screen = WidgetTree->ConstructWidget<UCanvasPanel>();
	auto FullScreen = [](UCanvasPanel* Into, UWidget* W)
	{
		UCanvasPanelSlot* S = Into->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	};
	// The world behind: blurred, darkened, a vignette (they fade in as the satchel opens, D58).
	UCanvasPanel* Behind = WidgetTree->ConstructWidget<UCanvasPanel>();
	FullScreen(Screen, Behind);
	Backdrop = Behind;
	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	Blur->SetBlurStrength(8.f);
	FullScreen(Behind, Blur);
	BackdropBlur = Blur;
	UImage* Dark = WidgetTree->ConstructWidget<UImage>();
	Dark->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	FullScreen(Behind, Dark);
	UImage* Vignette = WidgetTree->ConstructWidget<UImage>();
	Vignette->SetBrush(ImageBrush(Tex(TEXT("T_UI_Vignette")), FVector2D(256.f, 144.f)));
	FullScreen(Behind, Vignette);

	// The satchel itself (it rises into place as it opens).
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	FullScreen(Screen, Canvas);
	Contents = Canvas;

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
		W->Setup(I, 50.f);
		SlotWidgets.Add(W);
		At(Canvas, W, PocketCentre(I), FVector2D(200.f, 200.f), FVector2D(0.5f, 0.5f));
	}

	UWidget* Tag = BuildTag();
	At(Canvas, Tag, TagHole - TagHoleInFrame, FVector2D(400.f, 600.f));
	UImage* Twine = WidgetTree->ConstructWidget<UImage>();
	Twine->SetBrush(ImageBrush(Tex(TEXT("T_UI_TagTwineFront")), FVector2D(80.f, 80.f)));
	Twine->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(Canvas, Twine, TagHole - FVector2D(40.f, 52.f), FVector2D(80.f, 80.f));

	// D58: the item being dragged, on top: lifted above the leather (larger, its shadow further below), with its coin.
	UCanvasPanel* DragLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
	DragLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	FullScreen(Screen, DragLayer);
	DragShadow = WidgetTree->ConstructWidget<UImage>();
	DragIcon = WidgetTree->ConstructWidget<UImage>();
	for (UImage* I : { DragShadow.Get(), DragIcon.Get() })
	{
		I->SetVisibility(ESlateVisibility::Collapsed);
		At(DragLayer, I, FVector2D::ZeroVector, FVector2D(106.f, 106.f), FVector2D(0.5f, 0.5f));
	}
	DragShadow->SetRenderOpacity(0.8f);
	UTextBlock* CoinLabel = nullptr;
	DragCoin = CountCoin(WidgetTree, &CoinLabel, 17.f);
	DragCoinText = CoinLabel;
	DragCoin->SetVisibility(ESlateVisibility::Collapsed);
	At(DragLayer, DragCoin, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(23.f / 48.f, 22.f / 48.f));
	return Screen;
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
	UseRow = Cast<UHorizontalBox>(UseText->GetParent());
	UseRowKey = TEXT("E");
	Action(TEXT("R"), NSLOCTEXT("Saltbound", "TagTurn", "Turn it in your hand"));
	Action(TEXT("G"), NSLOCTEXT("Saltbound", "TagDrop", "Drop it"));
	return TagRoot;
}

void USaltSatchelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// D66: the tag's "use" keycap shows the player's Use key.
	const FString UseKey = SaltMenu::ShownKey(TEXT("E"));
	if (UseRow && UseRow->GetChildrenCount() > 0 && UseKey != UseRowKey)
	{
		UseRowKey = UseKey;
		UseRow->RemoveChildAt(0);
		UseRow->InsertChildAt(0, Keycap(WidgetTree, FText::FromString(UseKey), 30.f));
		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(UseRow->GetChildAt(0)->Slot))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
	}
	// The screen takes the mouse itself: hover, click and drag on the pockets (D58).
	SetVisibility(ESlateVisibility::Visible);
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			InventoryChanged = Satchel->OnChanged.AddUObject(this, &USaltSatchelWidget::Refresh);
		}
	}
	// D58 (owner: "a small but fast animation for opening the satchel"): it rises into place in 0.2 s, the tag swinging.
	OpenAge = 0.f;
	TagSwingRate += 55.f;
	ApplyOpening(Contents, Backdrop, BackdropBlur, 8.f, 0.f);
	Refresh();
}

void USaltSatchelWidget::NativeDestruct()
{
	EndDrag();
	ReturnTime = -1.f;
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			Satchel->OnChanged.Remove(InventoryChanged);
		}
	}
	Super::NativeDestruct();
}

FVector2D USaltSatchelWidget::PocketAt(int32 Index)
{
	return PocketCentre(Index);
}

int32 USaltSatchelWidget::SlotAt(FVector2D Design) const
{
	for (int32 I = 0; I < USaltInventoryComponent::Capacity; ++I)
	{
		// The pocket's stitched ring (radius 60) and a little more.
		if (FVector2D::DistSquared(Design, PocketCentre(I)) < FMath::Square(64.f))
		{
			return I;
		}
	}
	return INDEX_NONE;
}

int32 USaltSatchelWidget::DropOnSlot(int32 From, int32 To)
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	if (!Satchel || From == To || !SlotWidgets.IsValidIndex(From) || !SlotWidgets.IsValidIndex(To))
	{
		return 0;
	}
	const bool bSwap = !Satchel->GetSlots()[To].IsEmpty() && Satchel->GetSlots()[To].Id != Satchel->GetSlots()[From].Id;
	// The pocket it went to is chosen (the tag tells what lies there now).
	if (To != Selected)
	{
		Selected = To;
		TagSwingRate += (To % 2 ? -1.f : 1.f) * 70.f;
	}
	const int32 Moved = Satchel->MoveToSlot(From, Satchel, To);
	if (Moved > 0)
	{
		SlotWidgets[To]->Land();
		if (bSwap)
		{
			SlotWidgets[From]->Land();
		}
	}
	Refresh();
	return Moved;
}

void USaltSatchelWidget::EndDrag()
{
	if (Drag.bDragging && SlotWidgets.IsValidIndex(Drag.From) && ReturnTime < 0.f)
	{
		SlotWidgets[Drag.From]->SetLifted(false);
	}
	Drag = FDrag();
	Refresh();
}

void USaltSatchelWidget::UpdateDragVisual()
{
	if (!DragIcon)
	{
		return;
	}
	const bool bReturning = ReturnTime >= 0.f;
	const bool bShow = Drag.bDragging || bReturning;
	FVector2D Where = Drag.Pointer;
	float Lift = 1.f;
	if (bReturning)
	{
		// Let go off the pockets: it goes back where it came from (0.15 s, eased out) and settles there.
		const float U = 1.f - FMath::Square(1.f - FMath::Clamp(ReturnTime / 0.15f, 0.f, 1.f));
		Where = FMath::Lerp(ReturnFrom, PocketCentre(ReturnSlot) - FVector2D(0.f, 2.f), U);
		Lift = 1.f - U;
	}
	for (UImage* I : { DragShadow.Get(), DragIcon.Get() })
	{
		I->SetVisibility(bShow && I->GetBrush().GetResourceObject() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	DragCoin->SetVisibility(bShow && DragCount >= 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShow)
	{
		return;
	}
	const float Size = FMath::Lerp(86.f, 106.f, Lift);
	auto Move = [](UWidget* W, FVector2D P, float S)
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(W->Slot))
		{
			Slot->SetPosition(P - FVector2D(960.f, 540.f));
			if (S > 0.f)
			{
				Slot->SetSize(FVector2D(S, S));
			}
		}
	};
	Move(DragIcon, Where - FVector2D(0.f, 6.f * Lift), Size);
	Move(DragShadow, Where + FVector2D(3.f, 6.f) + FVector2D(6.f, 22.f) * Lift, Size);
	Move(DragCoin, Where + FVector2D(38.f, 36.f) * Size / 86.f, 0.f);
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
		SlotWidgets[I]->Show(Def, Def ? Stack.Count : 0, I == HighlightSlot());
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

void USaltSatchelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Opening (D58).
	if (OpenAge < OpenTime)
	{
		OpenAge = FMath::Min(OpenAge + InDeltaTime, OpenTime);
		ApplyOpening(Contents, Backdrop, BackdropBlur, 8.f, OpenAge / OpenTime);
	}
	// A dragged item let go off the pockets goes back to its own.
	if (ReturnTime >= 0.f)
	{
		ReturnTime += InDeltaTime;
		if (ReturnTime >= 0.15f)
		{
			ReturnTime = -1.f;
			if (SlotWidgets.IsValidIndex(ReturnSlot))
			{
				SlotWidgets[ReturnSlot]->SetLifted(false);
				SlotWidgets[ReturnSlot]->Land();
			}
		}
		UpdateDragVisual();
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
	FKey Key = InKeyEvent.GetKey();
	// D66: the player's satchel key closes it, their movement keys move the choice as the arrows do.
	if (SaltMenu::IsActionKey(Key, TEXT("Satchel")))
	{
		Key = EKeys::Tab;
	}
	const TPair<FName, FKey> MoveKeys[] = { { TEXT("MoveLeft"), EKeys::Left }, { TEXT("MoveRight"), EKeys::Right }, { TEXT("MoveForward"), EKeys::Up },
		{ TEXT("MoveBack"), EKeys::Down } };
	for (const TPair<FName, FKey>& M : MoveKeys)
	{
		if (Key == USaltInputConfig::KeyOf(M.Key))
		{
			Key = M.Value;
			break;
		}
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
	if (Is({ EKeys::Escape, EKeys::F10, EKeys::Gamepad_FaceButton_Right, EKeys::I, EKeys::Tab, EKeys::Gamepad_Special_Left }))
	{
		RequestClose();
	}
	else if (Is({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left }))
	{
		SelectSlot(Selected - 1);
	}
	else if (Is({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right }))
	{
		SelectSlot(Selected + 1);
	}
	else if (Is({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }))
	{
		SelectSlot(Selected - Columns);
	}
	else if (Is({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }))
	{
		SelectSlot(Selected + Columns);
	}
	else if (Is({ EKeys::R, EKeys::Gamepad_FaceButton_Top }))
	{
		if (SlotWidgets.IsValidIndex(Selected))
		{
			SlotWidgets[Selected]->Turn();
		}
	}
	else if (Is({ EKeys::G, EKeys::Gamepad_FaceButton_Left }))
	{
		DropSelected();
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

// --- Mouse: hover chooses a pocket, a press picks the item up, moving drags it, letting go puts it down (D58) ---------

bool USaltSatchelWidget::PointerDown(FVector2D P)
{
	const int32 I = SlotAt(P);
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	if (I == INDEX_NONE || !Satchel || ReturnTime >= 0.f)
	{
		return false;
	}
	SelectSlot(I);
	if (!Satchel->GetSlots().IsValidIndex(I) || Satchel->GetSlots()[I].IsEmpty())
	{
		return false;
	}
	Drag = FDrag();
	Drag.bPressed = true;
	Drag.From = I;
	Drag.PressAt = P;
	Drag.Pointer = P;
	return true;
}

void USaltSatchelWidget::PointerMove(FVector2D P)
{
	if (!Drag.bPressed)
	{
		// Hovering chooses the pocket under the mouse.
		const int32 I = SlotAt(P);
		if (I != INDEX_NONE && ReturnTime < 0.f)
		{
			SelectSlot(I);
		}
		return;
	}
	Drag.Pointer = P;
	if (!Drag.bDragging && FVector2D::Distance(P, Drag.PressAt) > 6.f)
	{
		// The item comes up out of its pocket and follows the mouse.
		const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
		const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
		const FSaltItemStack Stack = Satchel && Satchel->GetSlots().IsValidIndex(Drag.From) ? Satchel->GetSlots()[Drag.From] : FSaltItemStack();
		const FSaltItemDef* Def = Stack.IsEmpty() ? nullptr : SaltItems::Find(Stack.Id);
		Drag.bDragging = true;
		DragCount = Stack.Count;
		DragIcon->SetBrush(ImageBrush(ItemIcon(Def), FVector2D(96.f, 96.f)));
		DragShadow->SetBrush(ImageBrush(ItemIconShadow(Def), FVector2D(96.f, 96.f)));
		SetCoinCount(DragCoin, DragCoinText, DragCount);
		SlotWidgets[Drag.From]->SetLifted(true);
	}
	if (Drag.bDragging)
	{
		const int32 Target = SlotAt(P);
		if (Target != Drag.Target)
		{
			Drag.Target = Target;
			Refresh();
		}
		UpdateDragVisual();
	}
}

void USaltSatchelWidget::PointerUp(FVector2D P)
{
	if (!Drag.bPressed)
	{
		return;
	}
	const bool bWasDragging = Drag.bDragging;
	const int32 From = Drag.From;
	const int32 To = SlotAt(P);
	if (bWasDragging && (To == INDEX_NONE || To == From || DropOnSlot(From, To) <= 0))
	{
		// Let go off the pockets (or where it cannot go): it goes back (owner, D58: nothing is lost by a drag).
		ReturnTime = 0.f;
		ReturnFrom = P;
		ReturnSlot = From;
		Drag = FDrag();
		Refresh();
		UpdateDragVisual();
	}
	else
	{
		EndDrag();
		UpdateDragVisual();
	}
}

FReply USaltSatchelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	return PointerDown(DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition())) ? FReply::Handled().CaptureMouse(TakeWidget()) : FReply::Handled();
}

FReply USaltSatchelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bDrag = Drag.bPressed;
	PointerMove(DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()));
	return bDrag ? FReply::Handled() : Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USaltSatchelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !Drag.bPressed)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	PointerUp(DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()));
	return FReply::Handled().ReleaseMouseCapture();
}

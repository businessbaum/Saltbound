// Saltbound - a store's screen (D52, restyled D55, UI_Design.md section 6).

#include "UI/SaltStoreWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltMenuWidget.h"
#include "UI/SaltSatchelWidget.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

namespace
{
	// The layout at 1080p (section 6; the pictures are Tools/textures/ui_parts.js, keep the numbers in sync).
	// The satchel's dishes: radius 44, 6 x 4 at 225 + 114 col, 374 + 150 row; the item lies 2 px above the centre.
	constexpr float StoreDishRadius = 44.f;
	FVector2D StoreDish(int32 I) { return FVector2D(225.f + 114.f * (I % 6), 374.f + 150.f * (I / 6)); }
	// The crate's compartments: 5 x 2, each 136 x 161 from (1060, 482); the item lies at (68, 76.5) in it, its coin at
	// (102, 131).
	FVector2D CrateCell(int32 I) { return FVector2D(1060.f + 136.f * (I % 5), 482.f + 161.f * (I / 5)); }
	const FVector2D CrateCellSize(136.f, 161.f);
	const FVector2D CrateItemInCell(68.f, 76.5f);
	// The barrel's: on its floor (1400 + u, 580 + 0.7 v), 3 in the middle (radius 91, at 270 / 30 / 150 degrees) and 7
	// around (radius 236, from 270 degrees on, clockwise); the items there are 88 px.
	FVector2D BarrelItem(int32 I)
	{
		const bool bInner = I < 3;
		const float R = bInner ? 91.f : 236.f;
		const float Deg = bInner ? (I == 0 ? 270.f : I == 1 ? 30.f : 150.f) : 270.f + (I - 3) * 360.f / 7.f;
		const float A = FMath::DegreesToRadians(Deg);
		return FVector2D(1400.f + R * FMath::Cos(A), 580.f + 0.7f * R * FMath::Sin(A));
	}
	constexpr int32 StoreMaxSlots = 10;
	const FVector2D BarrelBox(124.f, 124.f);
	constexpr float BarrelItemSize = 88.f;

	// The flight (section 6): a quadratic arc in ~0.25 s, eased out; the item grows on the way as if lifted.
	constexpr float StoreFlightTime = 0.25f;
	constexpr float StorePathStep = 15.f;
	constexpr int32 StoreMaxDots = 120;

	FVector2D Bezier(const FVector2D& A, const FVector2D& Q, const FVector2D& B, float T)
	{
		return A * FMath::Square(1.f - T) + Q * (2.f * (1.f - T) * T) + B * (T * T);
	}

	void MoveTo(UWidget* W, FVector2D Design, FVector2D Size)
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

	FText CountOf(const TArray<FSaltItemStack>& Slots, int32 Capacity)
	{
		int32 Used = 0;
		for (const FSaltItemStack& S : Slots)
		{
			Used += S.IsEmpty() ? 0 : 1;
		}
		return FText::Format(NSLOCTEXT("Saltbound", "StoreCount", "{0} / {1}"), FText::AsNumber(Used), FText::AsNumber(Capacity));
	}
}

// --- A store's compartment ----------------------------------------------------------------------------------------

void USaltStoreSlotWidget::Setup(int32 InIndex, FVector2D InBoxSize, FVector2D InItemCentre, FVector2D InCoinOffset, float InItemSize)
{
	ItemSize = InItemSize;
	Index = InIndex;
	BoxSize = InBoxSize;
	ItemCentre = InItemCentre;
	CoinOffset = InCoinOffset;
}

void USaltStoreSlotWidget::Place(UWidget* W, FVector2D Centre, FVector2D Size)
{
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
	{
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(Centre);
		S->SetSize(Size);
	}
}

TSharedRef<SWidget> USaltStoreSlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(BoxSize.X);
		Box->SetHeightOverride(BoxSize.Y);
		WidgetTree->RootWidget = Box;
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		Box->AddChild(Root);
		auto Img = [this](const TCHAR* Texture, FVector2D Size, FVector2D Centre)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>();
			if (Texture)
			{
				I->SetBrush(ImageBrush(Tex(Texture), Size));
			}
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			Root->AddChildToCanvas(I);
			Place(I, Centre, Size);
			return I;
		};
		const float K = ItemSize / 96.f;
		Glow = Img(TEXT("T_UI_SlotGlow"), FVector2D(180.f, 180.f) * K, ItemCentre);
		ShardGlow = Img(TEXT("T_UI_ShardGlow"), FVector2D(200.f, 200.f) * K, ItemCentre);
		// The design's drop shadow: 4 px right, 8 px down, soft.
		IconShadow = Img(nullptr, FVector2D(ItemSize, ItemSize), ItemCentre + FVector2D(4.f, 8.f) * K);
		IconImage = Img(nullptr, FVector2D(ItemSize, ItemSize), ItemCentre);
		IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UTextBlock* CoinLabel = nullptr;
		Coin = CountCoin(WidgetTree, &CoinLabel, 17.f);
		CoinText = CoinLabel;
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Coin))
		{
			S->SetAutoSize(true);
			S->SetAlignment(FVector2D(23.f / 48.f, 22.f / 48.f));
			S->SetPosition(ItemCentre + CoinOffset);
		}
		// The store screen takes the mouse (hover, click, drag: D58).
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Show(nullptr, 0, false);
	}
	return Super::RebuildWidget();
}

void USaltStoreSlotWidget::Show(const FSaltItemDef* Item, int32 Count, bool bInHovered)
{
	if (!IconImage)
	{
		return;
	}
	bHovered = bInHovered;
	bAncient = Item && Item->Id == TEXT("AncientShard");
	UTexture2D* Icon = ItemIcon(Item);
	UTexture2D* Shadow = ItemIconShadow(Item);
	IconImage->SetBrush(ImageBrush(Icon, FVector2D(ItemSize, ItemSize)));
	IconShadow->SetBrush(ImageBrush(Shadow, FVector2D(ItemSize, ItemSize)));
	IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	IconShadow->SetVisibility(Icon && Shadow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ShardGlow->SetVisibility(bAncient ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetCoinCount(Coin, CoinText, Item ? Count : 0);
}

void USaltStoreSlotWidget::Land()
{
	LandTime = 0.f;
}

void USaltStoreSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IconImage)
	{
		return;
	}
	Clock += InDeltaTime;
	HoverAlpha = FMath::FInterpConstantTo(HoverAlpha, bHovered ? 1.f : 0.f, InDeltaTime, 1.f / 0.12f);
	Glow->SetRenderOpacity(0.85f * FMath::InterpEaseInOut(0.f, 1.f, HoverAlpha, 2.f));
	if (bAncient)
	{
		ShardGlow->SetRenderOpacity(0.7f + 0.3f * FMath::Sin(Clock * TWO_PI / 2.5f));
	}
	// Landing: it drops in a little large and settles (0.18 s).
	float Scale = 1.f;
	if (LandTime >= 0.f)
	{
		LandTime += InDeltaTime;
		const float U = FMath::Clamp(LandTime / 0.18f, 0.f, 1.f);
		Scale = 1.f + 0.12f * FMath::Square(1.f - U);
		LandTime = U >= 1.f ? -1.f : LandTime;
	}
	IconImage->SetRenderScale(FVector2D(Scale, Scale));
	// Being dragged away (D58): a faint trace stays, the coin goes with the item.
	IconImage->SetRenderOpacity(bLifted ? 0.25f : 1.f);
	IconShadow->SetRenderOpacity(bLifted ? 0.f : 1.f);
	Coin->SetRenderOpacity(bLifted ? 0.f : 1.f);
}

// --- The store screen ---------------------------------------------------------------------------------------------

USaltStoreWidget::USaltStoreWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void USaltStoreWidget::SetInventories(USaltInventoryComponent* InSatchel, USaltInventoryComponent* InStore)
{
	// Set before the screen is shown (it binds to both when constructed, and is made anew for every store).
	Satchel = InSatchel;
	Store = InStore;
	bCrate = !InStore || InStore->GetStorageId() != FName(TEXT("Boat_Barrel"));
}

int32 USaltStoreWidget::SlotCount(bool bStoreSide) const
{
	if (!bStoreSide)
	{
		return USaltInventoryComponent::Capacity;
	}
	return FMath::Min(Store.IsValid() ? Store->GetCapacity() : StoreMaxSlots, StoreMaxSlots);
}

float USaltStoreWidget::ItemSize(bool bStoreSide) const
{
	return bStoreSide ? (bCrate ? 96.f : BarrelItemSize) : 86.f * StoreDishRadius / 50.f;
}

FVector2D USaltStoreWidget::SlotCentre(bool bStoreSide, int32 Index) const
{
	if (!bStoreSide)
	{
		return StoreDish(Index) - FVector2D(0.f, 2.f * StoreDishRadius / 50.f);
	}
	return bCrate ? CrateCell(Index) + CrateItemInCell : BarrelItem(Index);
}

TSharedRef<SWidget> USaltStoreWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Screen = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Screen;
		auto FullScreen = [](UCanvasPanel* Into, UWidget* W)
		{
			UCanvasPanelSlot* S = Into->AddChildToCanvas(W);
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		};
		auto Picture = [this](const TCHAR* Texture, FVector2D Size)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>();
			I->SetBrush(ImageBrush(Tex(Texture), Size));
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			return I;
		};
		// The world behind: blurred, darkened, a vignette (as the satchel; they fade in as it opens, D58).
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
		FullScreen(Behind, Picture(TEXT("T_UI_Vignette"), FVector2D(256.f, 144.f)));
		// The satchel and the store (they rise into place as the screen opens).
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		FullScreen(Screen, Canvas);
		Contents = Canvas;

		// Left, the satchel: the panel with its dishes, "N / 24" debossed (right-aligned at x 842, baseline 272), the pockets.
		At(Canvas, Picture(TEXT("T_UI_StoreSatchel"), FVector2D(900.f, 910.f)), FVector2D(60.f, 130.f), FVector2D(900.f, 910.f));
		SatchelCount = Deboss(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 36.f, 0.f, ETextJustify::Right);
		At(Canvas, SatchelCount, FVector2D(842.f, 272.f - 0.905f * 36.f), FVector2D::ZeroVector, FVector2D(1.f, 0.f));
		SatchelSlots.Reset();
		for (int32 I = 0; I < USaltInventoryComponent::Capacity; ++I)
		{
			USaltItemSlotWidget* W = CreateWidget<USaltItemSlotWidget>(this, USaltItemSlotWidget::StaticClass());
			W->Setup(I, StoreDishRadius);
			SatchelSlots.Add(W);
			At(Canvas, W, StoreDish(I), FVector2D(4.f, 4.f) * StoreDishRadius, FVector2D(0.5f, 0.5f));
		}

		// Right, the store: the open crate or the barrel, its count burned in under its name, the compartments.
		if (bCrate)
		{
			At(Canvas, Picture(TEXT("T_UI_StoreCrate"), FVector2D(900.f, 940.f)), FVector2D(950.f, 40.f), FVector2D(900.f, 940.f));
		}
		else
		{
			At(Canvas, Picture(TEXT("T_UI_StoreBarrel"), FVector2D(900.f, 1000.f)), FVector2D(950.f, 10.f), FVector2D(900.f, 1000.f));
		}
		StoreCount = Burn(WidgetTree, FText::GetEmpty(), ESaltFace::FellItalic, 34.f, ETextJustify::Center);
		At(Canvas, StoreCount, FVector2D(1400.f, (bCrate ? 288.f : 212.f) - 0.905f * 34.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f));
		StoreSlots.Reset();
		for (int32 I = 0; I < SlotCount(true); ++I)
		{
			USaltStoreSlotWidget* W = CreateWidget<USaltStoreSlotWidget>(this, USaltStoreSlotWidget::StaticClass());
			if (bCrate)
			{
				W->Setup(I, CrateCellSize, CrateItemInCell, FVector2D(102.f, 131.f) - CrateItemInCell);
				At(Canvas, W, CrateCell(I), CrateCellSize);
			}
			else
			{
				W->Setup(I, BarrelBox, BarrelBox * 0.5f, FVector2D(31.f, 35.f), BarrelItemSize);
				At(Canvas, W, BarrelItem(I) - BarrelBox * 0.5f, BarrelBox);
			}
			StoreSlots.Add(W);
		}

		// Along the bottom: "Click: move the stack . Shift-click: move one . [E] close the lid".
		At(Canvas, Picture(TEXT("T_UI_StoreHints"), FVector2D(860.f, 80.f)), FVector2D(540.f, 1000.f), FVector2D(860.f, 80.f));

		// On top, the flight (and a dragged item, D58): the dotted path, the target's outline, the item with its shadow and its
		// coin.
		FlightLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
		FlightLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
		FullScreen(Screen, FlightLayer);
		PathDots.Reset();
		for (int32 I = 0; I < StoreMaxDots; ++I)
		{
			UImage* Dot = Picture(TEXT("T_UI_Dot"), FVector2D(12.f, 12.f));
			Dot->SetColorAndOpacity(SRGB(245, 232, 205));
			Dot->SetVisibility(ESlateVisibility::Collapsed);
			At(FlightLayer, Dot, FVector2D::ZeroVector, FVector2D(12.f, 12.f), FVector2D(0.5f, 0.5f));
			PathDots.Add(Dot);
		}
		TargetOutline = Picture(bCrate ? TEXT("T_UI_StoreTargetRect") : TEXT("T_UI_StoreTargetRound"), FVector2D(160.f, 160.f));
		TargetOutline->SetVisibility(ESlateVisibility::Collapsed);
		At(FlightLayer, TargetOutline, FVector2D::ZeroVector, FVector2D(160.f, 160.f), FVector2D(0.5f, 0.5f));
		FlyShadow = Picture(TEXT("T_UI_Dot"), FVector2D(96.f, 96.f));
		FlyIcon = Picture(TEXT("T_UI_Dot"), FVector2D(96.f, 96.f));
		for (UImage* I : { FlyShadow.Get(), FlyIcon.Get() })
		{
			I->SetVisibility(ESlateVisibility::Collapsed);
			At(FlightLayer, I, FVector2D::ZeroVector, FVector2D(96.f, 96.f), FVector2D(0.5f, 0.5f));
		}
		UTextBlock* CoinLabel = nullptr;
		FlyCoin = CountCoin(WidgetTree, &CoinLabel, 17.f);
		FlyCoinText = CoinLabel;
		FlyCoin->SetVisibility(ESlateVisibility::Collapsed);
		At(FlightLayer, FlyCoin, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(23.f / 48.f, 22.f / 48.f));
	}
	return Super::RebuildWidget();
}

void USaltStoreWidget::Bind(bool bBind)
{
	USaltInventoryComponent* S = Satchel.Get();
	USaltInventoryComponent* T = Store.Get();
	if (bBind)
	{
		if (S)
		{
			SatchelChanged = S->OnChanged.AddUObject(this, &USaltStoreWidget::Refresh);
		}
		if (T)
		{
			StoreChanged = T->OnChanged.AddUObject(this, &USaltStoreWidget::Refresh);
		}
	}
	else
	{
		if (S)
		{
			S->OnChanged.Remove(SatchelChanged);
		}
		if (T)
		{
			T->OnChanged.Remove(StoreChanged);
		}
	}
}

void USaltStoreWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The screen takes the mouse itself: hover, click and drag (D58).
	SetVisibility(ESlateVisibility::Visible);
	Bind(true);
	// D58: it opens as the satchel does (rising into place in 0.2 s).
	OpenAge = 0.f;
	ApplyOpening(Contents, Backdrop, BackdropBlur, 8.f, 0.f);
	Refresh();
}

void USaltStoreWidget::NativeDestruct()
{
	Drag = FDrag();
	Bind(false);
	Super::NativeDestruct();
}

void USaltStoreWidget::Refresh()
{
	// While something flies, the side it flies to shows what it held before (the item arrives when it lands).
	auto Data = [this](bool bStoreSide)
	{
		if (Flight.bActive && Flight.bToStore == bStoreSide)
		{
			return Flight.TargetBefore;
		}
		const USaltInventoryComponent* Inv = bStoreSide ? Store.Get() : Satchel.Get();
		return Inv ? Inv->GetSlots() : TArray<FSaltItemStack>();
	};
	const TArray<FSaltItemStack> InSatchel = Data(false);
	const TArray<FSaltItemStack> InStore = Data(true);
	auto Def = [](const TArray<FSaltItemStack>& Slots, int32 I) -> const FSaltItemDef*
	{
		return Slots.IsValidIndex(I) && !Slots[I].IsEmpty() ? SaltItems::Find(Slots[I].Id) : nullptr;
	};
	for (int32 I = 0; I < SatchelSlots.Num(); ++I)
	{
		SatchelSlots[I]->Show(Def(InSatchel, I), InSatchel.IsValidIndex(I) ? InSatchel[I].Count : 0, bCursorShown && !bCursorStore && CursorSlot == I);
	}
	for (int32 I = 0; I < StoreSlots.Num(); ++I)
	{
		StoreSlots[I]->Show(Def(InStore, I), InStore.IsValidIndex(I) ? InStore[I].Count : 0, bCursorShown && bCursorStore && CursorSlot == I);
	}
	SetLayeredText(SatchelCount, CountOf(InSatchel, USaltInventoryComponent::Capacity));
	SetLayeredText(StoreCount, CountOf(InStore, Store.IsValid() ? Store->GetCapacity() : StoreMaxSlots));
}

FText USaltStoreWidget::GetSatchelCountText() const
{
	return Satchel.IsValid() ? CountOf(Satchel->GetSlots(), USaltInventoryComponent::Capacity) : FText::GetEmpty();
}

FText USaltStoreWidget::GetStoreCountText() const
{
	return Store.IsValid() ? CountOf(Store->GetSlots(), Store->GetCapacity()) : FText::GetEmpty();
}

int32 USaltStoreWidget::ClickSatchelSlot(int32 Index, bool bOne)
{
	return Click(false, Index, bOne);
}

int32 USaltStoreWidget::ClickStoreSlot(int32 Index, bool bOne)
{
	return Click(true, Index, bOne);
}

int32 USaltStoreWidget::Click(bool bFromStore, int32 Index, bool bOne)
{
	USaltInventoryComponent* From = bFromStore ? Store.Get() : Satchel.Get();
	USaltInventoryComponent* To = bFromStore ? Satchel.Get() : Store.Get();
	// Not while something is being dragged (or going back after a drag).
	if (!From || !To || !From->GetSlots().IsValidIndex(Index) || From->GetSlots()[Index].IsEmpty() || Drag.bDragging || Drag.ReturnTime >= 0.f)
	{
		return 0;
	}
	if (Flight.bActive)
	{
		Land();
	}
	const FSaltItemStack Stack = From->GetSlots()[Index];
	// The flight is set up before the move, so the refresh the move causes still shows the target as it was.
	Flight = FFlight();
	Flight.bActive = true;
	Flight.bToStore = !bFromStore;
	Flight.TargetBefore = To->GetSlots();
	int32 ToSlot = INDEX_NONE;
	const int32 Moved = From->MoveStack(Index, To, bOne ? 1 : MAX_int32, &ToSlot);
	if (Moved <= 0 || ToSlot == INDEX_NONE || ToSlot >= SlotCount(!bFromStore))
	{
		Flight = FFlight();
		Refresh();
		return Moved;
	}
	Flight.ToSlot = ToSlot;
	Flight.Count = Moved;
	Flight.Item = SaltItems::Find(Stack.Id);
	Flight.From = SlotCentre(bFromStore, Index);
	Flight.To = SlotCentre(!bFromStore, ToSlot);
	Flight.FromSize = ItemSize(bFromStore);
	Flight.ToSize = ItemSize(!bFromStore);
	Flight.PathFade = 1.f;
	// The arc rises soon after it leaves (as in the design: the control point a fifth of the way along, lifted).
	const float Dist = FVector2D::Distance(Flight.From, Flight.To);
	Flight.Control = Flight.From + (Flight.To - Flight.From) * 0.22f - FVector2D(0.f, FMath::Max(80.f, 0.17f * Dist));
	// Dots every 15 px along the arc.
	PathPoints.Reset();
	PathParams.Reset();
	FVector2D Prev = Flight.From;
	float Acc = StorePathStep * 0.5f;
	for (int32 I = 1; I <= 240 && PathPoints.Num() < StoreMaxDots; ++I)
	{
		const float T = I / 240.f;
		const FVector2D P = Bezier(Flight.From, Flight.Control, Flight.To, T);
		Acc += FVector2D::Distance(P, Prev);
		Prev = P;
		if (Acc >= StorePathStep)
		{
			Acc -= StorePathStep;
			PathPoints.Add(P);
			PathParams.Add(T);
		}
	}
	for (int32 I = 0; I < PathDots.Num(); ++I)
	{
		const bool bUsed = PathPoints.IsValidIndex(I);
		PathDots[I]->SetVisibility(bUsed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bUsed)
		{
			MoveTo(PathDots[I], PathPoints[I], FVector2D::ZeroVector);
		}
	}
	// The target: the compartment's outline (crate), or a ring round the item (barrel: radius 56; satchel dish: 50).
	if (Flight.bToStore && bCrate)
	{
		MoveTo(TargetOutline, CrateCell(ToSlot) + CrateCellSize * 0.5f, FVector2D(160.f, 185.f));
	}
	else
	{
		const float D = Flight.bToStore ? 160.f : 160.f * 50.f / 56.f;
		MoveTo(TargetOutline, Flight.bToStore ? Flight.To : StoreDish(ToSlot), FVector2D(D, D));
	}
	UTexture2D* Icon = ItemIcon(Flight.Item);
	UTexture2D* Shadow = ItemIconShadow(Flight.Item);
	FlyIcon->SetBrush(ImageBrush(Icon, FVector2D(96.f, 96.f)));
	FlyShadow->SetBrush(ImageBrush(Shadow, FVector2D(96.f, 96.f)));
	FlyIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	FlyShadow->SetVisibility(Shadow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetCoinCount(FlyCoin, FlyCoinText, Moved);
	Refresh();
	UpdateFlight(0.f);
	return Moved;
}

void USaltStoreWidget::Land()
{
	const bool bToStore = Flight.bToStore;
	const int32 LandedIn = Flight.ToSlot;
	Flight.bActive = false;
	Flight.Progress = 1.f;
	Flight.TargetBefore.Reset();
	FlyIcon->SetVisibility(ESlateVisibility::Collapsed);
	FlyShadow->SetVisibility(ESlateVisibility::Collapsed);
	FlyCoin->SetVisibility(ESlateVisibility::Collapsed);
	Refresh();
	if (bToStore && StoreSlots.IsValidIndex(LandedIn))
	{
		StoreSlots[LandedIn]->Land();
	}
}

void USaltStoreWidget::UpdateFlight(float Dt)
{
	if (!FlyIcon)
	{
		return;
	}
	if (Flight.bActive)
	{
		Flight.Time += Dt;
		const float T = FMath::Clamp(Flight.Time / StoreFlightTime, 0.f, 1.f);
		Flight.Progress = FlightHold >= 0.f ? FMath::Clamp(FlightHold, 0.f, 1.f) : 1.f - FMath::Square(1.f - T);
		const float E = Flight.Progress;
		const FVector2D P = Bezier(Flight.From, Flight.Control, Flight.To, E);
		const float Lift = FMath::Sin(PI * E);
		const float Size = FMath::Lerp(Flight.FromSize, Flight.ToSize, E) * (1.f + 0.55f * Lift);
		MoveTo(FlyIcon, P, FVector2D(Size, Size));
		MoveTo(FlyShadow, P + FVector2D(4.f, 8.f) + FVector2D(6.f, 32.f) * Lift, FVector2D(Size, Size));
		FlyShadow->SetRenderOpacity(1.f - 0.15f * Lift);
		MoveTo(FlyCoin, P + FVector2D(48.f, 48.f) * Size / 132.f, FVector2D::ZeroVector);
		Flight.OutlineFade = FMath::Min(1.f, Flight.OutlineFade + (Dt > 0.f ? Dt / 0.06f : 1.f));
		if (T >= 1.f && FlightHold < 0.f)
		{
			Land();
		}
	}
	else
	{
		Flight.OutlineFade = FMath::Max(0.f, Flight.OutlineFade - Dt / 0.15f);
		Flight.PathFade = FMath::Max(0.f, Flight.PathFade - Dt / 0.35f);
	}
	// A drag shows its own target (UpdateDrag).
	if (!Drag.bDragging)
	{
		TargetOutline->SetRenderOpacity(Flight.OutlineFade);
		TargetOutline->SetVisibility(Flight.OutlineFade > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// The path: 70 %, a little fainter behind the item, and all of it fading after the landing.
	for (int32 I = 0; I < PathPoints.Num() && I < PathDots.Num(); ++I)
	{
		const float Behind = FMath::Max(0.f, Flight.Progress - PathParams[I]);
		const float A = 0.7f * Flight.PathFade * FMath::Clamp(1.f - Behind * 0.8f, 0.6f, 1.f);
		PathDots[I]->SetRenderOpacity(A);
		PathDots[I]->SetVisibility(A > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USaltStoreWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (OpenAge < OpenTime)
	{
		OpenAge = FMath::Min(OpenAge + InDeltaTime, OpenTime);
		ApplyOpening(Contents, Backdrop, BackdropBlur, 8.f, OpenAge / OpenTime);
	}
	UpdateFlight(InDeltaTime);
	// A dragged thing let go off the slots goes back where it came from (0.15 s).
	if (Drag.ReturnTime >= 0.f)
	{
		Drag.ReturnTime += InDeltaTime;
		if (Drag.ReturnTime >= 0.15f)
		{
			SetLifted(Drag.bFromStore, Drag.From, false);
			if (Drag.bFromStore && StoreSlots.IsValidIndex(Drag.From))
			{
				StoreSlots[Drag.From]->Land();
			}
			else if (!Drag.bFromStore && SatchelSlots.IsValidIndex(Drag.From))
			{
				SatchelSlots[Drag.From]->Land();
			}
			Drag = FDrag();
			FlyIcon->SetVisibility(ESlateVisibility::Collapsed);
			FlyShadow->SetVisibility(ESlateVisibility::Collapsed);
			FlyCoin->SetVisibility(ESlateVisibility::Collapsed);
		}
		UpdateDrag();
	}
}

// --- Drag and drop (D58) ------------------------------------------------------------------------------------------

bool USaltStoreWidget::SlotAt(FVector2D Design, bool& bOutStore, int32& OutIndex) const
{
	for (int32 I = 0; I < SlotCount(false); ++I)
	{
		if (FVector2D::DistSquared(Design, StoreDish(I)) < FMath::Square(56.f))
		{
			bOutStore = false;
			OutIndex = I;
			return true;
		}
	}
	for (int32 I = 0; I < SlotCount(true); ++I)
	{
		const bool bIn = bCrate ? (Design.X >= CrateCell(I).X && Design.Y >= CrateCell(I).Y && Design.X < CrateCell(I).X + CrateCellSize.X
			&& Design.Y < CrateCell(I).Y + CrateCellSize.Y) : FVector2D::DistSquared(Design, BarrelItem(I)) < FMath::Square(60.f);
		if (bIn)
		{
			bOutStore = true;
			OutIndex = I;
			return true;
		}
	}
	return false;
}

void USaltStoreWidget::SetLifted(bool bStoreSide, int32 Index, bool bLifted)
{
	if (bStoreSide && StoreSlots.IsValidIndex(Index))
	{
		StoreSlots[Index]->SetLifted(bLifted);
	}
	else if (!bStoreSide && SatchelSlots.IsValidIndex(Index))
	{
		SatchelSlots[Index]->SetLifted(bLifted);
	}
}

void USaltStoreWidget::PlaceTarget(bool bStoreSide, int32 Index)
{
	// As a flight's target: the compartment's outline (crate), or a ring round the item (barrel: radius 56; dish: 50).
	if (bStoreSide && bCrate)
	{
		MoveTo(TargetOutline, CrateCell(Index) + CrateCellSize * 0.5f, FVector2D(160.f, 185.f));
	}
	else
	{
		const float D = bStoreSide ? 160.f : 160.f * 50.f / 56.f;
		MoveTo(TargetOutline, bStoreSide ? BarrelItem(Index) : StoreDish(Index), FVector2D(D, D));
	}
}

int32 USaltStoreWidget::DropOnSlot(bool bFromStore, int32 From, bool bToStore, int32 To, bool bOne)
{
	USaltInventoryComponent* FromInv = bFromStore ? Store.Get() : Satchel.Get();
	USaltInventoryComponent* ToInv = bToStore ? Store.Get() : Satchel.Get();
	if (!FromInv || !ToInv || To < 0 || To >= SlotCount(bToStore) || !FromInv->GetSlots().IsValidIndex(From) || !ToInv->GetSlots().IsValidIndex(To))
	{
		return 0;
	}
	if (Flight.bActive)
	{
		Land();
	}
	const bool bSwap = !ToInv->GetSlots()[To].IsEmpty() && ToInv->GetSlots()[To].Id != FromInv->GetSlots()[From].Id;
	const int32 Moved = FromInv->MoveToSlot(From, ToInv, To, bOne ? 1 : MAX_int32);
	if (Moved > 0)
	{
		auto Settle = [this](bool bStoreSide, int32 Index)
		{
			if (bStoreSide && StoreSlots.IsValidIndex(Index))
			{
				StoreSlots[Index]->Land();
			}
			else if (!bStoreSide && SatchelSlots.IsValidIndex(Index))
			{
				SatchelSlots[Index]->Land();
			}
		};
		Settle(bToStore, To);
		if (bSwap)
		{
			Settle(bFromStore, From);
		}
		SetCursor(bToStore, To);
	}
	Refresh();
	return Moved;
}

void USaltStoreWidget::UpdateDrag()
{
	const bool bReturning = Drag.ReturnTime >= 0.f;
	if (!Drag.bDragging && !bReturning)
	{
		return;
	}
	// The item follows the pointer lifted above the leather and the wood (larger, its shadow further below), with its coin;
	// going back, it sinks into its slot.
	FVector2D Where = Drag.Pointer;
	float Lift = 1.f;
	if (bReturning)
	{
		const float U = 1.f - FMath::Square(1.f - FMath::Clamp(Drag.ReturnTime / 0.15f, 0.f, 1.f));
		Where = FMath::Lerp(Drag.ReturnFrom, SlotCentre(Drag.bFromStore, Drag.From), U);
		Lift = 1.f - U;
	}
	const float Size = ItemSize(Drag.bFromStore) * (1.f + 0.15f * Lift);
	MoveTo(FlyIcon, Where - FVector2D(0.f, 6.f * Lift), FVector2D(Size, Size));
	MoveTo(FlyShadow, Where + FVector2D(4.f, 8.f) + FVector2D(6.f, 22.f) * Lift, FVector2D(Size, Size));
	FlyShadow->SetRenderOpacity(1.f - 0.15f * Lift);
	MoveTo(FlyCoin, Where + FVector2D(48.f, 48.f) * Size / 132.f, FVector2D::ZeroVector);
	const bool bTarget = Drag.bDragging && Drag.Target != INDEX_NONE && !(Drag.bTargetStore == Drag.bFromStore && Drag.Target == Drag.From);
	if (bTarget)
	{
		PlaceTarget(Drag.bTargetStore, Drag.Target);
	}
	TargetOutline->SetRenderOpacity(1.f);
	TargetOutline->SetVisibility(bTarget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool USaltStoreWidget::PointerDown(FVector2D P, bool bShift)
{
	bool bStoreSide = false;
	int32 I = INDEX_NONE;
	if (Drag.ReturnTime >= 0.f || !SlotAt(P, bStoreSide, I))
	{
		return false;
	}
	SetCursor(bStoreSide, I);
	const USaltInventoryComponent* Inv = bStoreSide ? Store.Get() : Satchel.Get();
	if (!Inv || !Inv->GetSlots().IsValidIndex(I) || Inv->GetSlots()[I].IsEmpty())
	{
		return false;
	}
	Drag = FDrag();
	Drag.bPressed = true;
	Drag.bOne = bShift;
	Drag.bFromStore = bStoreSide;
	Drag.From = I;
	Drag.PressAt = P;
	Drag.Pointer = P;
	return true;
}

void USaltStoreWidget::PointerMove(FVector2D P)
{
	bool bStoreSide = false;
	int32 I = INDEX_NONE;
	const bool bOnSlot = SlotAt(P, bStoreSide, I);
	if (!Drag.bPressed)
	{
		// Hovering moves the cursor to the slot under the mouse.
		if (bOnSlot && Drag.ReturnTime < 0.f)
		{
			SetCursor(bStoreSide, I);
		}
		return;
	}
	Drag.Pointer = P;
	if (!Drag.bDragging && FVector2D::Distance(P, Drag.PressAt) > 6.f)
	{
		// The thing comes up out of its slot and follows the mouse (Shift held at the press: one of the stack).
		if (Flight.bActive)
		{
			Land();
		}
		const USaltInventoryComponent* Inv = Drag.bFromStore ? Store.Get() : Satchel.Get();
		const FSaltItemStack Stack = Inv && Inv->GetSlots().IsValidIndex(Drag.From) ? Inv->GetSlots()[Drag.From] : FSaltItemStack();
		const FSaltItemDef* Def = Stack.IsEmpty() ? nullptr : SaltItems::Find(Stack.Id);
		Drag.bDragging = true;
		UTexture2D* Icon = ItemIcon(Def);
		UTexture2D* Shadow = ItemIconShadow(Def);
		FlyIcon->SetBrush(ImageBrush(Icon, FVector2D(96.f, 96.f)));
		FlyShadow->SetBrush(ImageBrush(Shadow, FVector2D(96.f, 96.f)));
		FlyIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		FlyShadow->SetVisibility(Shadow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		SetCoinCount(FlyCoin, FlyCoinText, Drag.bOne ? 1 : Stack.Count);
		// The whole stack lifts out; one of several leaves the rest lying there.
		if (!Drag.bOne || Stack.Count <= 1)
		{
			SetLifted(Drag.bFromStore, Drag.From, true);
		}
	}
	if (Drag.bDragging)
	{
		Drag.bTargetStore = bStoreSide;
		Drag.Target = bOnSlot ? I : INDEX_NONE;
		if (bOnSlot)
		{
			SetCursor(bStoreSide, I);
		}
		UpdateDrag();
	}
}

void USaltStoreWidget::PointerUp(FVector2D P, bool bShift)
{
	if (!Drag.bPressed)
	{
		return;
	}
	bool bStoreSide = false;
	int32 I = INDEX_NONE;
	const bool bOnSlot = SlotAt(P, bStoreSide, I);
	if (!Drag.bDragging)
	{
		// A click: the stack (Shift: one) goes to the other side, flying there.
		const bool bFromStore = Drag.bFromStore;
		const int32 From = Drag.From;
		Drag = FDrag();
		Click(bFromStore, From, bShift);
		return;
	}
	const bool bSame = bOnSlot && bStoreSide == Drag.bFromStore && I == Drag.From;
	if (bOnSlot && !bSame && DropOnSlot(Drag.bFromStore, Drag.From, bStoreSide, I, Drag.bOne) > 0)
	{
		SetLifted(Drag.bFromStore, Drag.From, false);
		Drag = FDrag();
		FlyIcon->SetVisibility(ESlateVisibility::Collapsed);
		FlyShadow->SetVisibility(ESlateVisibility::Collapsed);
		FlyCoin->SetVisibility(ESlateVisibility::Collapsed);
		TargetOutline->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// Let go off the slots (or where it cannot go): it goes back (owner, D58: nothing is lost by a drag).
		Drag.bDragging = false;
		Drag.bPressed = false;
		Drag.ReturnTime = 0.f;
		Drag.ReturnFrom = P;
		TargetOutline->SetVisibility(ESlateVisibility::Collapsed);
		UpdateDrag();
	}
}

FReply USaltStoreWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	return PointerDown(DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()), InMouseEvent.IsShiftDown())
		? FReply::Handled().CaptureMouse(TakeWidget()) : FReply::Handled();
}

FReply USaltStoreWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bDrag = Drag.bPressed;
	PointerMove(DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()));
	return bDrag ? FReply::Handled() : Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USaltStoreWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !Drag.bPressed)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	PointerUp(DesignPoint(InGeometry, InMouseEvent.GetScreenSpacePosition()), InMouseEvent.IsShiftDown());
	return FReply::Handled().ReleaseMouseCapture();
}

void USaltStoreWidget::SetCursor(bool bStoreSide, int32 Index)
{
	if (Index < 0 || Index >= SlotCount(bStoreSide))
	{
		return;
	}
	if (!bCursorShown || bCursorStore != bStoreSide || CursorSlot != Index)
	{
		bCursorShown = true;
		bCursorStore = bStoreSide;
		CursorSlot = Index;
		Refresh();
	}
}

void USaltStoreWidget::MoveCursor(FVector2D Direction)
{
	if (!bCursorShown)
	{
		SetCursor(false, 0);
		return;
	}
	const FVector2D Dir = Direction.GetSafeNormal();
	const FVector2D From = SlotCentre(bCursorStore, CursorSlot);
	float Best = TNumericLimits<float>::Max();
	bool bBestStore = bCursorStore;
	int32 BestSlot = INDEX_NONE;
	// The nearest slot within about 50 degrees of the direction, on either side.
	for (const bool bSide : { false, true })
	{
		for (int32 I = 0; I < SlotCount(bSide); ++I)
		{
			const FVector2D D = SlotCentre(bSide, I) - From;
			const float Along = FVector2D::DotProduct(D, Dir);
			const float Across = FMath::Abs(FVector2D::CrossProduct(D, Dir));
			if (Along < 1.f || Across > Along * 1.2f)
			{
				continue;
			}
			const float Score = Along + 2.f * Across;
			if (Score < Best)
			{
				Best = Score;
				bBestStore = bSide;
				BestSlot = I;
			}
		}
	}
	if (BestSlot != INDEX_NONE)
	{
		SetCursor(bBestStore, BestSlot);
	}
}

void USaltStoreWidget::Close()
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
	{
		PC->CloseStore();
	}
}

FReply USaltStoreWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::E || Key == EKeys::Escape || Key == EKeys::Tab || Key == EKeys::I || Key == EKeys::F10 || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Close();
		return FReply::Handled();
	}
	struct FMove { FKey Key; FVector2D Dir; };
	const FMove Moves[] = {
		{ EKeys::Left, FVector2D(-1.f, 0.f) }, { EKeys::Right, FVector2D(1.f, 0.f) }, { EKeys::Up, FVector2D(0.f, -1.f) }, { EKeys::Down, FVector2D(0.f, 1.f) },
		{ EKeys::A, FVector2D(-1.f, 0.f) }, { EKeys::D, FVector2D(1.f, 0.f) }, { EKeys::W, FVector2D(0.f, -1.f) }, { EKeys::S, FVector2D(0.f, 1.f) },
		{ EKeys::Gamepad_DPad_Left, FVector2D(-1.f, 0.f) }, { EKeys::Gamepad_DPad_Right, FVector2D(1.f, 0.f) },
		{ EKeys::Gamepad_DPad_Up, FVector2D(0.f, -1.f) }, { EKeys::Gamepad_DPad_Down, FVector2D(0.f, 1.f) },
	};
	for (const FMove& M : Moves)
	{
		if (Key == M.Key)
		{
			MoveCursor(M.Dir);
			return FReply::Handled();
		}
	}
	const bool bStack = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
	const bool bOne = Key == EKeys::Gamepad_FaceButton_Left;
	if ((bStack || bOne) && bCursorShown)
	{
		Click(bCursorStore, CursorSlot, bOne || InKeyEvent.IsShiftDown());
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

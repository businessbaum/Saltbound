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

void USaltStoreSlotWidget::Setup(int32 InIndex, TFunction<void(int32)> InOnClicked, TFunction<void(int32)> InOnHovered, FVector2D InBoxSize,
	FVector2D InItemCentre, FVector2D InCoinOffset, float InItemSize)
{
	ItemSize = InItemSize;
	Index = InIndex;
	OnClickedFn = MoveTemp(InOnClicked);
	OnHoveredFn = MoveTemp(InOnHovered);
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
		USaltMenuButton* Button = WidgetTree->ConstructWidget<USaltMenuButton>();
		FButtonStyle Style;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
		Button->SetStyle(Style);
		Button->OnClicked.AddDynamic(this, &USaltStoreSlotWidget::HandleClicked);
		Button->OnHovered.AddDynamic(this, &USaltStoreSlotWidget::HandleHovered);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Button))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}
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
}

void USaltStoreSlotWidget::HandleClicked()
{
	if (OnClickedFn)
	{
		OnClickedFn(Index);
	}
}

void USaltStoreSlotWidget::HandleHovered()
{
	if (OnHoveredFn)
	{
		OnHoveredFn(Index);
	}
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
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;
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
		// The world behind: blurred, darkened, a vignette (as the satchel).
		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
		Blur->SetBlurStrength(8.f);
		FullScreen(Canvas, Blur);
		UImage* Dark = WidgetTree->ConstructWidget<UImage>();
		Dark->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		FullScreen(Canvas, Dark);
		FullScreen(Canvas, Picture(TEXT("T_UI_Vignette"), FVector2D(256.f, 144.f)));

		// Left, the satchel: the panel with its dishes, "N / 24" debossed (right-aligned at x 842, baseline 272), the pockets.
		At(Canvas, Picture(TEXT("T_UI_StoreSatchel"), FVector2D(900.f, 910.f)), FVector2D(60.f, 130.f), FVector2D(900.f, 910.f));
		SatchelCount = Deboss(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 36.f, 0.f, ETextJustify::Right);
		At(Canvas, SatchelCount, FVector2D(842.f, 272.f - 0.905f * 36.f), FVector2D::ZeroVector, FVector2D(1.f, 0.f));
		SatchelSlots.Reset();
		for (int32 I = 0; I < USaltInventoryComponent::Capacity; ++I)
		{
			USaltItemSlotWidget* W = CreateWidget<USaltItemSlotWidget>(this, USaltItemSlotWidget::StaticClass());
			W->Setup(I, [this](int32 SlotIndex) { ClickSatchelSlot(SlotIndex, FSlateApplication::Get().GetModifierKeys().IsShiftDown()); },
				[this](int32 SlotIndex) { SetCursor(false, SlotIndex); }, StoreDishRadius);
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
			auto OnClick = [this](int32 SlotIndex) { ClickStoreSlot(SlotIndex, FSlateApplication::Get().GetModifierKeys().IsShiftDown()); };
			auto OnHover = [this](int32 SlotIndex) { SetCursor(true, SlotIndex); };
			if (bCrate)
			{
				W->Setup(I, OnClick, OnHover, CrateCellSize, CrateItemInCell, FVector2D(102.f, 131.f) - CrateItemInCell);
				At(Canvas, W, CrateCell(I), CrateCellSize);
			}
			else
			{
				W->Setup(I, OnClick, OnHover, BarrelBox, BarrelBox * 0.5f, FVector2D(31.f, 35.f), BarrelItemSize);
				At(Canvas, W, BarrelItem(I) - BarrelBox * 0.5f, BarrelBox);
			}
			StoreSlots.Add(W);
		}

		// Along the bottom: "Click: move the stack . Shift-click: move one . [E] close the lid".
		At(Canvas, Picture(TEXT("T_UI_StoreHints"), FVector2D(860.f, 80.f)), FVector2D(540.f, 1000.f), FVector2D(860.f, 80.f));

		// On top, the flight: the dotted path, the target's outline, the item with its shadow and its coin.
		FlightLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
		FlightLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
		FullScreen(Canvas, FlightLayer);
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
	Bind(true);
	Refresh();
}

void USaltStoreWidget::NativeDestruct()
{
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
	if (!From || !To || !From->GetSlots().IsValidIndex(Index) || From->GetSlots()[Index].IsEmpty())
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
	TargetOutline->SetRenderOpacity(Flight.OutlineFade);
	TargetOutline->SetVisibility(Flight.OutlineFade > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
	UpdateFlight(InDeltaTime);
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

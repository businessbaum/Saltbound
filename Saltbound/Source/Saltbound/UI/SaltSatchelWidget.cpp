// Saltbound - the satchel (D50): the inventory and the map, in a hand-crafted frame (wood, iron, parchment, ink).

#include "UI/SaltSatchelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Boat/SaltBoat.h"
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
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Core/SaltItems.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor Ink(0.2f, 0.13f, 0.075f, 1.f);
	const FLinearColor InkSoft(0.32f, 0.23f, 0.15f, 1.f);
	const FLinearColor ParchmentText(0.95f, 0.88f, 0.72f, 1.f);
	const TCHAR* UI = TEXT("/Game/Saltbound/UI/Textures/");

	UTexture2D* Tex(const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s%s.%s"), UI, Name, Name));
	}

	FSlateBrush BoxBrush(UTexture2D* T, FVector2D Size, float Margin = 0.3f)
	{
		FSlateBrush B;
		B.SetResourceObject(T);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Box;
		B.Margin = FMargin(Margin);
		return B;
	}

	FSlateBrush ImageBrush(UTexture2D* T, FVector2D Size, bool bTile = false)
	{
		FSlateBrush B;
		B.SetResourceObject(T);
		B.ImageSize = Size;
		B.DrawAs = ESlateBrushDrawType::Image;
		B.Tiling = bTile ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
		return B;
	}

	UTextBlock* Text(UWidgetTree* Tree, const FText& Value, int32 Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetText(Value);
		return T;
	}

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
}

// --- Slot ---------------------------------------------------------------------------------------------------------

void USaltItemSlotWidget::Setup(USaltSatchelWidget* InOwner, int32 InIndex)
{
	Owner = InOwner;
	Index = InIndex;
}

TSharedRef<SWidget> USaltItemSlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(84.f);
		Size->SetHeightOverride(84.f);
		WidgetTree->RootWidget = Size;
		Button = WidgetTree->ConstructWidget<USaltMenuButton>();
		Button->OnClicked.AddDynamic(this, &USaltItemSlotWidget::HandleClicked);
		Button->OnHovered.AddDynamic(this, &USaltItemSlotWidget::HandleHovered);
		Size->SetContent(Button);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		Button->AddChild(Layers);
		IconImage = WidgetTree->ConstructWidget<UImage>();
		if (UOverlaySlot* S = Layers->AddChildToOverlay(IconImage))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}
		CountText = Text(WidgetTree, FText::GetEmpty(), 14, Ink, true);
		if (UOverlaySlot* S = Layers->AddChildToOverlay(CountText))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Bottom);
		}
		Show(nullptr, 0, false);
	}
	return Super::RebuildWidget();
}

void USaltItemSlotWidget::Show(UTexture2D* Icon, int32 Count, bool bSelected)
{
	if (!Button)
	{
		return;
	}
	static UTexture2D* SlotTex = nullptr;
	static UTexture2D* SlotSelTex = nullptr;
	SlotTex = SlotTex ? SlotTex : Tex(TEXT("T_UI_Slot"));
	SlotSelTex = SlotSelTex ? SlotSelTex : Tex(TEXT("T_UI_SlotSel"));
	FButtonStyle Style;
	FSlateBrush Normal = BoxBrush(bSelected ? SlotSelTex : SlotTex, FVector2D(84.f, 84.f));
	FSlateBrush Hovered = BoxBrush(SlotSelTex, FVector2D(84.f, 84.f));
	Hovered.TintColor = FSlateColor(FLinearColor(1.f, 0.96f, 0.9f));
	Style.SetNormal(Normal).SetHovered(Hovered).SetPressed(Hovered).SetDisabled(Normal);
	Style.SetNormalPadding(FMargin(8.f)).SetPressedPadding(FMargin(8.f, 9.f, 8.f, 7.f));
	Button->SetStyle(Style);
	IconImage->SetBrush(ImageBrush(Icon, FVector2D(60.f, 60.f)));
	IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CountText->SetText(Count > 1 ? FText::AsNumber(Count) : FText::GetEmpty());
	bShownSelected = bSelected;
}

void USaltItemSlotWidget::HandleClicked()
{
	if (USaltSatchelWidget* O = Owner.Get())
	{
		O->SelectSlot(Index);
	}
}

void USaltItemSlotWidget::HandleHovered()
{
	if (USaltSatchelWidget* O = Owner.Get())
	{
		O->SelectSlot(Index);
	}
}

// --- Tab ----------------------------------------------------------------------------------------------------------

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
		// Dimmed world behind the wooden frame; parchment inside.
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Backdrop;
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(1060.f);
		Size->SetHeightOverride(720.f);
		Backdrop->SetContent(Size);
		PanelSize = Size;
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
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(TabRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		const FText TabNames[] = { NSLOCTEXT("Saltbound", "TabInventory", "Inventory"), NSLOCTEXT("Saltbound", "TabMap", "Map") };
		Tabs.Reset();
		for (int32 I = 0; I < 2; ++I)
		{
			USaltSatchelTab* T = CreateWidget<USaltSatchelTab>(this, USaltSatchelTab::StaticClass());
			T->Setup(this, I, TabNames[I]);
			Tabs.Add(T);
			TabRow->AddChildToHorizontalBox(T)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		UTextBlock* Hint = Text(WidgetTree, NSLOCTEXT("Saltbound", "SatchelHint", "Q / E  page     Tab / Esc  close"), 13, InkSoft);
		if (UHorizontalBoxSlot* S = TabRow->AddChildToHorizontalBox(Hint))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}
		Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>();
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Pages))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Pages->AddChild(BuildInventoryPage());
		Pages->AddChild(BuildMapPage());
	}
	return Super::RebuildWidget();
}

UWidget* USaltSatchelWidget::BuildInventoryPage()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(5.f));
	SlotWidgets.Reset();
	for (int32 I = 0; I < USaltInventoryComponent::Capacity; ++I)
	{
		USaltItemSlotWidget* W = CreateWidget<USaltItemSlotWidget>(this, USaltItemSlotWidget::StaticClass());
		W->Setup(this, I);
		SlotWidgets.Add(W);
		Grid->AddChildToUniformGrid(W, I / 6, I % 6);
	}
	Row->AddChildToHorizontalBox(Grid)->SetVerticalAlignment(VAlign_Top);
	// The selected item: name, count, description.
	UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Details))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetPadding(FMargin(28.f, 8.f, 8.f, 0.f));
	}
	ItemName = Text(WidgetTree, FText::GetEmpty(), 26, Ink, true);
	Details->AddChildToVerticalBox(ItemName);
	ItemCount = Text(WidgetTree, FText::GetEmpty(), 15, InkSoft);
	Details->AddChildToVerticalBox(ItemCount)->SetPadding(FMargin(0.f, 2.f, 0.f, 14.f));
	ItemDescription = Text(WidgetTree, FText::GetEmpty(), 16, Ink);
	ItemDescription->SetAutoWrapText(true);
	Details->AddChildToVerticalBox(ItemDescription);
	return Row;
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

	// Beside the chart: which chart, the legend, the way to the boat and the scale.
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
	// Scale bar: 500 m on the home chart and 2 km on the sea chart are both the same length on the page.
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
	if (Pages)
	{
		Pages->SetActiveWidgetIndex(CurrentPage);
		// The frame fits the page: the slot grid is shorter than the chart.
		PanelSize->SetHeightOverride(CurrentPage == Map ? 720.f : 600.f);
	}
	for (int32 I = 0; I < Tabs.Num(); ++I)
	{
		Tabs[I]->SetActive(I == CurrentPage);
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
		Refresh();
	}
}

FText USaltSatchelWidget::GetSelectedName() const
{
	return ItemName ? ItemName->GetText() : FText::GetEmpty();
}

void USaltSatchelWidget::Refresh()
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	if (!Satchel || !ItemName)
	{
		return;
	}
	const TArray<FSaltItemStack>& Slots = Satchel->GetSlots();
	for (int32 I = 0; I < SlotWidgets.Num(); ++I)
	{
		const FSaltItemStack Stack = Slots.IsValidIndex(I) ? Slots[I] : FSaltItemStack();
		const FSaltItemDef* Def = Stack.IsEmpty() ? nullptr : SaltItems::Find(Stack.Id);
		UTexture2D* Icon = Def && Def->Icon ? LoadObject<UTexture2D>(nullptr, Def->Icon) : nullptr;
		SlotWidgets[I]->Show(Icon, Def ? Stack.Count : 0, I == Selected);
	}
	const FSaltItemStack Sel = Slots.IsValidIndex(Selected) ? Slots[Selected] : FSaltItemStack();
	const FSaltItemDef* Def = Sel.IsEmpty() ? nullptr : SaltItems::Find(Sel.Id);
	ItemName->SetText(Def ? Def->Name : NSLOCTEXT("Saltbound", "EmptySlot", "Empty"));
	ItemCount->SetText(Def ? FText::Format(NSLOCTEXT("Saltbound", "ItemCount", "{0} in the satchel"), FText::AsNumber(Satchel->CountOf(Sel.Id)))
		: FText::GetEmpty());
	ItemDescription->SetText(Def ? Def->Description
		: NSLOCTEXT("Saltbound", "EmptySlotDesc", "Things you find on beaches, in wrecks and among the ruins go here."));
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
	// Markers stay on the chart's edge when off it, pointing the way.
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
	if (Is({ EKeys::Tab, EKeys::Escape, EKeys::F10, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Left }))
	{
		RequestClose();
	}
	else if (Is({ EKeys::I }))
	{
		CurrentPage == Inventory ? RequestClose() : SetPage(Inventory);
	}
	else if (Is({ EKeys::M }))
	{
		CurrentPage == Map ? RequestClose() : SetPage(Map);
	}
	else if (Is({ EKeys::Q, EKeys::E, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder }))
	{
		SetPage(1 - CurrentPage);
	}
	else if (CurrentPage == Inventory && Is({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left }))
	{
		SelectSlot(Selected - 1);
	}
	else if (CurrentPage == Inventory && Is({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right }))
	{
		SelectSlot(Selected + 1);
	}
	else if (CurrentPage == Inventory && Is({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }))
	{
		SelectSlot(Selected - 6);
	}
	else if (CurrentPage == Inventory && Is({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }))
	{
		SelectSlot(Selected + 6);
	}
	else if (CurrentPage == Map && Is({ EKeys::Left, EKeys::Right, EKeys::A, EKeys::D, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right }))
	{
		SetChart(1 - CurrentChart);
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

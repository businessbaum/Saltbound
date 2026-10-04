// Saltbound - the satchel (D50): the inventory and the map, in a hand-crafted frame (wood, iron, parchment, ink).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/SaltMenuWidget.h"
#include "SaltSatchelWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidgetSwitcher;
class USaltSatchelWidget;
class UTexture2D;
class USizeBox;

/** One satchel slot: icon and count on an inked parchment square; hover or click shows the item. */
UCLASS()
class SALTBOUND_API USaltItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Index of the slot, and what a click and a hover do with it (the satchel selects, a store moves the stack). */
	void Setup(int32 InIndex, TFunction<void(int32)> InOnClicked, TFunction<void(int32)> InOnHovered);
	void Show(UTexture2D* Icon, int32 Count, bool bSelected);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();

	UPROPERTY(Transient) TObjectPtr<USaltMenuButton> Button;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountText;
	TFunction<void(int32)> OnClickedFn;
	TFunction<void(int32)> OnHoveredFn;
	int32 Index = 0;
	bool bShownSelected = false;
};

/** A tab at the top of the satchel (a wooden tab; the open page's tab is parchment). */
UCLASS()
class SALTBOUND_API USaltSatchelTab : public UUserWidget
{
	GENERATED_BODY()

public:
	/** A page tab, or (bInChart) a button that picks the chart on the map page. */
	void Setup(USaltSatchelWidget* InOwner, int32 InPage, const FText& InLabel, bool bInChart = false);
	void SetActive(bool bActive);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TObjectPtr<USaltMenuButton> Button;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LabelText;
	TWeakObjectPtr<USaltSatchelWidget> Owner;
	int32 Page = 0;
	bool bChart = false;
	FText Label;
};

/**
 * The satchel screen. Inventory: 24 slots (6 x 4) and the selected item's name and description. Map: a parchment
 * chart (home waters 3 km or open sea 12 km, Tools/textures/gen_ui.py) with place names, the player's and the boat's
 * markers turned to their headings, and the distance to the boat. Keys: Tab / I / M switch or close, Q / E change
 * the page, Esc closes; mouse and gamepad (B closes, shoulders change the page).
 */
UCLASS()
class SALTBOUND_API USaltSatchelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USaltSatchelWidget(const FObjectInitializer& ObjectInitializer);

	enum EPage : int32 { Inventory = 0, Map = 1 };

	void SetPage(int32 Page);
	int32 GetPage() const { return CurrentPage; }
	void SelectSlot(int32 Index);
	int32 GetSelectedSlot() const { return Selected; }
	FText GetSelectedName() const;
	void SetChart(int32 Chart);
	int32 GetChart() const { return CurrentChart; }
	/** Where a world position (cm) lies on the chart, in the chart image's pixels (its size is MapSize). */
	FVector2D WorldToChart(const FVector& World) const;
	FVector2D GetPlayerMarkerPosition() const;
	void Refresh();

	static constexpr float MapSize = 520.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UWidget* BuildInventoryPage();
	UWidget* BuildMapPage();
	void UpdateMarkers();
	void RequestClose();

	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY(Transient) TObjectPtr<USizeBox> PanelSize;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltSatchelTab>> Tabs;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltItemSlotWidget>> SlotWidgets;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemCount;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemDescription;
	UPROPERTY(Transient) TObjectPtr<UImage> ChartImage;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> ChartOverlay;
	UPROPERTY(Transient) TObjectPtr<UImage> PlayerMarker;
	UPROPERTY(Transient) TObjectPtr<UImage> BoatMarker;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PlaceLabels;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChartTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BoatDistance;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ScaleLabel;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> ChartTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltSatchelTab>> ChartButtons;

	int32 CurrentPage = 0;
	int32 CurrentChart = 0;
	int32 Selected = 0;
	FDelegateHandle InventoryChanged;
};

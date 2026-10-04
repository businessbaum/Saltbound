// Saltbound - the satchel (D50, restyled D55): a leather bag thrown open, items lying in round dishes pressed into the
// leather, the selected one described on a parchment tag on twine; and the map (M) as its own screen.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/SaltMenuWidget.h"
#include "SaltSatchelWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UOverlay;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class USaltSatchelWidget;
class UTexture2D;
class USizeBox;
struct FSaltItemDef;

/**
 * One pocket (D55, UI_Design.md section 5): what lies in a round dish pressed into the leather. The dish itself is part
 * of the screen's background; this draws the selection glow and gold ring, the item with its contact shadow (lifted when
 * selected), the ancient shard's turquoise glow and the count on a leather coin. Radius 50 in the satchel, 44 beside a
 * store; the widget is 4 x the radius square, its centre on the dish.
 */
UCLASS()
class SALTBOUND_API USaltItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Index of the slot, and what a click and a hover do with it (the satchel selects, a store moves the stack). */
	void Setup(int32 InIndex, TFunction<void(int32)> InOnClicked, TFunction<void(int32)> InOnHovered, float InRadius = 50.f);
	void Show(const FSaltItemDef* Item, int32 Count, bool bSelected);
	/** Turns the item in the hand (R): a slow turn of the icon. */
	void Turn();
	float GetRadius() const { return Radius; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	void Place(UWidget* W, FVector2D Centre, FVector2D Size);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<USaltMenuButton> Button;
	UPROPERTY(Transient) TObjectPtr<UImage> Glow;
	UPROPERTY(Transient) TObjectPtr<UImage> Ring;
	UPROPERTY(Transient) TObjectPtr<UImage> ShardGlow;
	UPROPERTY(Transient) TObjectPtr<UImage> Contact;
	UPROPERTY(Transient) TObjectPtr<UImage> IconShadow;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UOverlay> Coin;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CoinText;
	TFunction<void(int32)> OnClickedFn;
	TFunction<void(int32)> OnHoveredFn;
	int32 Index = 0;
	float Radius = 50.f;
	bool bSelected = false;
	bool bAncient = false;
	bool bHasItem = false;
	float SelAlpha = 0.f;
	float TurnTime = -1.f;
	float Clock = 0.f;
};

/** A button on the map screen that picks the chart (a wooden tab; the open chart's is parchment). */
UCLASS()
class SALTBOUND_API USaltSatchelTab : public UUserWidget
{
	GENERATED_BODY()

public:
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
 * The satchel screen (I or Tab) and the map screen (M), one widget with two pages. The world keeps running.
 * Satchel: 24 pockets (8 x 3); the selected one's name, how many it holds and how many fit, its description and what you
 * can do with it on a parchment tag that hangs on twine from a brass D-ring and swings when the selection changes.
 * Keys: I / Tab / Esc close, mouse or arrows / d-pad choose a pocket, R turns the item in your hand, G drops one,
 * E uses it (later). Map: a parchment chart with place names and the player's and the boat's markers (unchanged, D50).
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
	/** The tag's text lines (tests): name, count line, description. */
	FText GetTagLine(int32 Line) const;
	void SetChart(int32 Chart);
	int32 GetChart() const { return CurrentChart; }
	/** Where a world position (cm) lies on the chart, in the chart image's pixels (its size is MapSize). */
	FVector2D WorldToChart(const FVector& World) const;
	FVector2D GetPlayerMarkerPosition() const;
	void Refresh();
	/** Drops one of the selected item in front of the player (G). */
	bool DropSelected();

	static constexpr float MapSize = 520.f;
	static constexpr int32 Columns = 8;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UWidget* BuildSatchelScreen();
	UWidget* BuildMapScreen();
	UWidget* BuildMapPage();
	UWidget* BuildTag();
	void UpdateMarkers();
	void RequestClose();

	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> Screens;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltItemSlotWidget>> SlotWidgets;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountDark;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountLip;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> TagRoot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TagName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TagCount;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TagDescription;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TagActions;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UseText;
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
	/** The tag's swing (degrees from its resting tilt) and its rate. */
	float TagSwing = 0.f;
	float TagSwingRate = 0.f;
	FDelegateHandle InventoryChanged;
};

// Saltbound - the satchel (D50, restyled D55): a leather bag thrown open, items lying in round dishes pressed into the
// leather, the selected one described on a parchment tag on twine. (The map left it, D57: the chart is on the mast.)

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
	/** Index of the slot and the dish's radius. The screen it is on handles the mouse (hover, click, drag; D58). */
	void Setup(int32 InIndex, float InRadius = 50.f);
	void Show(const FSaltItemDef* Item, int32 Count, bool bSelected);
	/** Turns the item in the hand (R): a slow turn of the icon. */
	void Turn();
	/** D58: the item is being dragged away: only a faint trace of it stays in the dish. */
	void SetLifted(bool bInLifted);
	/** D58: something was just put here: it drops in a little large and settles. */
	void Land() { LandTime = 0.f; }
	float GetRadius() const { return Radius; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Place(UWidget* W, FVector2D Centre, FVector2D Size);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UImage> Glow;
	UPROPERTY(Transient) TObjectPtr<UImage> Ring;
	UPROPERTY(Transient) TObjectPtr<UImage> ShardGlow;
	UPROPERTY(Transient) TObjectPtr<UImage> Contact;
	UPROPERTY(Transient) TObjectPtr<UImage> IconShadow;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UOverlay> Coin;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CoinText;
	int32 Index = 0;
	float Radius = 50.f;
	bool bSelected = false;
	bool bAncient = false;
	bool bHasItem = false;
	bool bLifted = false;
	float SelAlpha = 0.f;
	float TurnTime = -1.f;
	float LandTime = -1.f;
	float Clock = 0.f;
};

/**
 * The satchel screen (I or Tab). The world keeps running.
 * Satchel: 24 pockets (8 x 3); the selected one's name, how many it holds and how many fit, its description and what you
 * can do with it on a parchment tag that hangs on twine from a brass D-ring and swings when the selection changes.
 * Keys: I / Tab / Esc close, mouse or arrows / d-pad choose a pocket, R turns the item in your hand, G drops one,
 * E uses it (later).
 */
UCLASS()
class SALTBOUND_API USaltSatchelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USaltSatchelWidget(const FObjectInitializer& ObjectInitializer);

	void SelectSlot(int32 Index);
	int32 GetSelectedSlot() const { return Selected; }
	FText GetSelectedName() const;
	/** The tag's text lines (tests): name, count line, description. */
	FText GetTagLine(int32 Line) const;
	void Refresh();
	/** Drops one of the selected item in front of the player (G). */
	bool DropSelected();
	/** D58: drag and drop. Puts the stack in pocket From into pocket To (moves, merges or changes places) as letting go of
	 *  a drag there does; returns how many moved. */
	int32 DropOnSlot(int32 From, int32 To);
	/** The pocket under a point (1080p design coordinates), or INDEX_NONE. */
	int32 SlotAt(FVector2D Design) const;
	bool IsDragging() const { return Drag.bDragging; }
	/** The pointer at a point (1080p design coordinates), as the mouse does it (tests and captures drive these too): a press
	 *  on a pocket picks its item up (true when a drag may start), moving drags it, letting go puts it down. */
	bool PointerDown(FVector2D Design);
	void PointerMove(FVector2D Design);
	void PointerUp(FVector2D Design);
	/** Where a pocket's dish is (1080p design coordinates). */
	static FVector2D PocketAt(int32 Index);
	/** D58: how far the opening has come (0..1, tests). */
	float GetOpenProgress() const { return FMath::Clamp(OpenAge / OpenTime, 0.f, 1.f); }

	static constexpr int32 Columns = 8;
	static constexpr float OpenTime = 0.2f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UWidget* BuildSatchelScreen();
	UWidget* BuildTag();
	void RequestClose();
	/** The pocket shown selected: the drag's target while dragging, else the selection. */
	int32 HighlightSlot() const { return Drag.bDragging ? Drag.Target : Selected; }
	void UpdateDragVisual();
	void EndDrag();

	/** A press on a pocket that becomes a drag once the pointer moves 6 px (D58). Positions in 1080p design coordinates. */
	struct FDrag
	{
		bool bPressed = false;
		bool bDragging = false;
		int32 From = INDEX_NONE;
		int32 Target = INDEX_NONE;
		FVector2D PressAt = FVector2D::ZeroVector;
		FVector2D Pointer = FVector2D::ZeroVector;
	};
	FDrag Drag;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Contents;
	UPROPERTY(Transient) TObjectPtr<UWidget> Backdrop;
	UPROPERTY(Transient) TObjectPtr<class UBackgroundBlur> BackdropBlur;
	UPROPERTY(Transient) TObjectPtr<UImage> DragShadow;
	UPROPERTY(Transient) TObjectPtr<UImage> DragIcon;
	UPROPERTY(Transient) TObjectPtr<UOverlay> DragCoin;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DragCoinText;
	int32 DragCount = 0;
	/** Let go off the pockets: the item goes back to ReturnSlot from ReturnFrom (time since letting go; < 0 when none). */
	float ReturnTime = -1.f;
	FVector2D ReturnFrom = FVector2D::ZeroVector;
	int32 ReturnSlot = INDEX_NONE;
	float OpenAge = 0.f;

	UPROPERTY(Transient) TArray<TObjectPtr<USaltItemSlotWidget>> SlotWidgets;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountDark;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountLip;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> TagRoot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TagName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TagCount;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TagDescription;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TagActions;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UseText;
	/** The tag's "use" row and the key on its keycap (the player's Use key, D66). */
	UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> UseRow;
	FString UseRowKey;

	int32 Selected = 0;
	/** The tag's swing (degrees from its resting tilt) and its rate. */
	float TagSwing = 0.f;
	float TagSwingRate = 0.f;
	FDelegateHandle InventoryChanged;
};

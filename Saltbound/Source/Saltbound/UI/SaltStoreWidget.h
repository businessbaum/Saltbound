// Saltbound - a store's screen (D52, restyled D55, UI_Design.md section 6): the satchel beside the boat's crate or barrel;
// the things fly from one to the other.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/SaltItems.h"
#include "SaltStoreWidget.generated.h"

class UCanvasPanel;
class UImage;
class UOverlay;
class UTextBlock;
class USaltInventoryComponent;
class USaltItemSlotWidget;

/**
 * A thing lying in one of a store's compartments: its icon with a drop shadow, the count on a leather coin, a warm glow
 * under it while the cursor is on it, a small settle when something lands. The widget is a box round the compartment
 * (it is the click target); the item lies at ItemCentre in it.
 */
UCLASS()
class SALTBOUND_API USaltStoreSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InIndex, TFunction<void(int32)> InOnClicked, TFunction<void(int32)> InOnHovered, FVector2D InBoxSize, FVector2D InItemCentre,
		FVector2D InCoinOffset, float InItemSize = 96.f);
	void Show(const FSaltItemDef* Item, int32 Count, bool bInHovered);
	void Land();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	void Place(UWidget* W, FVector2D Centre, FVector2D Size);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UImage> Glow;
	UPROPERTY(Transient) TObjectPtr<UImage> ShardGlow;
	UPROPERTY(Transient) TObjectPtr<UImage> IconShadow;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UOverlay> Coin;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CoinText;
	TFunction<void(int32)> OnClickedFn;
	TFunction<void(int32)> OnHoveredFn;
	int32 Index = 0;
	FVector2D BoxSize = FVector2D(140.f, 140.f);
	FVector2D ItemCentre = FVector2D(70.f, 70.f);
	FVector2D CoinOffset = FVector2D(34.f, 40.f);
	float ItemSize = 96.f;
	bool bHovered = false;
	bool bAncient = false;
	float HoverAlpha = 0.f;
	float LandTime = -1.f;
	float Clock = 0.f;
};

/**
 * The satchel (a leather panel with 24 pressed dishes) beside a store: the crate (an open chest with 5 x 2 compartments,
 * "CRATE" burned into its lid) or the round barrel (3 compartments in the middle, 7 around; its head leans behind it).
 * Click moves the stack to the other side, Shift-click moves one; the thing flies there on an arc with a dotted path and
 * the slot it goes to is outlined until it lands. Arrows / d-pad move the cursor, Enter / A move the stack, X moves one.
 * E / Esc close. The world keeps running.
 */
UCLASS()
class SALTBOUND_API USaltStoreWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USaltStoreWidget(const FObjectInitializer& ObjectInitializer);

	/** Call before it is shown. */
	void SetInventories(USaltInventoryComponent* InSatchel, USaltInventoryComponent* InStore);
	USaltInventoryComponent* GetStore() const { return Store.Get(); }
	/** Same as clicking the slot (bOne: Shift-click). Returns how many moved. */
	int32 ClickSatchelSlot(int32 Index, bool bOne = false);
	int32 ClickStoreSlot(int32 Index, bool bOne = false);
	void Refresh();

	bool IsCrate() const { return bCrate; }
	/** Something in flight (tests): where it goes. */
	bool IsFlying() const { return Flight.bActive; }
	bool IsFlyingToStore() const { return Flight.bToStore; }
	int32 GetFlightTargetSlot() const { return Flight.bActive ? Flight.ToSlot : INDEX_NONE; }
	/** The counts as shown ("6 / 24", "2 / 10"). */
	FText GetSatchelCountText() const;
	FText GetStoreCountText() const;
	/** Where a slot's item lies on the screen (1080p design coordinates). */
	FVector2D SlotCentre(bool bStoreSide, int32 Index) const;
	/** The cursor (the slot under the mouse, or moved with the arrows / d-pad). */
	void SetCursor(bool bStoreSide, int32 Index);
	bool IsCursorOnStore() const { return bCursorStore; }
	int32 GetCursorSlot() const { return CursorSlot; }
	/** Moves the cursor to the nearest slot in a screen direction (x right, y down). */
	void MoveCursor(FVector2D Direction);
	/** Captures: hold a flight at this point of its arc (0..1), or let it fly (< 0). */
	void HoldFlightAt(float Progress) { FlightHold = Progress; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	int32 Click(bool bFromStore, int32 Index, bool bOne);
	void Land();
	void Bind(bool bBind);
	void UpdateFlight(float Dt);
	int32 SlotCount(bool bStoreSide) const;
	float ItemSize(bool bStoreSide) const;
	void Close();

	struct FFlight
	{
		bool bActive = false;
		bool bToStore = false;
		int32 ToSlot = INDEX_NONE;
		int32 Count = 0;
		const FSaltItemDef* Item = nullptr;
		FVector2D From = FVector2D::ZeroVector;
		FVector2D Control = FVector2D::ZeroVector;
		FVector2D To = FVector2D::ZeroVector;
		float FromSize = 96.f;
		float ToSize = 96.f;
		float Time = 0.f;
		/** Where it is on its arc (eased, 0..1). */
		float Progress = 0.f;
		/** The side it flies to as it was before the move (shown until it lands). */
		TArray<FSaltItemStack> TargetBefore;
		/** The dotted path fades after the landing. */
		float PathFade = 0.f;
		float OutlineFade = 0.f;
	};

	TWeakObjectPtr<USaltInventoryComponent> Satchel;
	TWeakObjectPtr<USaltInventoryComponent> Store;
	bool bCrate = true;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltItemSlotWidget>> SatchelSlots;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltStoreSlotWidget>> StoreSlots;
	UPROPERTY(Transient) TObjectPtr<UOverlay> SatchelCount;
	UPROPERTY(Transient) TObjectPtr<UOverlay> StoreCount;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> FlightLayer;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> PathDots;
	UPROPERTY(Transient) TObjectPtr<UImage> TargetOutline;
	UPROPERTY(Transient) TObjectPtr<UImage> FlyShadow;
	UPROPERTY(Transient) TObjectPtr<UImage> FlyIcon;
	UPROPERTY(Transient) TObjectPtr<UOverlay> FlyCoin;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FlyCoinText;
	FFlight Flight;
	/** The path's dots: where they lie and how far along the arc each is. */
	TArray<FVector2D> PathPoints;
	TArray<float> PathParams;
	float FlightHold = -1.f;
	bool bCursorStore = false;
	int32 CursorSlot = 0;
	/** The cursor shows once the mouse is on a slot or the arrows were used. */
	bool bCursorShown = false;
	FDelegateHandle SatchelChanged;
	FDelegateHandle StoreChanged;
};

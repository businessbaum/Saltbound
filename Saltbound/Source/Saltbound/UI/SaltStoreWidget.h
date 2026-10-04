// Saltbound - a store's screen (D52): the satchel beside a store (the boat's barrel or crate). Functional first; the
// owner gives the design later.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltStoreWidget.generated.h"

class UTextBlock;
class USaltInventoryComponent;
class USaltItemSlotWidget;

/**
 * Two grids in the satchel's frame: the satchel (24) and the store (its own size). Clicking a thing moves its stack to
 * the other side (as much as fits); hovering names it. E / Esc / Tab close; gamepad B closes.
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
	/** Same as clicking the slot (tests use these). */
	void ClickSatchelSlot(int32 Index);
	void ClickStoreSlot(int32 Index);
	void Refresh();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void ShowItem(const USaltInventoryComponent* From, int32 Index);
	void Bind(bool bBind);

	TWeakObjectPtr<USaltInventoryComponent> Satchel;
	TWeakObjectPtr<USaltInventoryComponent> Store;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltItemSlotWidget>> SatchelSlots;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltItemSlotWidget>> StoreSlots;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StoreLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemLine;
	FDelegateHandle SatchelChanged;
	FDelegateHandle StoreChanged;
};

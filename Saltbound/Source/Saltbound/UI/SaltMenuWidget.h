// Saltbound - the pause menu (D66, Docs/Design/Menu_Ideas/Menus.html "Pause"): painted lines over the blurred world and a
// "where you are" card.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "UI/SaltMenuKit.h"
#include "SaltMenuWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/** A button that never takes the keyboard focus: the screen keeps it and handles the keys. */
UCLASS()
class SALTBOUND_API USaltMenuButton : public UButton
{
	GENERATED_BODY()

public:
	USaltMenuButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { InitIsFocusable(false); }
};

/**
 * Resume, Save now, Voyages, Settings, Return to the title, Quit to desktop. The world stops and blurs; the card on the right
 * answers what a player asks after a break: where am I, the time of day, the wind, where the boat is, the satchel, the last
 * save. Returning to the title asks first when something happened since the last save (quitting saves, as before, D46).
 */
UCLASS()
class SALTBOUND_API USaltMenuWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltMenuWidget(const FObjectInitializer& ObjectInitializer);

	enum class EItem : uint8 { Resume, Save, Voyages, Settings, Title, Quit };
	static int32 FindItem(EItem Item) { return (int32)Item; }
	/** Re-reads the card and the save line. */
	void Refresh();
	/** The card's lines as shown (tests). */
	FString DescribeCard() const;

	virtual void Back() override;
	virtual void OnRevealed() override { Refresh(); }

protected:
	virtual void Build() override;
	virtual void NativeConstruct() override;
	virtual void ItemUsed(int32 Index) override;

private:
	UTextBlock* CardLine(UVerticalBox* Box, const FText& Label, TObjectPtr<UTextBlock>& OutValue, float Top);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlaceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WhereText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WindText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BoatText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SatchelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SavedText;
};

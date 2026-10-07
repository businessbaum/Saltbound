// Saltbound - the letter in the castaway's pocket (D85): read with L, the world blurred behind.

#pragma once

#include "CoreMinimal.h"
#include "UI/SaltMenuKit.h"
#include "SaltLetterWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * The letter for the Cartographer (Tools/textures/ui_parts.js, T_UI_Letter_Wet / _Dry): a portrait sheet in the middle of the
 * screen, 76 % of its 820 x 1100 design size. Wet, the ink has run and a line under it says a fire would dry it; dried, every word
 * reads. E, Esc or L put it away. The game runs on behind it.
 */
UCLASS()
class SALTBOUND_API USaltLetterWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltLetterWidget();
	void SetDry(bool bInDry);
	bool IsDry() const { return bDry; }

protected:
	virtual void Build() override;
	virtual void Accept() override;
	virtual bool RawKey(const FKey& Key, bool bRepeat) override;

private:
	void Apply();

	UPROPERTY(Transient) TObjectPtr<UImage> Page;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Caption;
	bool bDry = false;
};

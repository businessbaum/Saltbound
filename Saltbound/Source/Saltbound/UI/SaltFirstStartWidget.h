// Saltbound - the first start (D66, Docs/Design/Menu_Ideas/Menus.html "First start"): brightness with the game's knot as the
// test pattern, three knots from clearly seen to not at all. Also opened from Settings > Display > Brightness.

#pragma once

#include "CoreMinimal.h"
#include "UI/SaltMenuKit.h"
#include "SaltFirstStartWidget.generated.h"

class UCanvasPanel;
class UImage;

/** A / D (or the rope) turn the brightness (the display gamma, applied at once); E keeps it. */
UCLASS()
class SALTBOUND_API USaltFirstStartWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltFirstStartWidget(const FObjectInitializer& ObjectInitializer);

	/** Opened from the settings (Esc goes back there, the first start is not marked). */
	bool bFromSettings = false;
	/** Steps the brightness by Delta steps of 0.1. */
	void Step(int32 Delta);
	/** Keeps the brightness (as E). */
	void Done();

	virtual void Back() override;

protected:
	virtual void Build() override;
	virtual void Navigate(int32 Dx, int32 Dy) override;
	virtual void Accept() override { Done(); }
	virtual void PointerMove(const FVector2D& Design) override;
	virtual void PointerDown(const FVector2D& Design) override;
	virtual void PointerUp(const FVector2D& Design) override { bDragging = false; }

private:
	void SetBrightness(float Value);
	void Layout();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Slider;
	float Before = 2.2f;
	bool bDragging = false;
};

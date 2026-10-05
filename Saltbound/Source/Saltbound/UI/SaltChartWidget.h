// Saltbound - a close look at the chart on the mast (D57; D58: the eyes glide to the sheet itself).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltChartWidget.generated.h"

class ASaltBoat;
class UCanvasPanel;
class UTextBlock;
class USaltKeyPrompt;

/**
 * Over the chart on the mast while the player looks at it closely (the camera has glided to it, ASaltPlayerCharacter::
 * FocusView): the places in view named in ink, a north mark and a 200 m scale, laid on the sheet where it is on screen
 * (projected every frame, so they stay on it as the boat moves), and "E Put the chart back" in the key-scrap prompt along
 * the bottom. They ink in once the eyes have arrived. E / Esc / B put it back; the world keeps running.
 */
UCLASS()
class SALTBOUND_API USaltChartWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USaltChartWidget(const FObjectInitializer& ObjectInitializer);

	void SetBoat(ASaltBoat* InBoat) { Boat = InBoat; }
	/** Where the chart is centred (region chart uv) and which places it names now (tests). */
	FVector2D GetShownCentre() const { return Centre; }
	TArray<FName> GetNamedPlaces() const;
	/** How far the names and marks have inked in (0..1, tests). */
	float GetInkOpacity() const { return Ink; }
	/** Follows the boat (every frame; tests call it at once). */
	void Update();

	/** The sheet's layout frame (1080p pixels at the size the old close look showed it): 640 wide. */
	static constexpr float SheetWidth = 640.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** Lays the sheet's marks over the sheet as it is on screen now. */
	void PlaceOnSheet();

	TWeakObjectPtr<ASaltBoat> Boat;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> SheetLayer;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PlaceLabels;
	UPROPERTY(Transient) TObjectPtr<USaltKeyPrompt> PutBack;
	FVector2D Centre = FVector2D(0.5, 0.75);
	float Ink = 0.f;
	bool bPromptShown = false;
};

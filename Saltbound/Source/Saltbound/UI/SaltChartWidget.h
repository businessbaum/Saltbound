// Saltbound - a close look at the chart on the mast (D57): the same live chart, large.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltChartWidget.generated.h"

class ASaltBoat;
class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;

/**
 * The chart sheet from the mast held up to the eye: a window of the region chart round the boat (1.6 km across, north
 * up, M_UI_Chart), the boat's arrow in its middle along her heading, the places in view named in ink, a north mark and a
 * scale. It follows the boat live. E / Esc / B put it back; the world keeps running.
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
	/** Follows the boat (every frame; tests call it at once). */
	void Update();

	/** The sheet on screen (1080p design coordinates): 640 wide, centred. */
	static constexpr float SheetWidth = 640.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<ASaltBoat> Boat;
	UPROPERTY(Transient) TObjectPtr<UImage> Sheet;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SheetMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PlaceLabels;
	FVector2D Centre = FVector2D(0.5, 0.75);
};

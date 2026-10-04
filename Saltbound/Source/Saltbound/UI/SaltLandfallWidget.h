// Saltbound - the landfall banner (D55, UI_Design.md section 9): a parchment ribbon with a wax seal when the player
// finds a new island.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltLandfallWidget.generated.h"

class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class USaltPaintedText;

/**
 * "A new island" in painted lettering over an arched parchment ribbon (its top at y 150, its tails tucked under) with the
 * island's name inked along the arch, an oxblood wax seal pinning its left end, and where it lies from Saltbound Cove
 * under it. The ribbon unrolls from the middle (0.6 s), the seal stamps down (0.5 s), the name inks in from the left
 * (0.6 s), the line under it fades in; it holds about 4 s and rolls up (0.5 s). A second landfall waits for the first.
 * The ribbon and the name are pictures made per island (T_UI_Landfall_<Id>_Band / _Name, Tools/textures/ui_parts.js).
 */
UCLASS()
class SALTBOUND_API USaltLandfallWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows an island's banner (after the one showing, if any). */
	void Show(FName PlaceId);
	/** Stops the banner and forgets the queue (and a capture's hold). */
	void Clear();
	bool IsShowing() const { return !Current.IsNone(); }
	FName GetShownPlace() const { return Current; }
	float GetShownTime() const { return Time; }
	/** Tests: how far the ribbon has unrolled and the name has inked in (0..1), the line under it. */
	float GetUnroll() const { return Unroll; }
	float GetInk() const { return Ink; }
	FText GetSubtitle() const;
	/** Captures: hold the banner at this time (s), or let it run (< 0). */
	void HoldAt(float T) { HoldTime = T; }

	static constexpr float Duration = 6.1f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	bool Begin(FName PlaceId);
	void Apply(float T);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UImage> Band;
	UPROPERTY(Transient) TObjectPtr<UImage> NameImage;
	UPROPERTY(Transient) TObjectPtr<UImage> Seal;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BandMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> NameMaterial;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> Heading;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> Subtitle;

	TArray<FName> Queue;
	FName Current;
	float Time = 0.f;
	float HoldTime = -1.f;
	/** Half the band's width and the name's width (px at 1080p). */
	float HalfWidth = 300.f;
	float NameWidth = 0.f;
	float Unroll = 0.f;
	float Ink = 0.f;
};

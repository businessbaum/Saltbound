// Saltbound - the loading screen (D66, Docs/Design/Menu_Ideas/Menus.html "Loading"): the place you sail to in large lettering
// over its picture, a line from an old log, and the save sign's knot turning in the corner.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltLoadingWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class USaltPaintedText;

/**
 * The world is loaded in place (a save is applied to the running map), so the screen is a veil: it fades in, does the work once
 * the screen is covered, stays at least MinShown so it can be read, hands over (Then: the title, or play) and fades out.
 */
UCLASS()
class SALTBOUND_API USaltLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The place's name, the line under it, the picture (a file, may be empty); Work runs covered, Then before the fade-out. */
	void Begin(const FText& Place, const FText& Line, const FString& Picture, TFunction<void()> InWork, TFunction<void()> InThen);
	bool IsDone() const { return Phase == EPhase::Done; }
	/** Covered and the work not yet done (tests). */
	bool IsCovering() const { return Phase == EPhase::In || Phase == EPhase::Hold; }

	/** Stays covered at least this long (captures). */
	float HoldFor = 0.f;

	static constexpr float FadeIn = 0.35f;
	static constexpr float MinShown = 1.4f;
	static constexpr float FadeOut = 0.5f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override { return FReply::Handled(); }

private:
	enum class EPhase : uint8 { In, Hold, Out, Done };

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UImage> Picture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> PictureTexture;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> PlaceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LineText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LogText;
	UPROPERTY(Transient) TObjectPtr<UImage> Ring;
	UPROPERTY(Transient) TObjectPtr<UImage> Rose;

	FText PlaceName;
	FText PlaceLine;
	FString PicturePath;
	TFunction<void()> Work;
	TFunction<void()> Then;
	EPhase Phase = EPhase::In;
	float Age = 0.f;
	float PhaseAge = 0.f;
	int32 WorkFrames = -1;
};

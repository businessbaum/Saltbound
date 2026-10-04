// Saltbound - minimal in-game HUD: focus dot, interaction prompt, station hint.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltHUDWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * Builds its widget tree in code (no widget asset needed).
 * Deliberately quiet: a small dot that brightens on usable objects and a short prompt that fades in.
 */
UCLASS()
class SALTBOUND_API USaltHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** A short notice at the top of the screen that fades after a few seconds. */
	void ShowNotice(const FText& Text);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient) TObjectPtr<UImage> Dot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Prompt;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Hint;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Notice;
	float NoticeTime = 0.f;

	float PromptAlpha = 0.f;
	float HintAlpha = 0.f;
	float DotAlpha = 0.35f;
	FText LastPrompt;
};

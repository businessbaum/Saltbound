// Saltbound - asking before something can't be undone (D66, Docs/Design/Menu_Ideas/Menus.html): a torn scrap with the question,
// what it costs, and two one-key answers. No hold: E answers yes, Esc no.

#pragma once

#include "CoreMinimal.h"
#include "UI/SaltMenuKit.h"
#include "SaltConfirmWidget.generated.h"

UCLASS()
class SALTBOUND_API USaltConfirmWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltConfirmWidget(const FObjectInitializer& ObjectInitializer);

	/** The question, what it means, the two answers' words; OnYes runs after the scrap has closed. Call before it is shown. */
	void Setup(const FText& InTitle, const FText& InBody, const FText& InYes, const FText& InNo, TFunction<void()> InOnYes);
	/** Answers yes (as E). */
	void Confirm();
	const FText& GetTitle() const { return Title; }

protected:
	virtual void Build() override;
	virtual void Navigate(int32 Dx, int32 Dy) override {}
	virtual void Accept() override { Confirm(); }
	virtual void PointerMove(const FVector2D& Design) override {}
	virtual void PointerDown(const FVector2D& Design) override;

private:
	FText Title;
	FText Body;
	FText Yes;
	FText No;
	TFunction<void()> OnYes;
	float YesRight = 0.f;
};

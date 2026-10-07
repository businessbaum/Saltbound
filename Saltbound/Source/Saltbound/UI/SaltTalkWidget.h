// Saltbound - a person speaks (FH2, D81; Docs/Design/UI/UI_FirstHour.html, idea A and B of D75).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaltTalkWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
struct FSaltShownChoice;

/**
 * Built in code over the HUD. A line: a torn half sheet (T_UI_TalkScrap, 960 wide, as a 9-slice as high as its words need) low in
 * the middle, its foot at 1080p y 1036, growing upwards; the speaker's mark in red wax at its corner, the name in small capitals
 * with a quill's rule under it, the line in ink (IM Fell English 32 px, 42 px lines, written out word by word), then the answers on
 * key scraps: 1-3, and E for the plain one. The world keeps running behind it. A call in passing: a small scrap (T_UI_CallScrap)
 * pinned beside the speaker for a few seconds.
 */
UCLASS()
class SALTBOUND_API USaltTalkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** A line on the sheet; the words are written out from the start, the answers come when they are done. */
	void ShowLine(const FText& Name, FName Seal, const FText& Text, const TArray<FSaltShownChoice>& Choices);
	void HideLine();
	bool IsShowingLine() const { return bLine; }
	bool IsWriting() const;
	void FinishWriting();
	/** Marks an answer as taken (the warm wash and the quill's rule under it), by its place among the shown ones. */
	void MarkChoice(int32 Shown);

	/** A call in passing, pinned beside a point in the world for Seconds. */
	void ShowCall(const FText& Name, FName Seal, const FText& Text, const FVector& WorldAt, float Seconds);
	bool IsShowingCall() const { return CallLeft > 0.f; }

	/** What the sheet shows now (tests): the words written so far, the answers. */
	FString GetWrittenText() const;
	FString GetLineText() const { return FullText; }
	int32 GetChoiceCount() const { return ChoiceRows.Num(); }
	FString GetCallText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Build();
	void SetWritten(int32 Chars);
	void PlaceCall();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<USizeBox> SheetBox;
	UPROPERTY(Transient) TObjectPtr<UImage> Seal;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Name;
	UPROPERTY(Transient) TObjectPtr<USizeBox> NameRule;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LineFull;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LineWritten;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Choices;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ChoiceRows;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> ChoiceGlows;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ChoiceRules;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ChoiceTexts;

	UPROPERTY(Transient) TObjectPtr<USizeBox> CallBox;
	UPROPERTY(Transient) TObjectPtr<UImage> CallSeal;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CallName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CallLine;

	FString FullText;
	int32 WrittenChars = 0;
	float WriteT = 0.f;
	float LineAge = 0.f;
	float HideAge = -1.f;
	float ChoicesAge = -1.f;
	bool bLine = false;

	FVector CallAt = FVector::ZeroVector;
	float CallLeft = 0.f;
	float CallAge = 0.f;
	FVector2D ViewSize = FVector2D(1920.f, 1080.f);
};

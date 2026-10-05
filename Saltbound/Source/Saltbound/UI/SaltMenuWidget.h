// Saltbound - pause menu with the settings (display and look), built in code like the HUD.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SaltMenuWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class USaltMenuWidget;

/** A button that never takes the keyboard focus: the menu keeps it and handles the keys. */
UCLASS()
class SALTBOUND_API USaltMenuButton : public UButton
{
	GENERATED_BODY()

public:
	USaltMenuButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { InitIsFocusable(false); }
};

/** One line of the menu: a label and (for options) its value; hover selects it, a click uses it. */
UCLASS()
class SALTBOUND_API USaltMenuRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call before the row is added to a panel. */
	void Setup(USaltMenuWidget* InMenu, int32 InIndex, const FText& InLabel);
	void SetValue(const FText& Value);
	void SetSelected(bool bSelected);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();

	UPROPERTY(Transient) TObjectPtr<USaltMenuButton> Button;
	UPROPERTY(Transient) TObjectPtr<UBorder> Background;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueText;
	TWeakObjectPtr<USaltMenuWidget> Menu;
	int32 Index = 0;
	FText Label;
};

/**
 * Pause menu: Resume, save, load the last save, V-Sync, frame rate limit, resolution scale, window mode, field of view, mouse sensitivity,
 * invert look, prompt position, restore defaults, quit. Keyboard (arrows / WASD, Enter, Esc / F10), mouse and gamepad (d-pad,
 * A, B / Start). Every change applies at once and is saved.
 */
UCLASS()
class SALTBOUND_API USaltMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class ERow : uint8 { Resume, Save, Load, VSync, FrameCap, ResolutionScale, WindowMode, FieldOfView, MouseSensitivity, InvertLook, PromptPlace, Defaults, Quit };
	static bool IsOption(ERow Row) { return Row >= ERow::VSync && Row <= ERow::PromptPlace; }

	/** Re-reads every value from the settings. */
	void Refresh();
	int32 GetRowCount() const { return Rows.Num(); }
	int32 FindRow(ERow Row) const { return Rows.IndexOfByKey(Row); }
	int32 GetSelectedRow() const { return Selected; }
	void SelectRow(int32 Row);
	/** Steps an option by Delta (wraps around); actions ignore it. */
	void ChangeRow(int32 Row, int32 Delta);
	/** Enter / click: options step forward, actions run. */
	void ActivateRow(int32 Row);
	FString DescribeRow(int32 Row) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FText ValueOf(ERow Row) const;
	void RequestClose();

	TArray<ERow> Rows;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltMenuRow>> RowWidgets;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Footer;
	int32 Selected = 0;
};

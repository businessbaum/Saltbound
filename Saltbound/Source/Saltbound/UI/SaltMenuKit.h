// Saltbound - the menus' shared pieces (D66, Docs/Design/Menu_Ideas/Menus.html): the intro's big rope (selection
// underlines, slider tracks, toggles), a knot tied in it (slider handles), small anchors (the "< value >" arrows), the
// compass rose as the pointer, key scraps, painted menu lines and the screen every menu is built on.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UI/SaltUIStyle.h"
#include "SaltMenuKit.generated.h"

class ASaltPlayerController;
class UBackgroundBlur;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class USaltPaintedText;
class UTextBlock;
struct FSaltPlace;
struct FSaltVoyageInfo;

namespace SaltMenu
{
	/** A colour stop of a made gradient (sRGB and alpha, as the design's CSS). */
	struct FStop
	{
		float At;
		uint8 R, G, B;
		float A;
	};

	/** A gradient texture made at runtime (256 px along x or y), kept for the session under its key. */
	SALTBOUND_API UTexture2D* Gradient(FName Key, bool bHorizontal, const TArray<FStop>& Stops);
	/** An image filling the whole canvas (backdrops, shades). */
	SALTBOUND_API UImage* FullScreen(UWidgetTree* Tree, UCanvasPanel* Canvas, UObject* Texture, const FLinearColor& Tint = FLinearColor::White);
	/** A plain colour over the whole canvas. */
	SALTBOUND_API UImage* FullScreenColor(UWidgetTree* Tree, UCanvasPanel* Canvas, const FLinearColor& Color);

	/** The big rope (T_UI_MenuRope), tiled along x; Height is the image's (the rope is 14/30 of it). */
	SALTBOUND_API FSlateBrush RopeBrush(float Height);
	SALTBOUND_API UImage* Rope(UWidgetTree* Tree, float Height);
	/** The knot tied in the rope (T_UI_MenuKnot, 2:1); the rope runs through it at 0.6 of its height. */
	SALTBOUND_API UImage* Knot(UWidgetTree* Tree, float Width);
	/** A small anchor pointing left (previous) or right (next). */
	SALTBOUND_API UImage* Anchor(UWidgetTree* Tree, float Size, bool bNext);
	/** The compass rose turned to point right (the menus' pointer). */
	SALTBOUND_API UImage* Pointer(UWidgetTree* Tree, float Size);

	/** Painted lettering (USaltPaintedText) with its text. */
	SALTBOUND_API USaltPaintedText* Paint(UWidgetTree* Tree, const FText& Text, ESaltFace Face, float Size, ETextJustify::Type Justify = ETextJustify::Left,
		float LetterSpacing = 0.f);
	/** Where a UMG text block's top goes for a CSS line box at CssTop (the faces' line is about 1.268 em). */
	inline float TextTop(float CssTop, float Size, float LineHeight = 1.f) { return CssTop + (LineHeight - 1.268f) * Size * 0.5f; }

	/** A key's short name on a scrap ("E", "Shift", "Space", "Ctrl"). */
	SALTBOUND_API FString KeyName(const FKey& Key);
	/** A key on a torn parchment scrap (as the prompts'), tilted -3 degrees, Size px high; words get the wide scrap. Returns a
	 *  box of the scrap's own size (its shadow reaches past it). */
	SALTBOUND_API UWidget* KeyScrap(UWidgetTree* Tree, const FString& Label, float Size = 56.f);
	SALTBOUND_API float KeyScrapWidth(const FString& Label, float Size = 56.f);
	/** A key whose scrap the ink soaks while it is held (M_UI_KeySoak, the letter from KeyLetter). */
	SALTBOUND_API UWidget* SoakKey(UWidgetTree* Tree, UObject* Outer, const FString& Letter, float Size, UMaterialInstanceDynamic*& OutMid);

	/** The player's key for what a prompt shows by its default key: "E" becomes "F" when Use was changed to F (Keys and
	 *  buttons). Keys that cannot be changed (R, G, Esc, the digits) stay as they are. */
	SALTBOUND_API FString ShownKey(const FString& DefaultLabel);
	/** A key pressed in a screen is the action's key: the player's, or the default (which keeps working in the screens). */
	SALTBOUND_API bool IsActionKey(const FKey& Pressed, FName ActionId);
	/** A key's inked letter for the key-soak material (Letter) or its light twin where the ink has soaked through (LetterLight):
	 *  the painted picture where there is one (T_UI_KeyLetter_<Key>: A D E F G Q R S W X), otherwise drawn once at run time in
	 *  the same face, size and place into a 240 px target (words a little smaller to fit the scrap). */
	SALTBOUND_API UTexture* KeyLetter(const FString& Label, bool bLight);
	/** Puts a key's letters into a key-soak material. */
	SALTBOUND_API void SetSoakKey(UMaterialInstanceDynamic* Mid, const FString& Label);

	/** One hint of the controls row: its keys and what they do. */
	struct FHint
	{
		TArray<FString> Keys;
		FText Label;
	};
	/** The controls row at the bottom ("[W][S] choose   [E] sail"): keys 12 px apart, then the label; hints 46 px apart. */
	SALTBOUND_API UHorizontalBox* Hints(UWidgetTree* Tree, const TArray<FHint>& Hints, float KeySize = 56.f, float TextSize = 30.f);
	/** A hints row in the leather screens' style (leather keycaps, the words in plain cream lettering with the soft halo), as
	 *  their baked hint pictures draw it; for when the player's keys differ from those pictures. A key "-" is a dash between
	 *  two keys ("1 - 5"). */
	SALTBOUND_API UHorizontalBox* KeycapHints(UWidgetTree* Tree, const TArray<FHint>& Hints, float KeySize = 40.f, float TextSize = 26.f);

	/** "2 h 14 min", "12 min", "under a minute". */
	SALTBOUND_API FText PlayTime(double Seconds);
	/** "just now", "6 minutes ago", "2 hours ago", "on 4 Oct". */
	SALTBOUND_API FText Ago(const FDateTime& When);
	/** "4 Oct". */
	SALTBOUND_API FText DayMonth(const FDateTime& When);
	/** "4 Oct, 22:41". */
	SALTBOUND_API FText When(const FDateTime& When);
	SALTBOUND_API FText Roman(int32 Number);
	/** The 16 points of the compass for a bearing (degrees clockwise from north). */
	SALTBOUND_API FString CompassPoint(float BearingDeg);
	/** Where a voyage was left: "at anchor off Ruin Isle", "under sail near Gull Rock", "adrift on the open sea". */
	SALTBOUND_API FText VoyageLine(const FSaltVoyageInfo& Info);
	/** Nearest named place within 1.5 km of a world position (cm), or null. */
	SALTBOUND_API const FSaltPlace* PlaceNear(const FVector& WorldCm);

	// The design's colours (sRGB): the sub-lines' grey, the detail text, ink on paper, faded ink.
	inline FLinearColor SubGrey() { return SaltUI::SRGB(0xB9, 0xC3, 0xC4); }
	inline FLinearColor DetailGrey() { return SaltUI::SRGB(0xC6, 0xCF, 0xD0); }
	inline FLinearColor FadedInk() { return SaltUI::SRGB(0x6B, 0x4D, 0x33); }
}

/**
 * One line of a menu (UI_Design's painted lettering): the rose turns in to point at it when it is chosen and the rope grows
 * under it; an optional italic line under the label. Place it at the top-left of its CSS box (the pointer sits left of the
 * label, the rope in the gap under the box).
 */
UCLASS()
class SALTBOUND_API USaltMenuItem : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Label size, pointer size, the label's indent (the pointer's room); call before it is shown. */
	void Setup(const FText& InLabel, float InSize = 48.f, float InPointer = 50.f, float InIndent = 65.f);
	void SetLabel(const FText& InLabel);
	void SetSub(const FText& InSub);
	void SetChosen(bool bInChosen, bool bInstant = false);
	bool IsChosen() const { return bChosen; }
	void SetEnabled(bool bInEnabled);
	bool IsItemEnabled() const { return bEnabled; }
	/** The CSS box's height (the label line, and the sub-line when there is one). */
	float GetBoxHeight() const;
	/** The box's width from its left edge (indent and the wider of label and sub-line). */
	float GetBoxWidth() const;
	const FText& GetLabel() const { return Label; }
	/** Tabs have the rose but no rope; unchosen ones are fainter (62 %). */
	bool bShowRope = true;
	float UnchosenOpacity = 0.72f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Layout();
	void ApplyLook();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UImage> PointerImage;
	UPROPERTY(Transient) TObjectPtr<USaltPaintedText> LabelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubText;
	UPROPERTY(Transient) TObjectPtr<UImage> Underline;

	FText Label;
	FText Sub;
	float Size = 48.f;
	float PointerSize = 50.f;
	float Indent = 65.f;
	bool bChosen = false;
	bool bEnabled = true;
	/** 0..1 towards chosen (0.35 s each way). */
	float Progress = 0.f;
};

/**
 * A full-screen menu screen (D66): the world behind blurred and shaded (or covered), its contents opening as the satchel's
 * (ApplyOpening), laid out at the design's 1080p numbers on a centre-anchored canvas. The player controller keeps the
 * screens on a stack (PushScreen / PopScreen); the top one has the keyboard. Keys: W / S or the arrows choose, A / D change,
 * E / Enter / Space use, Esc / B back, Q / E page where a screen has pages; the mouse hovers and clicks; the gamepad's d-pad,
 * A, B and bumpers do the same.
 */
UCLASS(Abstract)
class SALTBOUND_API USaltScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	ASaltPlayerController* GetSaltController() const;
	/** Pauses the game while it is on the stack (the pause menu and what is opened from it). */
	bool bPausesGame = false;
	/** Drawn over the screen under it (a question), which stays visible. */
	bool bOverlay = false;
	/** Shows the mouse cursor. */
	bool bWantsCursor = true;
	/** Esc / B. Pops itself by default. */
	virtual void Back();
	/** It is the top of the stack again (a screen over it closed). */
	virtual void OnRevealed() {}

	// Menu lines (the start and pause menus): for tests and the dev commands.
	int32 GetItemCount() const { return Items.Num(); }
	int32 GetChosenItem() const { return Chosen; }
	void ChooseItem(int32 Index, bool bInstant = false);
	/** Uses a line (as E or a click on it). */
	void UseItem(int32 Index);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** The backdrop: blur and shades over the world (or a cover), into Root under Contents. */
	virtual void BuildBackdrop();
	/** The screen's contents, into Contents at design coordinates. */
	virtual void Build() {}

	// Input, already mapped (return false to let the key through).
	virtual void Navigate(int32 Dx, int32 Dy);
	virtual void Accept();
	virtual void Page(int32 Dir) {}
	/** Any key first (the keys screen listens for one). Return true when used. */
	virtual bool RawKey(const FKey& Key, bool bRepeat) { return false; }
	virtual void RawKeyUp(const FKey& Key) {}
	virtual void PointerMove(const FVector2D& Design);
	virtual void PointerDown(const FVector2D& Design);
	virtual void PointerUp(const FVector2D& Design) {}
	/** A line was used (E or a click). */
	virtual void ItemUsed(int32 Index) {}

	/** Adds a menu line at the design position (its CSS box's top-left). */
	USaltMenuItem* AddItem(const FText& Label, FVector2D Design, float Size = 48.f, float Pointer = 50.f, float Indent = 65.f);
	/** Restacks the lines from Top with the gap between their boxes (the design's 1.25cqw = 24 px). */
	void StackItems(FVector2D TopLeft, float Gap = 24.f);
	int32 ItemAt(const FVector2D& Design) const;

	/** Places the controls row at the bottom (left at x, or centred when Centre). */
	void PlaceHints(const TArray<SaltMenu::FHint>& InHints, float X, bool bCentre);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Contents;
	UPROPERTY(Transient) TObjectPtr<UBackgroundBlur> Blur;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Backdrop;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> HintsRow;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltMenuItem>> Items;
	TArray<FVector2D> ItemPos;
	int32 Chosen = 0;

	/** How the world behind shows. */
	enum class EBackdrop : uint8 { Blurred, Title, Dim, Cover };
	EBackdrop BackdropKind = EBackdrop::Blurred;
	float BlurStrength = 14.f;
	/** The pause menu's dark shade from the left (else an even one). */
	bool bShadeLeft = true;
	float OpenTime = 0.f;
	static constexpr float OpenDuration = 0.22f;
};

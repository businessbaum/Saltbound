// Saltbound - keys and buttons (D66, Docs/Design/Menu_Ideas/Menus.html "Keys and buttons"): every action with its key on a
// parchment scrap (as the prompts show it) and its gamepad button; a key is changed by pressing the new one.

#pragma once

#include "CoreMinimal.h"
#include "UI/SaltMenuKit.h"
#include "SaltKeysWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;

/**
 * W / S choose an action, E listens for its new key ("Press a key or button", a quill caret blinking); the next key pressed
 * is taken. A key another action uses is explained on a scrap: E swaps the two, Esc keeps both on it. Esc while listening
 * changes nothing. The keys that open the pause menu (Esc, F10) and take the tools (1 to 5) cannot be taken; the mouse and the
 * gamepad are not changed here. The last row puts every key back.
 */
UCLASS()
class SALTBOUND_API USaltKeysWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltKeysWidget(const FObjectInitializer& ObjectInitializer);

	enum class EState : uint8 { Browse, Listen, Clash };
	EState GetState() const { return State; }
	int32 GetChosenRow() const { return Row; }
	void ChooseRow(int32 InRow);
	/** Listens for the chosen action's key (as E). */
	void Listen();
	/** A key pressed while listening (tests send it here). */
	void Offer(const FKey& Key);
	/** Answers a clash: swap (E) or keep both (Esc). */
	void Resolve(bool bSwap);

	virtual void Back() override;

protected:
	virtual void Build() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void Navigate(int32 Dx, int32 Dy) override;
	virtual void Accept() override;
	virtual bool RawKey(const FKey& Key, bool bRepeat) override;
	virtual void PointerMove(const FVector2D& Design) override;
	virtual void PointerDown(const FVector2D& Design) override;

private:
	void Rebuild();
	void Commit(FName Id, const FKey& Key);
	void ShowNote(const FText& Text);
	int32 RowCount() const;
	int32 RowAt(const FVector2D& Design) const;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Table;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> NoteScrap;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NoteText;
	UPROPERTY(Transient) TObjectPtr<UImage> Caret;
	EState State = EState::Browse;
	int32 Row = 0;
	FKey Pending;
	FName ClashWith;
	bool bClashFixed = false;
	float Blink = 0.f;
	float NoteAge = 0.f;
};

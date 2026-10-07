// Saltbound - the start menu (D66, Docs/Design/Menu_Ideas/Menus.html "Night sea", owner-chosen): the launch intro's own title
// where the intro leaves it, painted lines under it, over the world where the last voyage was saved.

#pragma once

#include "CoreMinimal.h"
#include "UI/SaltMenuKit.h"
#include "SaltStartMenuWidget.generated.h"

/**
 * Continue (the voyage last played: its place, how the boat lies, the play time), New voyage, Voyages, Settings, Quit. With no
 * voyage saved yet Continue is not there and New voyage is chosen. The world behind goes on (it is not paused); the play clock
 * does not run.
 */
UCLASS()
class SALTBOUND_API USaltStartMenuWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltStartMenuWidget(const FObjectInitializer& ObjectInitializer);

	enum class EItem : uint8 { Continue, NewVoyage, Voyages, Settings, Quit };
	static int32 FindItem(EItem Item) { return (int32)Item; }
	/** Re-reads the voyages (Continue's line, whether it shows). */
	void Refresh();

	virtual void Back() override {}
	virtual void OnRevealed() override { Refresh(); }

	/** D79 (owner: "Create a small animation between continue and actually loading into the game, its too sudden"): Continue
	 *  lets go of the world. The lines and the title drift left and fade (0.35 s), the dusk shades lift (LeaveSeconds, eased),
	 *  the cursor goes at once; then the screen leaves the stack and play (the HUD, the keys) takes over. */
	void BeginLeave();
	bool IsLeaving() const { return bLeaving; }
	/** Ends the leave now (tests). */
	void FinishLeave();
	static constexpr float LeaveSeconds = 1.2f;

	/** Where the title sits (the intro's last frame, cut to its box: Tools/textures/gen_menu_parts.py TITLE_BOX). */
	static constexpr float TitleX = 101.f;
	static constexpr float TitleY = 155.f;
	static constexpr float TitleW = 732.f;
	static constexpr float TitleH = 150.f;

protected:
	virtual void Build() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void Navigate(int32 Dx, int32 Dy) override;
	virtual void ItemUsed(int32 Index) override;

private:
	bool bLeaving = false;
	float LeaveT = 0.f;
};

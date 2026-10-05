// Saltbound - the voyages (save slots, D66, Docs/Design/Menu_Ideas/Menus.html "Voyages"): three pages, each with a picture of
// where the voyage was last saved, its place and play time.

#pragma once

#include "CoreMinimal.h"
#include "Core/SaltSaveSubsystem.h"
#include "UI/SaltMenuKit.h"
#include "SaltVoyagesWidget.generated.h"

class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;

/**
 * A / D choose a page, E sails on with it (loads it, or starts a new voyage on an empty page), holding X scraps it (the ink
 * soaks the key as the prompts' holds do; no extra question), Esc goes back. The voyage in play cannot be scrapped from the
 * pause menu.
 */
UCLASS()
class SALTBOUND_API USaltVoyagesWidget : public USaltScreen
{
	GENERATED_BODY()

public:
	USaltVoyagesWidget(const FObjectInitializer& ObjectInitializer);

	/** Opened from the pause menu (in play) rather than the title. */
	bool bInPlay = false;
	int32 GetChosenVoyage() const { return Voyage; }
	void ChooseVoyage(int32 Index);
	/** Sails on with the chosen page (as E). */
	void SailOn();
	/** Scraps the chosen voyage at once (as a full hold of X). */
	bool Scrap();
	static constexpr float ScrapHold = 1.1f;

	virtual void OnRevealed() override { Refresh(); }

protected:
	virtual void Build() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void Navigate(int32 Dx, int32 Dy) override;
	virtual void Accept() override { SailOn(); }
	virtual bool RawKey(const FKey& Key, bool bRepeat) override;
	virtual void RawKeyUp(const FKey& Key) override;
	virtual void PointerMove(const FVector2D& Design) override;
	virtual void PointerDown(const FVector2D& Design) override;

private:
	void Refresh();
	void Layout();
	bool CanScrap() const;
	int32 CardAt(const FVector2D& Design) const;

	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Pictures;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SoakMid;
	TArray<FSaltVoyageInfo> Infos;
	int32 Voyage = 0;
	bool bHolding = false;
	float Hold = 0.f;
	float Time = 0.f;
};

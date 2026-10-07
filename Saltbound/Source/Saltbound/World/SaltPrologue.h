// Saltbound - the first hour's prologue (FH1, D77): the castaway's start and the story's first beats.
//
// A new voyage begins on the south-west beach by the wreck (D71) at dawn with nothing: no tools, an empty satchel, the
// Cartographer's boat at the pier with her sail ripped. The Keeper gives the shovel and a lantern, the sea chest aboard the
// Cartographer's compass and spyglass (and, with FH3, the log); the buried canvas mends the sail, and only then does it set.
// The beats are saved with the game (USaltSaveSubsystem); Docs/Design/FirstHour.md has the whole hour.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltPrologue.generated.h"

class ASaltPlayerCharacter;
class USaltInteractableComponent;

namespace SaltStory
{
	/** The first hour's beats, in the order a player meets them. */
	inline const FName Prologue = TEXT("Prologue");         // this voyage began with the prologue
	inline const FName Woke = TEXT("Woke");                 // on the wreck beach
	inline const FName Chest = TEXT("Chest");               // the Cartographer's sea chest opened
	inline const FName Keeper = TEXT("Keeper");             // the Harbour Keeper gave the shovel and a lantern
	inline const FName Cache = TEXT("Cache");               // the buried canvas dug up
	inline const FName SailMended = TEXT("SailMended");     // the ripped sail mended: she sails
	inline const FName CastOff = TEXT("CastOff");           // under sail, away from the pier
	/** D85: the letter for the Cartographer in the castaway's pocket (from the start), dried at the fire pit, its name known to the
	 *  Keeper (Lines.json records that one). */
	inline const FName Letter = TEXT("Letter");
	inline const FName LetterDried = TEXT("LetterDried");
	inline const FName KeeperLetter = TEXT("KeeperLetter");
	/** The island's fire pit (island_shape.py PLACES), where the letter dries (m). */
	constexpr double FirePitXM = -34.0;
	constexpr double FirePitYM = -41.0;

	/** Where the castaway wakes: a few steps inland of the wreck on the south-west beach (m), facing it (degrees). */
	constexpr double WakeXM = -62.0;
	constexpr double WakeYM = -104.0;
	constexpr float WakeYaw = -148.f;
	/** Dawn of the first day (D72's clock). */
	constexpr float StartHour = 5.5f;
	/** The sail at the start: ripped (D74's tear 2; the cache's two sailcloths mend it). */
	constexpr int32 StartTear = 2;
	/** The dig spot of the Cartographer's cache (World/SaltDig.cpp). */
	inline const FName CacheSpot = TEXT("CartographerCanvas");
}

UCLASS()
class SALTBOUND_API USaltPrologueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** A new voyage (USaltSaveSubsystem::StartNewVoyage, between restoring the world and saving it): the castaway's start.
	 *  bWakeUp: the view rises from the sand (tests skip it). */
	void Begin(bool bWakeUp = true);
	/** Records a beat at the clock's day and hour (once). */
	void Reach(FName Beat);
	/** Checks the beats that follow from the world (the cache dug, the sail mended, cast off) now; the tick does it twice a
	 *  second. */
	void UpdateBeats();
	/** The sea chest aboard: the Cartographer's compass and spyglass (FH3: the log). */
	UFUNCTION() void HandleChest(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	USaltInteractableComponent* GetChest() const { return Chest; }
	/** D85: the letter (the castaway carries it from the start; dry once it has been by the fire). */
	bool HasLetter() const;
	bool IsLetterDry() const;
	/** Opens the letter (L): wet, the ink has run; dried, it all reads. */
	void ReadLetter();
	/** The fire pit's use: hold E to dry the letter by the embers. */
	UFUNCTION() void HandleDryLetter(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	USaltInteractableComponent* GetFireSpot() const { return FireSpot; }
	/** The castaway's spot on the ground (cm). */
	FVector GetWakeSpot() const;
	/** The clock now as day * 24 + hour. */
	float Now() const;

private:
	void PlaceBeachFinds();
	void AttachChest();
	void ApplyState();
	void PlaceFireSpot();
	void BindLetterKey();
	void HandleLetterKey();

	UPROPERTY(Transient) TObjectPtr<USaltInteractableComponent> Chest;
	UPROPERTY(Transient) TObjectPtr<USaltInteractableComponent> FireSpot;
	UPROPERTY(Transient) TObjectPtr<class UInputMappingContext> LetterContext;
	UPROPERTY(Transient) TObjectPtr<class UInputAction> LetterAction;
	TWeakObjectPtr<UObject> LetterInput;
	/** The note about the letter comes when the castaway has stood up. */
	float LetterNoticeIn = 0.f;
	FDelegateHandle LoadedHandle;
	float CheckIn = 0.f;
	bool bStarted = false;
};

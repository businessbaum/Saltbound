// Saltbound - saving and loading the game (D46): the boat, the player and the sea.

#pragma once

#include "CoreMinimal.h"
#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Core/SaltItems.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaltSaveSubsystem.generated.h"

/** One saved game. The player is stored in the boat's frame while aboard, so they wake up on their deck. */
UCLASS()
class SALTBOUND_API USaltSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 Version = 5;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FString Map;

	UPROPERTY() bool bHasBoat = false;
	UPROPERTY() FVector BoatLocation = FVector::ZeroVector;    // cm
	UPROPERTY() float BoatYaw = 0.f;
	UPROPERTY() ESaltBoatHold BoatHold = ESaltBoatHold::Moored;
	UPROPERTY() bool bBoatSail = false;

	UPROPERTY() bool bPlayerAboard = false;
	UPROPERTY() FVector PlayerFeet = FVector::ZeroVector;      // world cm, or the boat's local space when aboard
	UPROPERTY() float PlayerYaw = 0.f;                         // world, or relative to the boat when aboard
	UPROPERTY() float PlayerPitch = 0.f;

	UPROPERTY() FName Preset;
	UPROPERTY() FName Region;
	UPROPERTY() FName Weather;
	UPROPERTY() FName SeaState;
	UPROPERTY() FVector Wind = FVector::ZeroVector;            // from (deg), speed (cm/s), gustiness
	// D72: the day's clock (hours 0..24; -1 in saves from before it, which keep only Preset) and the day of the voyage.
	UPROPERTY() float TimeOfDay = -1.f;
	UPROPERTY() int32 DayNumber = 1;
	// D73: the dig spots' stages (1 dug up, 2 emptied; missing = buried) and the tools the player owns (empty in older saves:
	// the start's tools stay).
	UPROPERTY() TMap<FName, int32> DigStages;
	// FH1 (D77): the story's beats reached and when (day * 24 + hour).
	UPROPERTY() TMap<FName, float> StoryBeats;
	UPROPERTY() bool bHasOwnedTools = false;
	UPROPERTY() TArray<FName> OwnedTools;
	// D74: the boat's damage (holes and patches, water aboard, the sail's tears).
	UPROPERTY() TArray<FSaltHullHole> HullHoles;
	UPROPERTY() float BilgeWaterKg = 0.f;
	UPROPERTY() int32 SailTear = 0;

	// D50: the satchel and the pickups already taken (by PickupId).
	UPROPERTY() bool bHasInventory = false;
	UPROPERTY() TArray<FSaltItemStack> Inventory;
	UPROPERTY() TArray<FName> Collected;

	// D52: the stores (the boat's barrel and crate) by their ids.
	UPROPERTY() TArray<FSaltSavedStore> Stores;

	// D55 (version 4): the islands found (SaltPlaces ids); each one's landfall banner shows only once.
	UPROPERTY() TArray<FName> FoundPlaces;

	// D62 (version 5): the hub ship. Where the boat lies moored (its mooring: the pier or one of the hub's berths), the berth
	// the hub lies at, the zones unlocked (L1..L17, none yet), the salt purse, the sea gate.
	UPROPERTY() FSaltMooring BoatMooring;
	UPROPERTY() FName HubBerth;
	UPROPERTY() TArray<FName> HubUnlocked;
	UPROPERTY() int32 Salt = 0;
	UPROPERTY() bool bSeaGateOpen = true;

	// D66: how long this voyage has been played (seconds; the menus show it).
	UPROPERTY() double PlayTime = 0.0;
};

/** What the start menu and the voyages screen show of a voyage (a save slot), D66. */
struct FSaltVoyageInfo
{
	bool bExists = false;
	FDateTime SavedAt;
	double PlayTime = 0.0;
	/** The nearest named place ("Ruin Isle"), or "the open sea". */
	FText Place;
	/** "moored", "at anchor", "under sail", "adrift". */
	FText Boat;
	ESaltBoatHold Hold = ESaltBoatHold::Free;
	/** Near a named place (else the open sea). */
	bool bNearPlace = false;
	/** The picture taken when it was saved (Saved/SaveGames/<slot>.png), empty when there is none. */
	FString Picture;
};

/**
 * Saves to and loads from a slot (Saved/SaveGames/<Slot>.sav). The game saves when the player moors the boat, when
 * they quit and from the pause menu; the packaged game continues from the last save on start (play-in-editor does
 * not, so tests and editor sessions start from the map as built).
 */
UCLASS()
class SALTBOUND_API USaltSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static USaltSaveSubsystem* Get(const UObject* WorldContext);

	bool SaveGame(UWorld* World, const TCHAR* Reason);
	/** A save begins / has been written (the HUD shows the save sign in between, D55). */
	FSimpleMulticastDelegate OnSaveStarted;
	FSimpleMulticastDelegate OnSaveFinished;
	bool LoadGame(UWorld* World);
	bool HasSave() const;
	/** When the slot was last written (FDateTime() when there is none). */
	FDateTime GetSaveTime() const;

	/** Saves (with a short notice) after something the player finished: mooring, quitting. */
	void Autosave(UWorld* World, const TCHAR* Reason) { SaveGame(World, Reason); }

	/** The slot in use: "Main", or a test slot while automated tests run. */
	FString Slot = TEXT("Main");

	/** Pickups taken this game (saved with it; pickups hide themselves when listed). */
	void MarkCollected(FName PickupId) { if (!PickupId.IsNone()) { Collected.Add(PickupId); } }
	bool IsCollected(FName PickupId) const { return !PickupId.IsNone() && Collected.Contains(PickupId); }
	void ClearCollected() { Collected.Reset(); }

	/** Islands found this game (D55, saved with it). */
	void MarkFound(FName PlaceId) { if (!PlaceId.IsNone()) { Found.Add(PlaceId); } }
	bool IsFound(FName PlaceId) const { return Found.Contains(PlaceId); }
	void ClearFound() { Found.Reset(); }
	bool AnyFound() const { return Found.Num() > 0; }

	/** D73: how far each dig spot has come (0 buried, 1 dug up, 2 emptied), saved with the game. */
	int32 GetDigStage(FName SpotId) const { const int32* S = DigStages.Find(SpotId); return S ? *S : 0; }
	void SetDigStage(FName SpotId, int32 Stage) { if (!SpotId.IsNone()) { DigStages.Add(SpotId, Stage); } }
	void ClearDigStages() { DigStages.Reset(); }

	/** FH1 (D77): the story's beats (the first hour's steps, World/SaltPrologue.h) and when each was reached (day * 24 + hour),
	 *  saved with the game. */
	bool HasBeat(FName Beat) const { return StoryBeats.Contains(Beat); }
	float GetBeatTime(FName Beat) const { const float* T = StoryBeats.Find(Beat); return T ? *T : -1.f; }
	const TMap<FName, float>& GetBeats() const { return StoryBeats; }
	/** Records a beat the first time (later calls keep the first time) and tells OnStoryBeat. */
	void SetBeat(FName Beat, float When);
	void ClearBeats() { StoryBeats.Reset(); }
	DECLARE_MULTICAST_DELEGATE_OneParam(FSaltStoryBeat, FName);
	FSaltStoryBeat OnStoryBeat;

	// The hub (D62), saved with the game -----------------------------------------------------------------------------
	/** The salt purse: one number, not a satchel item; nothing spends it yet. */
	int32 GetSalt() const { return Salt; }
	void SetSalt(int32 Amount) { Salt = FMath::Max(Amount, 0); }
	/** The hub's locked zones (L1..L17) that have been opened; the gates follow (OnHubChanged). */
	bool IsUnlocked(FName ZoneId) const { return Unlocked.Contains(ZoneId); }
	void SetUnlocked(FName ZoneId, bool bUnlocked);
	void ClearUnlocked() { Unlocked.Reset(); OnHubChanged.Broadcast(); }
	TArray<FName> GetUnlocked() const { return Unlocked.Array(); }
	/** The berth the hub lies at (ASaltHubShip keeps it); None: her default berth. */
	FName HubBerth;
	/** The sea gate's state to keep (the gate reads it when its level streams in). */
	bool bSeaGateOpen = true;
	/** A game was loaded (the hub moves to its berth, the gates follow the zones). */
	FSimpleMulticastDelegate OnGameLoaded;
	/** The hub's state changed (zones unlocked). */
	FSimpleMulticastDelegate OnHubChanged;

	// Voyages (D66): three save slots; "Main" (the slot of D46) is the first, so older saves are Voyage I -------------
	static constexpr int32 VoyageCount = 3;
	static FString VoyageSlot(int32 Index);
	/** The voyage the slot in use belongs to (0..2), or INDEX_NONE (a test slot). */
	int32 GetActiveVoyage() const;
	/** Uses this voyage's slot from now on and remembers it for the start menu's Continue. */
	void SetActiveVoyage(int32 Index);
	FSaltVoyageInfo GetVoyageInfo(int32 Index) const;
	bool DeleteVoyage(int32 Index);
	/** The map as built, taken once at the start before anything is loaded (a quiet save to the "NewVoyage" slot). */
	void TakeNewVoyageSnapshot(UWorld* World);
	bool HasNewVoyageSnapshot() const { return bHasSnapshot; }
	/** Starts a new voyage in the slot: the world goes back to the snapshot, the satchel and the stores to their start, and
	 *  it is saved there at once. */
	bool StartNewVoyage(UWorld* World, int32 Index);
	/** Puts the world back to the snapshot without saving (returning to the title of a voyage never saved). */
	bool RestoreNewVoyageSnapshot(UWorld* World);
	/** Seconds played in the voyage in use (counted by the controller while playing, saved with it). */
	double PlayTime = 0.0;
	/** Play time at the last save or load: what Return to the title would lose. */
	double GetUnsavedTime() const { return FMath::Max(PlayTime - PlayTimeAtSave, 0.0); }
	/** No save sign, notice or picture (the snapshot, the new voyage's switch). */
	bool bQuiet = false;

private:
	bool bHasSnapshot = false;
	double PlayTimeAtSave = 0.0;

	void Notify(UWorld* World, const FText& Text) const;

	TSet<FName> Collected;
	TSet<FName> Found;
	TSet<FName> Unlocked;
	int32 Salt = 0;
	TMap<FName, int32> DigStages;
	TMap<FName, float> StoryBeats;
};

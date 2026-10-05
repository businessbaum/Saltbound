// Saltbound - saving and loading the game (D46): the boat, the player and the sea.

#pragma once

#include "CoreMinimal.h"
#include "Boat/SaltBoat.h"
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

private:
	void Notify(UWorld* World, const FText& Text) const;

	TSet<FName> Collected;
	TSet<FName> Found;
	TSet<FName> Unlocked;
	int32 Salt = 0;
};

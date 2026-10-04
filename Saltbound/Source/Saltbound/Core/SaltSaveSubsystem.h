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
	UPROPERTY() int32 Version = 3;
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

private:
	void Notify(UWorld* World, const FText& Text) const;

	TSet<FName> Collected;
};

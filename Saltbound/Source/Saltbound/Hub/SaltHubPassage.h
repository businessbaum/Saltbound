// Saltbound - the hub ship's harbour passages (D70): the boat's arrival into the harbour and her departure out of it, as
// cinematics. Owner (2026-10-06): "The doors automatically open when the player ship comes near, if the ship tries to enter,
// the animation takes over. The animation shows how the ship is sailing into the Hub and docking on the docking station. Behind
// the ship the Doors are closing ... When the player wants to leave, he will be asked again when cast off, if confirmed the
// same animation mechanic comes. The Doors are opening and the ship is sailing out. Both animations with a cool smooth and epic
// camera pan. The animation start needs to perfectly align with the last camera angle of the player, same when the animation
// ends"; "always play an animation, but the first one is longer. no animation is skippable".

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubPassage.generated.h"

class ACameraActor;
class ASaltBoat;
class ASaltHubDoors;
class ASaltPlayerCharacter;

UENUM(BlueprintType)
enum class ESaltPassage : uint8
{
	None,
	Arrival,     // in through a pair of doors, a turn in the basin, along the canal to the docking station, moored
	Departure    // astern off the docking station, round in the basin, out through the doors she came in by
};

/**
 * One per world (Get finds or spawns it). A passage drives the boat along a path in the doors' frame (her own simulation
 * rests: ASaltBoat::SetScripted), raises and lowers the lifting bridge on her way, opens and shuts the doors, and flies a
 * camera through keyed shots: it starts exactly at the player's view and ends exactly at the player's eyes (live), where the
 * player gets the view back. Input and the HUD are off meanwhile; nothing skips it. The first arrival and the first
 * departure are the long ones (the hub's "seen" names in the save: HubArrivalSeen, HubDepartureSeen).
 */
UCLASS()
class SALTBOUND_API ASaltHubPassage : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubPassage();

	/** The world's passage (spawned on first use). */
	static ASaltHubPassage* Get(UWorld* World);

	/** In through Doors from Side (+1 starboard, -1 port): the boat must carry the player. */
	bool StartArrival(ASaltHubDoors* Doors, ASaltBoat* Boat, int32 Side);
	/** Casting off at one of the hub's berths: the question first ("Sail out?"), then the departure. */
	bool AskDeparture(ASaltBoat* Boat, ASaltPlayerCharacter* Player);
	/** Out by the doors of the station she lies at, on the side she came in by (starboard after a load). */
	bool StartDeparture(ASaltBoat* Boat);

	bool IsRunning() const { return Kind != ESaltPassage::None; }
	ESaltPassage GetKind() const { return Kind; }
	float GetElapsed() const { return Elapsed; }
	float GetDuration() const { return Duration; }
	/** The last passage was a first (long) one (tests). */
	bool WasLong() const { return bLong; }
	/** How the last one went (tests): its kind, its length, its station and side. */
	FString Describe() const;

	/** The doors whose docking station a mooring belongs to (or the nearest doors to Near). */
	static ASaltHubDoors* FindDoorsFor(const UWorld* World, FName MooringId, const FVector& Near);

	/** Long or short regardless of the save (-1 back to the save; tests and captures). */
	int32 ForceLong = -1;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	/** A point of the boat's path in the doors' frame: where (cm), her heading (deg), her speed there (cm/s). */
	struct FPathPoint
	{
		FVector2D P;
		float Yaw = 0.f;
		float Speed = 0.f;
		float T = 0.f;
	};
	/** A camera key: fixed in the doors' frame (Pos, Look), behind the boat (Pos, Look in her frame), or the player's view. */
	enum class EKey : uint8 { StartView, EndView, Fixed, FixedOnBoat, Chase };
	struct FCamKey
	{
		EKey Kind = EKey::Fixed;
		float T = 0.f;
		FVector Pos = FVector::ZeroVector;
		FVector Look = FVector::ZeroVector;
		float Fov = 62.f;
	};
	struct FView
	{
		FVector Pos = FVector::ZeroVector;
		FVector Look = FVector::ZeroVector;
		float Fov = 90.f;
		float Roll = 0.f;
	};

	bool Begin(ESaltPassage InKind, ASaltHubDoors* Doors, ASaltBoat* Boat, int32 Side);
	void Finish();
	void BuildArrival(const FVector2D& StartLocal, float StartYaw, float StartSpeed);
	void BuildDeparture();
	void AddPoint(const FVector2D& P, float Yaw, float Speed);
	void TimePath(float TurnRateDeg);
	void SamplePath(float T, FVector2D& OutP, float& OutYaw) const;
	FView EvaluateKey(const FCamKey& Key) const;
	FView PlayerView() const;
	void UpdateCamera(float DeltaSeconds);
	void Events();
	bool PlayerAboard(ASaltBoat* Boat) const;
	void SetPlayerControl(bool bOn);

	FTransform Frame() const;
	FVector LocalToWorld(const FVector& Local) const;

	ESaltPassage Kind = ESaltPassage::None;
	TWeakObjectPtr<ASaltHubDoors> DoorsRef;
	TWeakObjectPtr<ASaltBoat> BoatRef;
	TWeakObjectPtr<ASaltPlayerCharacter> PlayerRef;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	int32 Side = 1;
	int32 Station = 0;
	bool bLong = false;
	float Elapsed = 0.f;       // camera time
	float PathTime = 0.f;      // the boat's time along the path (it waits for the doors and the inside)
	float Duration = 1.f;
	float PathDuration = 1.f;
	TArray<FPathPoint> Path;
	TArray<FCamKey> Keys;
	FView Start;
	/** The player's eyes this frame (their camera asked with the frame's time while ours is shown). */
	FView Live;
	bool bLive = false;
	/** Things done once on the way: the bridge raised / lowered, the doors shut, the sail taken in. */
	bool bBridgeDown = false;
	bool bDoorsShut = false;
	bool bSailIn = false;
	bool bHanded = false;
	/** The side each station's doors were last entered by (departures leave by it). */
	TMap<int32, int32> EntrySide;
	FString LastDescription;
};

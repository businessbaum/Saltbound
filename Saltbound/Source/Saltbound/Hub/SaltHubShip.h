// Saltbound - the hub ship in the world (D62, phase 3 of the hub prompt): her level streamed in at her berth, following the
// player from harbour to harbour, out of sight.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ocean/SaltWaveTypes.h"
#include "SaltHubShip.generated.h"

class ASaltHubBerth;
class ULevelStreamingDynamic;

/**
 * Places her level (L_Hub, the ship with her pivot at its origin, bow to +X) at the active berth with
 * ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr, and unloads the old instance when she moves.
 *
 * Follow rule: she lies at the berth of the harbour the player last moored at or sailed to (within the berth's
 * HarbourRadiusM), and moves only while the player is more than FollowDistanceM from both berths (checked every
 * CheckInterval s), never while the boat lies moored to her. First sight: hidden until the first landfall (an island found),
 * then she appears when the player is far from her berth or not looking that way; sb.Hub.Show 1 shows her at once.
 * While she lies at a berth, its calm zones are added to the ocean.
 */
UCLASS()
class SALTBOUND_API ASaltHubShip : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubShip();

	/** Her level: /Game/Saltbound/Maps/L_Hub (build_hub_level.py). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TSoftObjectPtr<UWorld> HubLevel;
	/** She moves only while the player is farther than this from both berths (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "m")) float FollowDistanceM = 2500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "s", ClampMin = "0.1")) float CheckInterval = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") bool bHiddenUntilLandfall = true;
	/** She first appears only outside this half-angle of the player's view (deg), or beyond FollowDistanceM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "deg")) float ViewHalfAngle = 75.f;

	static ASaltHubShip* Find(const UWorld* World);

	/** sb.Hub.Show: 1 shows her now, 0 hides her, -1 back to the rule. */
	void SetShowOverride(int32 Override);
	/** sb.Hub.Berth: move to a berth now (dev; the follow rule otherwise). */
	bool MoveToBerth(FName BerthId);
	/** Checks the rule now (tests). */
	void EvaluateNow() { Evaluate(); }

	FName GetBerthId() const { return BerthId; }
	FName GetTargetBerthId() const { return TargetBerthId; }
	bool IsShown() const { return bShown; }
	/** Her level is loaded and visible at the berth. */
	bool IsLoaded() const;
	ASaltHubBerth* FindBerth(FName Id) const;
	ASaltHubBerth* GetBerth() const { return FindBerth(BerthId); }
	TArray<ASaltHubBerth*> GetBerths() const;

	/** Moves her to a berth regardless of the rule (tests). */
	void PlaceAt(ASaltHubBerth* Berth);

	/** D70: everything inside her (L_HubInside: the decks and rooms, the harbour, its lights, gates and moorings), streamed in at
	 *  her berth apart from her outside (L_Hub): loaded while the player is near her, shown before her doors open, out again
	 *  when they sail away; never while the player or a boat is in her or a passage runs. None: one level holds all of her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TSoftObjectPtr<UWorld> InsideLevel;
	/** Loaded (not shown) within this of her hull (m), shown within InsideShowM, unloaded beyond InsideUnloadM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "m")) float InsideLoadM = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "m")) float InsideShowM = 140.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "m")) float InsideUnloadM = 450.f;
	/** Her inside is loaded and shown (with no separate inside: her level is). */
	bool IsInsideVisible() const;
	/** The harbour is there to sail into: the world's hub ship shows her inside (or there is no hub ship). */
	static bool IsInsideReady(const UWorld* World);
	/** Load and show her inside now and keep it until the player is away again (bBlock: before this returns). */
	void RequireInside(bool bBlock);
	/** A passage runs: her inside stays whatever the distance. */
	void SetInsideHeld(bool bHeld);
	ULevelStreamingDynamic* GetInsideInstance() const { return Inside; }
	/** How far a world point is from her hull's footprint (m; 0 within it), and whether it is in her. */
	double DistanceToHullM(const FVector& World) const;
	bool IsInHull(const FVector& World) const;
	/** Her level as streamed in (null while she is not shown). */
	ULevelStreamingDynamic* GetInstance() const { return Instance; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void Evaluate();
	void Unload();
	void SetCalmZones(ASaltHubBerth* Berth);
	bool PlayerView(FVector& OutLocation, FVector& OutForward) const;
	bool BoatMooredToHer() const;
	void HandleGameLoaded();
	void UpdateInside(bool bBlock);
	void UnloadInside();
	void ApplyShelters();

	UPROPERTY(Transient) TObjectPtr<ULevelStreamingDynamic> Instance;
	FName BerthId;
	FName TargetBerthId;
	bool bShown = false;
	int32 ShowOverride = -1;
	float Accumulated = 0.f;
	int32 InstanceCount = 0;
	TArray<FSaltCalmZone> AddedZones;
	FDelegateHandle LoadedHandle;
	UPROPERTY(Transient) TObjectPtr<ULevelStreamingDynamic> Inside;
	bool bInsideHeld = false;
	bool bInsideForced = false;
	/** The player has been near her since her inside was required (it is let go once they leave again). */
	bool bForcedNear = false;
	bool bShelterOn = false;
	float InsideClock = 0.f;
};

// Saltbound - the hub ship's hidden side doors (D64; owner: "a cavity in the bottom deck filled with water so other ships can
// enter from THE SIDE ... not a big obvious gate, it's the hull that opens inside like a double door"; hub.doors in
// HubShip_Data.json, the leaves' hinges, rope eyes and blocks in Art/Generated/hub_doors.json).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubDoors.generated.h"

class ASaltPlayerCharacter;
class UStaticMeshComponent;
class USaltInteractableComponent;

/** One leaf of a pair (build_hub_doors.py: built where it stands shut, its pivot on its hinge axis at its foot). */
USTRUCT(BlueprintType)
struct FSaltHubDoorLeaf
{
	GENERATED_BODY()

	/** Its mesh: SM_Hub_<Mesh>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName Mesh;
	/** Its hinge axis at its foot (local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector Pivot = FVector::ZeroVector;
	/** Its yaw when open (degrees; + turns +X to +Y): it swings into the hall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") float OpenYaw = 90.f;
	/** Where its rope takes hold of it, shut (local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector Eye = FVector::ZeroVector;
	/** The fixed block under the gun deck its rope runs to (local cm); from there down to the capstan is the frame's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector Block = FVector::ZeroVector;
	/** The fore leaf follows the aft one (the master) by LagDegrees: it opens second and shuts first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") bool bFollows = false;
};

/**
 * The actor's origin is the middle of the doors at the waterline (ship x 36, y 0, z 0), +X forward along the ship. Two pairs
 * of leaves, one in each side, swing in 90 degrees about hinge posts standing in the hall. Shut, they are the hull's own
 * planking. They open on their own for a boat that comes near outside (the dock watch) and shut again a while after it is in
 * and clear of their swing; E at a capstan on the aft quay opens or shuts them by hand. They never swing into a boat. Not
 * saved: they start shut, and the watch lets a boat in whenever it comes home.
 */
UCLASS()
class SALTBOUND_API ASaltHubDoors : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubDoors();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	TArray<FSaltHubDoorLeaf> Leaves;

	/** How far the fore leaf trails the aft one (hub_doors.json lag_deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "deg"))
	float LagDegrees = 10.f;

	/** Seconds for the whole swing (hub.doors.seconds); it eases in and out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (ClampMin = "1", Units = "s"))
	float TravelSeconds = 10.f;

	/** Turns of the capstans for the whole swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	float CapstanTurns = 5.f;

	/** The capstans' axes on the aft quay (local cm, on the deck). Up to two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	TArray<FVector> CapstanSpots;

	/** The dock watch opens for a boat outside the hull within this distance of a pair (hub.doors.open_m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm"))
	float WatchOpenCm = 4500.f;

	/** It shuts what it opened when no boat has been near or in the doorway for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "s"))
	float WatchCloseSeconds = 20.f;

	/** The hull's half-breadth at the doors (local cm): outside it a boat is coming or going. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm"))
	float HullHalfCm = 1950.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bWatch = true;

	/** D70: which pair this is (hub.doors.stations: 0 aft, 1 fore), the docking station its passages moor at, and the boat's
	 *  pose there (local cm, her heading as its yaw). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") int32 Station = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName MooringId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FTransform DockPose;
	/** D70: a boat carrying the player that heads into the open doorway from within this (cm) starts the arrival. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float ArrivalReachCm = 2200.f;

	UFUNCTION(BlueprintCallable, Category = "Hub") bool Open();
	UFUNCTION(BlueprintCallable, Category = "Hub") bool Close();
	UFUNCTION(BlueprintCallable, Category = "Hub") bool Toggle();
	/** Snap to open (1) or shut (0) at once (tests, loading). */
	void SetOpenAmountNow(float Amount);
	/** 0 shut .. 1 open. */
	float GetOpenAmount() const { return Amount; }
	bool IsOpen() const { return Amount >= 1.f - KINDA_SMALL_NUMBER && Target >= 1.f; }
	bool IsClosed() const { return Amount <= KINDA_SMALL_NUMBER && Target <= 0.f; }
	bool IsMoving() const { return !FMath::IsNearlyEqual(Amount, Target); }
	/** A leaf's yaw now (degrees). */
	float GetLeafYaw(int32 Leaf) const;
	/** Is a boat inside any leaf's swing (no move then)? */
	bool IsSwingBlocked() const;
	const FString& GetLastReason() const { return LastReason; }

	static constexpr int32 MaxLeaves = 4;
	static constexpr int32 MaxCapstans = 2;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	void Resolve();
	void Apply();
	void Watch(float DeltaSeconds);
	bool MoveTo(float NewTarget, const TCHAR* Why);
	float LeafFraction(int32 Leaf) const;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> LeafMeshes;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> Ropes;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> Capstans;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<USaltInteractableComponent>> Handles;

	/** 0 shut .. 1 open, where they are and where they go. */
	float Amount = 0.f;
	float Target = 0.f;
	float Travel = 1.f;
	float From = 0.f;
	/** The watch opened them (and so may shut them). */
	bool bByWatch = false;
	/** A boat is in the rest of the swing: the leaves hold where they are. */
	bool bHeld = false;
	float QuietSeconds = 0.f;
	float WatchClock = 0.f;
	FString LastReason;
};

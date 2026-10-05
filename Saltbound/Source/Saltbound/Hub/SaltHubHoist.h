// Saltbound - the Well's hoist (D62; spaces.well.hoist, counterweight, hoist_brake, gantry in HubShip_Data.json): a cage on
// ropes from the gantry's sheaves, three stops, the player rides inside.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubHoist.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USaltInteractableComponent;
class USaltStationComponent;
class ASaltPlayerCharacter;

/**
 * The actor stands at the cage's floor centre at its lowest stop (the quay deck), +X towards the bow. E in the cage rides it
 * (a station: W up, S down, to the next stop; easing in and out; the rider is held inside and steps out only at a stop);
 * E at a stop's door calls it there. The counterweight runs the other way on its own rope over the second sheave, down into
 * its railed bay when the cage is at the top. The rail gaps at the stops are guarded while the cage is elsewhere.
 */
UCLASS()
class SALTBOUND_API ASaltHubHoist : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubHoist();

	/** The stops: the cage floor's height over the actor (cm): the quay, the lower deck, the main deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<float> StopHeights;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") TArray<FText> StopNames;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "m/s")) float HoistSpeed = 1.6f;
	/** Acceleration and braking (m/s per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") float HoistAccel = 0.6f;
	/** The sheaves under the gantry's beam (actor space, cm): the cage's rope and the counterweight's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector CageSheave = FVector(0.f, 0.f, 2300.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector WeightSheave = FVector(-90.f, -230.f, 2300.f);
	/** The counterweight's bottom (actor space, cm) when the cage is at its top stop: in its bay on the chamber floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector WeightLow = FVector(-90.f, -230.f, 0.f);
	/** The cage's rope from its yoke (this high over the cage floor, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") float YokeHeight = 320.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") float WeightHeight = 150.f;
	/** The rail gap at each stop (actor space, relative to the stop's floor): its centre and half size; none at the quay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector GapCentre = FVector(-150.f, 0.f, 50.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector GapExtent = FVector(6.f, 130.f, 50.f);

	static constexpr int32 MaxStops = 3;

	/** Sends the cage to a stop (tests, the call points); false while it moves. */
	bool GoTo(int32 Stop);
	/** The stop it stands at (INDEX_NONE between stops). */
	int32 GetStop() const { return FMath::IsNearlyEqual(Height, StopZ(At), 1.f) && Speed == 0.f ? At : INDEX_NONE; }
	int32 GetTargetStop() const { return Target; }
	bool IsMoving() const { return Speed != 0.f || !FMath::IsNearlyEqual(Height, StopZ(Target), 1.f); }
	/** The cage floor's height over the actor (cm). */
	float GetHeight() const { return Height; }
	USaltStationComponent* GetStation() const { return Station; }
	USaltInteractableComponent* GetCall(int32 Stop) const { return Calls.IsValidIndex(Stop) ? Calls[Stop].Get() : nullptr; }
	/** Snap to a stop (tests). */
	void SetStopNow(int32 Stop);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void HandleCall(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	void Resolve();
	void Apply();
	float StopZ(int32 Stop) const { return StopHeights.IsValidIndex(Stop) ? StopHeights[Stop] : 0.f; }
	FText StopName(int32 Stop) const;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Cage;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Weight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> CageRope;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> WeightRope;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltStationComponent> Station;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<USaltInteractableComponent>> Calls;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UBoxComponent>> Guards;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> GuardRopes;

	float Height = 0.f;
	float Speed = 0.f;
	int32 At = 0;
	int32 Target = 0;
	bool bInputLatch = false;
};

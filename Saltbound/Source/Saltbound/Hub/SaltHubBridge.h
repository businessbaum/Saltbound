// Saltbound - a lifting bridge of the hub ship's harbour (D70; hub.bridges.lift in HubShip_Data.json): the quay along the canal
// crosses a basin on two leaves hinged at the basin's edges; they rise for a boat passing (every mast clears between them).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubBridge.generated.h"

class UStaticMeshComponent;

/**
 * The actor's origin is the bridge's middle on the quay's deck, +X along the ship. Its two leaves (SM_Hub_BridgeLeaf, each
 * pivoted on its hinge and built out along its own +X) hinge at -HalfSpanCm (the aft leaf) and +HalfSpanCm (the fore leaf,
 * turned round); raised, each pitches up OpenDegrees about its hinge. They carry their own collision. Not saved: lowered at
 * the start; the harbour's passages raise and lower them (ASaltHubPassage).
 */
UCLASS()
class SALTBOUND_API ASaltHubBridge : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubBridge();

	/** The door station (basin) it crosses: 0 aft, 1 fore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") int32 Station = 0;
	/** The canal's side its quay runs along: +1 starboard, -1 port. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") int32 Side = 1;
	/** Half the basin's width: each leaf's hinge from the middle (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float HalfSpanCm = 600.f;
	/** How far the leaves rise (hub.bridges.lift.open). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "deg")) float OpenDegrees = 80.f;
	/** Seconds for the whole lift; it eases in and out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "s", ClampMin = "0.5")) float TravelSeconds = 5.f;

	void Raise() { Target = 1.f; }
	void Lower() { Target = 0.f; }
	void SetOpenNow(float InAmount);
	/** 0 lowered .. 1 raised. */
	float GetOpenAmount() const { return Amount; }
	bool IsRaised() const { return Amount >= 1.f - KINDA_SMALL_NUMBER; }
	bool IsLowered() const { return Amount <= KINDA_SMALL_NUMBER; }

	/** The bridge at a station's side, in the world (null when the harbour is not loaded). */
	static ASaltHubBridge* Find(const UWorld* World, int32 Station, int32 Side);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void Resolve();
	void Apply();

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> AftLeaf;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> ForeLeaf;

	float Amount = 0.f;
	float Target = 0.f;
};

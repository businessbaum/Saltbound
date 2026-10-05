// Saltbound - the hub ship's sea gate: a portcullis over the dock mouth, raised and lowered on ropes from two capstans
// (D62; owner: "open and close it with ropes like a castle gate"; hub.seagate in HubShip_Data.json).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltSeaGate.generated.h"

class ASaltPlayerCharacter;
class UStaticMeshComponent;
class USaltInteractableComponent;

/**
 * The actor's origin is the gate's bottom centre where it stands closed (its spikes in the sill), +X forward along the
 * ship. The gate slides straight up by RaiseCm into the gatehouse; the four ropes hang from under their sheaves
 * (RopeTops) to the gate's head and pay in as it rises; both capstans turn while it moves. E at a capstan raises or
 * lowers it (not while it moves). Not saved: it starts open (bStartOpen), so a boat can always come home.
 */
UCLASS()
class SALTBOUND_API ASaltSeaGate : public AActor
{
	GENERATED_BODY()

public:
	ASaltSeaGate();

	/** How far the gate rises (cm): hub.seagate.raise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm"))
	float RaiseCm = 2650.f;

	/** The gate's height (cm): its head's eyes are this far above the origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm"))
	float GateHeightCm = 910.f;

	/** Seconds for the whole way up or down (hub.seagate.seconds); it eases in and out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (ClampMin = "1", Units = "s"))
	float TravelSeconds = 24.f;

	/** Turns of the capstans for the whole way (hub.seagate.turns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	float CapstanTurns = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bStartOpen = true;

	/** Where each rope hangs from (local cm; under its sheave in the gatehouse). Up to four. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	TArray<FVector> RopeTops;

	/** The capstans' axes on the quays (local cm, on the deck). Up to two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	TArray<FVector> CapstanSpots;

	UFUNCTION(BlueprintCallable, Category = "Hub") void Open();
	UFUNCTION(BlueprintCallable, Category = "Hub") void Close();
	UFUNCTION(BlueprintCallable, Category = "Hub") void Toggle();
	/** Snap to open (1) or closed (0) at once (tests, loading). */
	void SetOpenAmountNow(float Amount);
	/** 0 closed .. 1 open. */
	float GetOpenAmount() const { return Amount; }
	bool IsOpen() const { return Amount >= 1.f - KINDA_SMALL_NUMBER && Target >= 1.f; }
	bool IsClosed() const { return Amount <= KINDA_SMALL_NUMBER && Target <= 0.f; }
	bool IsMoving() const { return !FMath::IsNearlyEqual(Amount, Target); }
	/** The gate's height above its closed place (cm). */
	float GetLiftCm() const;

	static constexpr int32 MaxRopes = 4;
	static constexpr int32 MaxCapstans = 2;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	void Resolve();
	void Apply();

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Gate;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> Ropes;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> Capstans;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<USaltInteractableComponent>> Handles;

	/** 0 closed .. 1 open, where it is and where it goes. */
	float Amount = 1.f;
	float Target = 1.f;
	/** Progress along the eased travel (0..1) for the current move. */
	float Travel = 1.f;
	float From = 1.f;
};

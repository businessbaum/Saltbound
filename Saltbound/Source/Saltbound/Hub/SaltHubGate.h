// Saltbound - a gate of the hub ship (D62, gates and gate_marks in HubShip_Data.json): it blocks the way into a locked zone
// and E says why; once the zone is unlocked it lets the player through.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubGate.generated.h"

class UBoxComponent;
class USaltInteractableComponent;
class ASaltPlayerCharacter;

/** Placed by build_hub_level.py at every gate mark (the art is SM_Hub_Gates); the actor's frame is the blocker's. */
UCLASS()
class SALTBOUND_API ASaltHubGate : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubGate();

	/** The locked zone behind it (L1..L17). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName ZoneId;
	/** G1 barred door, G2 rope and board, G3 chained ladder or hatch, G4 shutters, G6 iron grille. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName GateType;
	/** What E does, and the line it shows (gates.prompt / gates.line, or the gate mark's own line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FText Prompt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FText Line;
	/** The blocker's half size (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector BlockExtent = FVector(20.f, 60.f, 110.f);
	/** Where a player meets it (feet, the actor's frame, cm; on the open floor beside it), and whether walking into it from
	 *  there is the test of it (not for a hatch in the deck: it closes a way down). The hub tests use both. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector Approach = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") bool bTryWalk = true;

	bool IsLocked() const;
	/** Follows the save's unlocked zones (OnHubChanged). */
	void Refresh();
	UBoxComponent* GetBlocker() const { return Blocker; }
	USaltInteractableComponent* GetUse() const { return Use; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION() void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UBoxComponent> Blocker;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltInteractableComponent> Use;
	FDelegateHandle ChangedHandle;
};

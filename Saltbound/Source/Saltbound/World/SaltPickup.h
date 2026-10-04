// Saltbound - something lying about that the player can pick up into the satchel (D50).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltPickup.generated.h"

class ASaltPlayerCharacter;
class UStaticMeshComponent;
class USaltInteractableComponent;

/**
 * Shows the item's mesh (from SaltItems) and offers "Pick up <item>". Taken, it disappears; the save remembers it by
 * PickupId (unique in the level), so it stays gone after loading.
 */
UCLASS()
class SALTBOUND_API ASaltPickup : public AActor
{
	GENERATED_BODY()

public:
	ASaltPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup") FName ItemId = TEXT("Driftwood");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup") int32 Count = 1;
	/** Unique in the level (the save records taken pickups by it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup") FName PickupId;

	void SetCollected(bool bInCollected);
	bool IsCollected() const { return bCollected; }
	/** Takes it into the player's satchel; false when it does not fit. */
	bool Collect(ASaltPlayerCharacter* Player);
	USaltInteractableComponent* GetInteraction() const { return Interaction; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UFUNCTION() void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	void Resolve();

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltInteractableComponent> Interaction;
	bool bCollected = false;
};

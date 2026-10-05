// Saltbound - a rung ladder up a quay of the hub's dock (D62, spaces.dock.rung_ladders; the D54 ladder): out of the water onto
// the quay, or down into it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubLadder.generated.h"

class USaltInteractableComponent;
class ASaltPlayerCharacter;
struct FSaltLadderLine;

/** The actor stands at the ladder's head on the quay's edge: +X along the quay, +Y out over the water (the rungs stand out
 *  from the wall's face), Z up; the quay's deck at its origin. */
UCLASS()
class SALTBOUND_API ASaltHubLadder : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubLadder();

	/** The lowest rung under the quay's deck (cm, negative). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float BottomZ = -330.f;
	/** The rungs out from the wall's face (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float RungsOut = 16.f;
	/** Where the top lets the climber off, in from the edge (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (Units = "cm")) float ExitIn = 75.f;

	FSaltLadderLine GetLine(float CapsuleHalfHeight, float CapsuleRadius) const;
	USaltInteractableComponent* GetUse() const { return Use; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltInteractableComponent> Use;
};

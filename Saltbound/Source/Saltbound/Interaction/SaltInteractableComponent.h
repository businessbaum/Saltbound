// Saltbound - focus volume that makes part of an actor usable by the player.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SaltInteractableComponent.generated.h"

class ASaltPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSaltInteractSignature, class USaltInteractableComponent*, Interactable, ASaltPlayerCharacter*, Player);

/**
 * Box-shaped focus volume detected by the player's interaction trace (Interaction channel only; it never
 * blocks movement or visibility). Several can live on one actor, e.g. helm, hatch and bunk on the boat.
 * Owners react via OnInteract or by subclassing (see USaltStationComponent).
 */
UCLASS(ClassGroup = (Saltbound), meta = (BlueprintSpawnableComponent))
class SALTBOUND_API USaltInteractableComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USaltInteractableComponent();

	/** Short verb phrase shown next to the key hint, e.g. "Take the helm". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText PromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FSaltInteractSignature OnInteract;

	virtual bool CanInteract(const ASaltPlayerCharacter* Player) const;
	virtual FText GetPromptText(const ASaltPlayerCharacter* Player) const;
	virtual void Interact(ASaltPlayerCharacter* Player);
};

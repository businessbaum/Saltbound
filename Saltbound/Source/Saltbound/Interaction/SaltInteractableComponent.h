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

	/** Seconds to hold the key before the action runs (0 = a short press). Holds are the owner's call (D55): only
	 *  raising the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0", Units = "s"))
	float HoldTime = 0.f;

	bool IsHold() const { return HoldTime > 0.f; }

	/** Where the prompt is pinned when it shows on the object (D60): the volume's middle moved by this (cm, local). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector PromptOffset = FVector::ZeroVector;

	FVector GetPromptLocation() const { return GetComponentTransform().TransformPosition(PromptOffset); }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FSaltInteractSignature OnInteract;

	/** D80: the things in hand it is used with (left mouse button, not E), e.g. Shovel. Empty: E uses it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TArray<FName> UseWith;

	/** D80: whether it is used now with what the player holds (the left mouse button) rather than with E. */
	virtual bool UsesHeldThing(const ASaltPlayerCharacter* Player) const;
	virtual bool CanInteract(const ASaltPlayerCharacter* Player) const;
	virtual FText GetPromptText(const ASaltPlayerCharacter* Player) const;
	virtual void Interact(ASaltPlayerCharacter* Player);
};

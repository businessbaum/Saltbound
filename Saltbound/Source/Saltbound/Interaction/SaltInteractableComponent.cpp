// Saltbound - focus volume that makes part of an actor usable by the player.

#include "Interaction/SaltInteractableComponent.h"

#include "Core/SaltTypes.h"

USaltInteractableComponent::USaltInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InitBoxExtent(FVector(20.f));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(SALT_TRACE_INTERACTION, ECR_Block);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	bHiddenInGame = true;
	ShapeColor = FColor(255, 196, 64);
	PromptText = NSLOCTEXT("Saltbound", "UseDefault", "Use");
}

bool USaltInteractableComponent::CanInteract(const ASaltPlayerCharacter* Player) const
{
	return bInteractionEnabled && IsCollisionEnabled();
}

FText USaltInteractableComponent::GetPromptText(const ASaltPlayerCharacter* Player) const
{
	return PromptText;
}

void USaltInteractableComponent::Interact(ASaltPlayerCharacter* Player)
{
	OnInteract.Broadcast(this, Player);
}

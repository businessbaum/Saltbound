// Saltbound - an interaction point of the hub ship (D62; see SaltHubPoint.h).

#include "Hub/SaltHubPoint.h"

#include "Interaction/SaltInteractableComponent.h"
#include "Saltbound.h"

ASaltHubPoint::ASaltHubPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	Use = CreateDefaultSubobject<USaltInteractableComponent>(TEXT("Use"));
	SetRootComponent(Use);
}

void ASaltHubPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Use->SetBoxExtent(UseExtent);
	Use->PromptText = Prompt;
}

void ASaltHubPoint::BeginPlay()
{
	Super::BeginPlay();
	Use->SetBoxExtent(UseExtent);
	Use->PromptText = Prompt;
	Use->OnInteract.AddDynamic(this, &ASaltHubPoint::HandleInteract);
}

void ASaltHubPoint::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	++Uses;
	UE_LOG(LogSaltbound, Display, TEXT("SB_HUB_POINT %s.%s \"%s\" (nothing here yet)"), *Space.ToString(), *PointId.ToString(), *Prompt.ToString());
}

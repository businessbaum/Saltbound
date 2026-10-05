// Saltbound - a gate of the hub ship (D62; see SaltHubGate.h).

#include "Hub/SaltHubGate.h"

#include "Components/BoxComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

ASaltHubGate::ASaltHubGate()
{
	PrimaryActorTick.bCanEverTick = false;
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	SetRootComponent(Blocker);
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->SetCanEverAffectNavigation(false);
	Use = CreateDefaultSubobject<USaltInteractableComponent>(TEXT("Use"));
	Use->SetupAttachment(Blocker);
}

void ASaltHubGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Blocker->SetBoxExtent(BlockExtent);
	// The focus volume a little bigger than the blocker, so it is found from the open side.
	Use->SetBoxExtent(BlockExtent + FVector(30.f, 30.f, 0.f));
	Use->PromptText = Prompt;
}

void ASaltHubGate::BeginPlay()
{
	Super::BeginPlay();
	Blocker->SetBoxExtent(BlockExtent);
	Use->SetBoxExtent(BlockExtent + FVector(30.f, 30.f, 0.f));
	Use->PromptText = Prompt;
	Use->OnInteract.AddDynamic(this, &ASaltHubGate::HandleInteract);
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		ChangedHandle = Saves->OnHubChanged.AddUObject(this, &ASaltHubGate::Refresh);
	}
	Refresh();
}

void ASaltHubGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		Saves->OnHubChanged.Remove(ChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool ASaltHubGate::IsLocked() const
{
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	return !(Saves && Saves->IsUnlocked(ZoneId));
}

void ASaltHubGate::Refresh()
{
	const bool bLocked = IsLocked();
	Blocker->SetCollisionEnabled(bLocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Use->bInteractionEnabled = bLocked;
}

void ASaltHubGate::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	UE_LOG(LogSaltbound, Display, TEXT("SB_HUB_GATE %s %s: %s"), *GateType.ToString(), *ZoneId.ToString(), *Line.ToString());
	if (ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr)
	{
		PC->ShowNotice(Line);
	}
}

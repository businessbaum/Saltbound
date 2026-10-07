// Saltbound - a rung ladder up a quay of the hub's dock (D62; see SaltHubLadder.h).

#include "Hub/SaltHubLadder.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Player/SaltPlayerCharacter.h"

ASaltHubLadder::ASaltHubLadder()
{
	// D79 (owner: "The only way to climb ladders is to press the interaction button"): D59's taking hold by swimming into
	// the ladder is gone; only E takes hold, so nothing ticks.
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Use = CreateDefaultSubobject<USaltInteractableComponent>(TEXT("Use"));
	Use->SetupAttachment(Root);
	Use->SetRelativeLocation(FVector(0.0, 40.0, -120.0));
	Use->SetBoxExtent(FVector(40.f, 50.f, 170.f));
	Use->PromptText = NSLOCTEXT("Saltbound", "HubLadderClimb", "Climb the ladder");
}

void ASaltHubLadder::BeginPlay()
{
	Super::BeginPlay();
	Use->OnInteract.AddDynamic(this, &ASaltHubLadder::HandleInteract);
}

FSaltLadderLine ASaltHubLadder::GetLine(float CapsuleHalfHeight, float CapsuleRadius) const
{
	// As the boat's (ASaltBoat::GetLadderLine): the capsule centre just out from the rungs, facing the wall, from the feet on
	// the lowest rung up to the feet at the quay's deck; at the top, in over the edge onto the quay.
	const float OnY = RungsOut + 4.f + CapsuleRadius + 4.f;
	FSaltLadderLine Line;
	Line.Base = Root;
	Line.Bottom = FVector(0.f, OnY, BottomZ + 8.f + CapsuleHalfHeight);
	Line.Top = FVector(0.f, OnY, CapsuleHalfHeight + 10.f);
	Line.TopExit = FVector(0.f, -ExitIn, CapsuleHalfHeight + 3.f);
	Line.FacingYaw = -90.f;
	Line.RungStep = 30.f;
	return Line;
}

void ASaltHubLadder::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	if (!Player || Player->IsOnLadder())
	{
		return;
	}
	Player->MountLadder(GetLine(Player->GetCapsuleHalfHeightForClimb(), Player->GetCapsuleComponent()->GetScaledCapsuleRadius()), !Player->IsSwimming());
}

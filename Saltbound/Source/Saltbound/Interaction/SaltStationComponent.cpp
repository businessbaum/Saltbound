// Saltbound - control station (helm, winch, ...) that the player can step into and operate.

#include "Interaction/SaltStationComponent.h"

#include "Player/SaltPlayerCharacter.h"

USaltStationComponent::USaltStationComponent()
{
	PromptText = NSLOCTEXT("Saltbound", "StationUse", "Take control");
	LeavePromptText = NSLOCTEXT("Saltbound", "StationLeave", "Step away");
	ShapeColor = FColor(64, 196, 255);
}

bool USaltStationComponent::CanInteract(const ASaltPlayerCharacter* Player) const
{
	return Super::CanInteract(Player) && Occupant == nullptr;
}

void USaltStationComponent::Interact(ASaltPlayerCharacter* Player)
{
	if (Player && CanInteract(Player))
	{
		Player->EnterStation(this);
	}
	Super::Interact(Player);
}

void USaltStationComponent::NotifyEntered(ASaltPlayerCharacter* Player)
{
	Occupant = Player;
	ControlInput = FVector2D::ZeroVector;
	OnEntered.Broadcast(this, Player);
}

void USaltStationComponent::NotifyLeft(ASaltPlayerCharacter* Player)
{
	if (Occupant == Player)
	{
		Occupant = nullptr;
		ControlInput = FVector2D::ZeroVector;
		OnLeft.Broadcast(this, Player);
	}
}

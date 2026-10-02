// Saltbound - game mode.

#include "Core/SaltGameMode.h"

#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"

ASaltGameMode::ASaltGameMode()
{
	DefaultPawnClass = ASaltPlayerCharacter::StaticClass();
	PlayerControllerClass = ASaltPlayerController::StaticClass();
}

// Saltbound - player controller: input contexts and HUD.

#include "Player/SaltPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Player/SaltInputConfig.h"
#include "UI/SaltHUDWidget.h"

ASaltPlayerController::ASaltPlayerController()
{
	bShowMouseCursor = false;
}

USaltInputConfig* ASaltPlayerController::GetInputConfig()
{
	if (!InputConfig)
	{
		InputConfig = NewObject<USaltInputConfig>(this, TEXT("SaltInputConfig"));
		InputConfig->Build();
	}
	return InputConfig;
}

void ASaltPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(GetInputConfig()->DefaultContext, 0);
	}

	HUDWidget = CreateWidget<USaltHUDWidget>(this, USaltHUDWidget::StaticClass(), TEXT("SaltHUD"));
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
	}
	SetInputMode(FInputModeGameOnly());
}

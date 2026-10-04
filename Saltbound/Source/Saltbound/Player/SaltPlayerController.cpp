// Saltbound - player controller: input contexts and HUD.

#include "Player/SaltPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerCharacter.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltMenuWidget.h"

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

void ASaltPlayerController::SetHUDVisible(bool bVisible)
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool ASaltPlayerController::IsMenuOpen() const
{
	return MenuWidget && MenuWidget->IsInViewport();
}

void ASaltPlayerController::SetMenuOpen(bool bOpen)
{
	if (bOpen == IsMenuOpen() || !IsLocalController())
	{
		return;
	}
	// Held keys would stay "down" across the switch to the menu and back.
	FlushPressedKeys();
	if (ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn()))
	{
		SaltCharacter->ResetHeldInput();
	}
	if (bOpen)
	{
		if (!MenuWidget)
		{
			MenuWidget = CreateWidget<USaltMenuWidget>(this, USaltMenuWidget::StaticClass(), TEXT("SaltMenu"));
		}
		MenuWidget->AddToViewport(10);
		SetPause(true);
		bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		MenuWidget->RemoveFromParent();
		SetPause(false);
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
}

void ASaltPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(GetInputConfig()->Menu, ETriggerEvent::Started, this, &ASaltPlayerController::ToggleMenu);
	}
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

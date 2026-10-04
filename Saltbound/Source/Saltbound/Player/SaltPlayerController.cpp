// Saltbound - player controller: input contexts and HUD.

#include "Player/SaltPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/SaltSaveSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerCharacter.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltMenuWidget.h"
#include "UI/SaltSatchelWidget.h"
#include "UI/SaltStoreWidget.h"
#include "Player/SaltInventoryComponent.h"

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

void ASaltPlayerController::ShowNotice(const FText& Text)
{
	if (HUDWidget)
	{
		HUDWidget->ShowNotice(Text);
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

bool ASaltPlayerController::IsStoreOpen() const
{
	return StoreWidget && StoreWidget->IsInViewport();
}

void ASaltPlayerController::SetUIInput(bool bOn, UUserWidget* Focus)
{
	FlushPressedKeys();
	if (ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn()))
	{
		SaltCharacter->ResetHeldInput();
	}
	bShowMouseCursor = bOn;
	if (bOn)
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Focus->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ASaltPlayerController::OpenStore(USaltInventoryComponent* Store)
{
	ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn());
	if (!IsLocalController() || !Store || !SaltCharacter || IsMenuOpen() || IsSatchelOpen() || IsStoreOpen())
	{
		return;
	}
	StoreWidget = CreateWidget<USaltStoreWidget>(this, USaltStoreWidget::StaticClass());
	StoreWidget->SetInventories(SaltCharacter->GetInventory(), Store);
	StoreWidget->AddToViewport(8);
	SetUIInput(true, StoreWidget);
}

void ASaltPlayerController::CloseStore()
{
	if (IsStoreOpen())
	{
		StoreWidget->RemoveFromParent();
		StoreWidget = nullptr;
		SetUIInput(false, nullptr);
	}
}

bool ASaltPlayerController::IsSatchelOpen() const
{
	return SatchelWidget && SatchelWidget->IsInViewport();
}

void ASaltPlayerController::SetSatchelOpen(bool bOpen, int32 Page)
{
	if (!IsLocalController() || (bOpen && (IsMenuOpen() || IsStoreOpen())))
	{
		return;
	}
	if (bOpen && IsSatchelOpen())
	{
		SatchelWidget->SetPage(Page);
		return;
	}
	if (!bOpen && !IsSatchelOpen())
	{
		return;
	}
	FlushPressedKeys();
	if (ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn()))
	{
		SaltCharacter->ResetHeldInput();
	}
	if (bOpen)
	{
		if (!SatchelWidget)
		{
			SatchelWidget = CreateWidget<USaltSatchelWidget>(this, USaltSatchelWidget::StaticClass(), TEXT("SaltSatchel"));
		}
		SatchelWidget->AddToViewport(8);
		SatchelWidget->SetPage(Page);
		bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(SatchelWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		SatchelWidget->RemoveFromParent();
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
		Input->BindAction(GetInputConfig()->Inventory, ETriggerEvent::Started, this, &ASaltPlayerController::OpenInventory);
		Input->BindAction(GetInputConfig()->Map, ETriggerEvent::Started, this, &ASaltPlayerController::OpenMap);
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

	// The game continues from the last save (D46). Play-in-editor and tool runs (-SaltNewGame: captures, benchmarks)
	// start from the map as built. Next tick: the boat has set itself up by then.
	if (!GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("SaltNewGame")))
	{
		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
			{
				Saves->LoadGame(GetWorld());
			}
		});
	}
}

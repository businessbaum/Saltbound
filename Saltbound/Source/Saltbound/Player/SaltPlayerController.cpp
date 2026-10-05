// Saltbound - player controller: input contexts and HUD.

#include "Player/SaltPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Boat/SaltBoat.h"
#include "Core/SaltSaveSubsystem.h"
#include "Core/SaltTools.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerCharacter.h"
#include "UI/SaltChartWidget.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltMenuWidget.h"
#include "UI/SaltSatchelWidget.h"
#include "UI/SaltStoreWidget.h"
#include "UI/SaltToolWheelWidget.h"
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

FText ASaltPlayerController::GetNoticeText() const
{
	return HUDWidget ? HUDWidget->GetNoticeText() : FText::GetEmpty();
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

void ASaltPlayerController::SetUIInput(bool bOn, UUserWidget* Focus, bool bCursor)
{
	FlushPressedKeys();
	if (ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn()))
	{
		SaltCharacter->ResetHeldInput();
	}
	bShowMouseCursor = bOn && bCursor;
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

bool ASaltPlayerController::IsChartOpen() const
{
	return ChartWidget && ChartWidget->IsInViewport();
}

void ASaltPlayerController::OpenChart(ASaltBoat* Boat)
{
	ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn());
	if (!IsLocalController() || !Boat || !SaltCharacter || IsMenuOpen() || IsSatchelOpen() || IsStoreOpen() || IsChartOpen() || bToolWheel)
	{
		return;
	}
	// D58 (owner: "smooth but fast camera to the mast, not an instant pop up"): the eyes glide to the chart on the mast and
	// stay with it as the boat moves; the names, north and the scale are inked over the sheet once there.
	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	const float Aspect = SizeY > 0 ? (float)SizeX / (float)SizeY : 16.f / 9.f;
	SaltCharacter->FocusView(Boat->Hull, Boat->GetChartViewLocal(SaltCharacter->FieldOfView, Aspect), ChartGlideIn);
	ChartWidget = CreateWidget<USaltChartWidget>(this, USaltChartWidget::StaticClass());
	ChartWidget->SetBoat(Boat);
	ChartWidget->AddToViewport(8);
	SetUIInput(true, ChartWidget, false);
}

void ASaltPlayerController::CloseChart()
{
	if (IsChartOpen())
	{
		ChartWidget->RemoveFromParent();
		ChartWidget = nullptr;
		SetUIInput(false, nullptr);
		if (ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn()))
		{
			SaltCharacter->ReleaseView(ChartGlideOut);
		}
	}
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

void ASaltPlayerController::SetSatchelOpen(bool bOpen)
{
	if (!IsLocalController() || (bOpen && (IsMenuOpen() || IsStoreOpen())))
	{
		return;
	}
	if (bOpen == IsSatchelOpen())
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
		Input->BindAction(GetInputConfig()->Menu, ETriggerEvent::Started, this, &ASaltPlayerController::HandleMenuKey);
		Input->BindAction(GetInputConfig()->Inventory, ETriggerEvent::Started, this, &ASaltPlayerController::OpenInventory);
		Input->BindAction(GetInputConfig()->ToolWheel, ETriggerEvent::Started, this, &ASaltPlayerController::OpenToolWheel);
		Input->BindAction(GetInputConfig()->ToolWheel, ETriggerEvent::Completed, this, &ASaltPlayerController::TakeFromToolWheel);
		for (int32 I = 0; I < GetInputConfig()->Tools.Num(); ++I)
		{
			Input->BindAction(GetInputConfig()->Tools[I], ETriggerEvent::Started, this, &ASaltPlayerController::SelectTool, I);
		}
	}
}

void ASaltPlayerController::HandleMenuKey()
{
	if (bToolWheel)
	{
		CloseToolWheel(false);
		return;
	}
	ToggleMenu();
}

void ASaltPlayerController::OpenToolWheel()
{
	ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn());
	USaltToolWheelWidget* Wheel = HUDWidget ? HUDWidget->GetToolWheel() : nullptr;
	if (bToolWheel || !IsLocalController() || !SaltCharacter || !Wheel || IsMenuOpen() || IsSatchelOpen() || IsStoreOpen())
	{
		return;
	}
	bToolWheel = true;
	WheelPointer = FVector2D::ZeroVector;
	TArray<bool> Owned;
	for (const FSaltToolDef& T : SaltTools::All())
	{
		Owned.Add(T.bAlwaysOwned || SaltCharacter->IsToolOwned(T.Id));
	}
	// It opens on what is in hand (empty hands when nothing is).
	const FName Held = SaltCharacter->GetHeldTool();
	Wheel->Open(FMath::Max(SaltTools::IndexOf(Held.IsNone() ? FName(TEXT("EmptyHands")) : Held), 0), Owned);
}

void ASaltPlayerController::CloseToolWheel(bool bTake)
{
	if (!bToolWheel)
	{
		return;
	}
	bToolWheel = false;
	USaltToolWheelWidget* Wheel = HUDWidget ? HUDWidget->GetToolWheel() : nullptr;
	const int32 Chosen = Wheel ? Wheel->GetSelected() : INDEX_NONE;
	if (Wheel)
	{
		Wheel->Close();
	}
	if (bTake && Chosen != INDEX_NONE)
	{
		UseTool(Chosen);
	}
}

void ASaltPlayerController::AimToolWheel(FVector2D Input, bool bStick)
{
	USaltToolWheelWidget* Wheel = HUDWidget ? HUDWidget->GetToolWheel() : nullptr;
	if (!bToolWheel || !Wheel)
	{
		return;
	}
	// Screen space, y down.
	FVector2D Dir;
	if (bStick)
	{
		Dir = FVector2D(Input.X, -Input.Y);
		if (Dir.Size() < 0.35f)
		{
			return;
		}
	}
	else
	{
		WheelPointer = (WheelPointer + FVector2D(Input.X, -Input.Y)).GetClampedToMaxSize(150.f);
		if (WheelPointer.Size() < 40.f)
		{
			return;
		}
		Dir = WheelPointer;
	}
	// The medallions sit evenly round the ring from the top: the one in the pointer's direction.
	const int32 N = FMath::Max(SaltTools::All().Num(), 1);
	const float Deg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	Wheel->SetSelected(((FMath::RoundToInt((Deg + 90.f) / (360.f / N)) % N) + N) % N);
}

void ASaltPlayerController::SelectTool(int32 Index)
{
	USaltToolWheelWidget* Wheel = HUDWidget ? HUDWidget->GetToolWheel() : nullptr;
	if (bToolWheel && Wheel)
	{
		Wheel->SetSelected(Index);
		CloseToolWheel(true);
		return;
	}
	if (!IsMenuOpen() && !IsSatchelOpen() && !IsStoreOpen())
	{
		UseTool(Index);
	}
}

bool ASaltPlayerController::UseTool(int32 Index)
{
	ASaltPlayerCharacter* SaltCharacter = Cast<ASaltPlayerCharacter>(GetPawn());
	if (!SaltCharacter || !SaltTools::All().IsValidIndex(Index))
	{
		return false;
	}
	const FSaltToolDef& Tool = SaltTools::All()[Index];
	if (!Tool.bAlwaysOwned && !SaltCharacter->IsToolOwned(Tool.Id))
	{
		return false;
	}
	switch (Tool.Use)
	{
	case ESaltToolUse::Held:
		SaltCharacter->SetHeldTool(Tool.Id);
		break;
	case ESaltToolUse::Satchel:
		SetSatchelOpen(true);
		break;
	case ESaltToolUse::PutAway:
		SaltCharacter->SetHeldTool(NAME_None);
		break;
	}
	return true;
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

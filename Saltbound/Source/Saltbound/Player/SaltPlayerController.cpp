// Saltbound - player controller: input contexts and HUD.

#include "Player/SaltPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Boat/SaltBoat.h"
#include "Core/SaltGameUserSettings.h"
#include "Core/SaltPlaces.h"
#include "Core/SaltSaveSubsystem.h"
#include "Core/SaltTools.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerCharacter.h"
#include "UI/SaltChartWidget.h"
#include "UI/SaltConfirmWidget.h"
#include "UI/SaltFirstStartWidget.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltKeysWidget.h"
#include "UI/SaltLoadingWidget.h"
#include "UI/SaltMenuWidget.h"
#include "UI/SaltSettingsWidget.h"
#include "UI/SaltStartMenuWidget.h"
#include "UI/SaltVoyagesWidget.h"
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
	bHUDWanted = bVisible;
	UpdateScreens();
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
	return Screens.Num() > 0 || IsLoading();
}

void ASaltPlayerController::SetMenuOpen(bool bOpen)
{
	if (bOpen == IsMenuOpen() || !IsLocalController())
	{
		return;
	}
	if (bOpen)
	{
		if (!MenuWidget)
		{
			MenuWidget = CreateWidget<USaltMenuWidget>(this, USaltMenuWidget::StaticClass(), TEXT("SaltMenu"));
		}
		PushScreen(MenuWidget);
	}
	else
	{
		CloseScreens();
	}
}

// --- Menu screens (D66) ------------------------------------------------------------------------------------------------

void ASaltPlayerController::PushScreen(USaltScreen* Screen)
{
	if (!Screen || !IsLocalController())
	{
		return;
	}
	if (Screens.Contains(Screen))
	{
		Screens.Remove(Screen);
		Screen->RemoveFromParent();
	}
	Screens.Add(Screen);
	Screen->AddToViewport(10 + Screens.Num());
	UpdateScreens();
}

void ASaltPlayerController::PopScreen(USaltScreen* Screen)
{
	if (!Screen)
	{
		Screen = GetTopScreen();
	}
	if (!Screen || !Screens.Contains(Screen))
	{
		return;
	}
	const bool bWasTop = Screen == GetTopScreen();
	Screens.Remove(Screen);
	Screen->RemoveFromParent();
	UpdateScreens();
	if (bWasTop && GetTopScreen())
	{
		GetTopScreen()->OnRevealed();
	}
}

void ASaltPlayerController::CloseScreens()
{
	for (USaltScreen* S : Screens)
	{
		if (S)
		{
			S->RemoveFromParent();
		}
	}
	Screens.Reset();
	UpdateScreens();
}

bool ASaltPlayerController::IsTitleOpen() const
{
	return FindScreen<USaltStartMenuWidget>() != nullptr;
}

bool ASaltPlayerController::IsLoading() const
{
	return LoadingWidget && LoadingWidget->IsInViewport() && !LoadingWidget->IsDone();
}

void ASaltPlayerController::UpdateScreens()
{
	if (!IsLocalController())
	{
		return;
	}
	// The top screen shows and takes the keys; one under it shows too while the top is a question over it.
	bool bShow = true;
	bool bPause = false;
	for (int32 I = Screens.Num() - 1; I >= 0; --I)
	{
		USaltScreen* S = Screens[I];
		S->SetVisibility(!bShow ? ESlateVisibility::Collapsed : I == Screens.Num() - 1 ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
		bShow = bShow && S->bOverlay;
		bPause = bPause || S->bPausesGame;
	}
	const bool bCovered = LoadingWidget && LoadingWidget->IsInViewport() && LoadingWidget->IsCovering();
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bHUDWanted && Screens.Num() == 0 && !bCovered ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bPause != bPausedByScreens)
	{
		bPausedByScreens = bPause;
		SetPause(bPause);
	}
	UUserWidget* KeysTo = GetTopScreen() ? (UUserWidget*)GetTopScreen() : bCovered ? (UUserWidget*)LoadingWidget : nullptr;
	if (KeysTo != ScreenInput.Get())
	{
		const bool bHadScreens = ScreenInput.IsValid();
		ScreenInput = KeysTo;
		if (USaltScreen* Top = GetTopScreen())
		{
			SetUIInput(true, Top, Top->bWantsCursor);
		}
		else if (KeysTo)
		{
			SetUIInput(true, KeysTo, false);
		}
		else if (bHadScreens && !IsSatchelOpen() && !IsStoreOpen() && !IsChartOpen())
		{
			SetUIInput(false, nullptr);
		}
	}
}

void ASaltPlayerController::ShowTitle()
{
	CloseScreens();
	PushScreen(CreateWidget<USaltStartMenuWidget>(this, USaltStartMenuWidget::StaticClass()));
}

void ASaltPlayerController::OpenBrightness(bool bFromSettings)
{
	if (!bFromSettings)
	{
		CloseScreens();
	}
	USaltFirstStartWidget* W = CreateWidget<USaltFirstStartWidget>(this, USaltFirstStartWidget::StaticClass());
	W->bFromSettings = bFromSettings;
	PushScreen(W);
}

void ASaltPlayerController::OpenSettings()
{
	PushScreen(CreateWidget<USaltSettingsWidget>(this, USaltSettingsWidget::StaticClass()));
}

USaltSettingsWidget* ASaltPlayerController::OpenSettingsAt(uint8 Setting)
{
	USaltSettingsWidget* W = CreateWidget<USaltSettingsWidget>(this, USaltSettingsWidget::StaticClass());
	PushScreen(W);
	W->ShowSetting((ESaltSetting)Setting);
	return W;
}

void ASaltPlayerController::OpenVoyages()
{
	USaltVoyagesWidget* W = CreateWidget<USaltVoyagesWidget>(this, USaltVoyagesWidget::StaticClass());
	W->bInPlay = FindScreen<USaltMenuWidget>() != nullptr;
	PushScreen(W);
}

void ASaltPlayerController::OpenKeys()
{
	PushScreen(CreateWidget<USaltKeysWidget>(this, USaltKeysWidget::StaticClass()));
}

void ASaltPlayerController::Ask(const FText& Title, const FText& Body, const FText& Yes, const FText& No, TFunction<void()> OnYes)
{
	USaltConfirmWidget* W = CreateWidget<USaltConfirmWidget>(this, USaltConfirmWidget::StaticClass());
	W->Setup(Title, Body, Yes, No, MoveTemp(OnYes));
	PushScreen(W);
}

void ASaltPlayerController::ContinueVoyage()
{
	CloseScreens();
}

void ASaltPlayerController::NewVoyage()
{
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (!Saves)
	{
		return;
	}
	for (int32 I = 0; I < USaltSaveSubsystem::VoyageCount; ++I)
	{
		if (!Saves->GetVoyageInfo(I).bExists)
		{
			LoadVoyage(I, true, nullptr);
			return;
		}
	}
	// Every page is taken: the voyages screen, where one can be scrapped.
	OpenVoyages();
}

void ASaltPlayerController::SailVoyage(int32 Index)
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (!Saves || Index < 0 || Index >= USaltSaveSubsystem::VoyageCount)
	{
		return;
	}
	const bool bNew = !Saves->GetVoyageInfo(Index).bExists;
	const bool bInPlay = FindScreen<USaltMenuWidget>() != nullptr;
	if (!bInPlay && !bNew && IsTitleOpen() && Saves->GetActiveVoyage() == Index)
	{
		// From the title the world behind already is this voyage.
		ContinueVoyage();
		return;
	}
	if (bInPlay && Saves->GetUnsavedTime() > 10.0)
	{
		TWeakObjectPtr<ASaltPlayerController> Self(this);
		Ask(bNew ? NSLOCTEXT("SaltVoyages", "AskNew", "Start a new voyage?") : NSLOCTEXT("SaltVoyages", "AskLeave", "Leave this voyage?"),
			Saves->HasSave()
				? FText::Format(NSLOCTEXT("SaltVoyages", "AskLeaveBody", "You last saved {0}. What happened since then stays at sea."), SaltMenu::Ago(Saves->GetSaveTime()))
				: NSLOCTEXT("SaltVoyages", "AskLeaveBodyNone", "This voyage has not been saved yet. What happened stays at sea."),
			NSLOCTEXT("SaltVoyages", "AskLeaveYes", "set sail"), NSLOCTEXT("SaltVoyages", "AskLeaveNo", "stay aboard"),
			[Self, Index, bNew]()
			{
				if (ASaltPlayerController* PC = Self.Get())
				{
					PC->LoadVoyage(Index, bNew, nullptr);
				}
			});
		return;
	}
	LoadVoyage(Index, bNew, nullptr);
}

void ASaltPlayerController::LoadVoyage(int32 Index, bool bNew, TFunction<void()> Then)
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (!Saves)
	{
		return;
	}
	FText Place;
	FText Line;
	FString Picture;
	const FSaltPlace* Home = SaltPlaces::Find(TEXT("SaltboundCove"));
	if (bNew)
	{
		Place = Home ? Home->Name : FText::FromString(TEXT("Saltbound Cove"));
		Line = NSLOCTEXT("SaltVoyages", "NewLine", "where every voyage begins");
	}
	else
	{
		const FSaltVoyageInfo Info = Saves->GetVoyageInfo(Index);
		Place = Info.bNearPlace ? Info.Place : NSLOCTEXT("SaltVoyages", "OpenSeaName", "The open sea");
		const FSaltPlace* Near = nullptr;
		for (const FSaltPlace& P : SaltPlaces::All())
		{
			if (Info.bNearPlace && P.Name.EqualTo(Info.Place))
			{
				Near = &P;
			}
		}
		// "1.7 km north-east of Saltbound Cove · at anchor" (the design's line), or where she lies.
		Line = Near && Near != Home ? FText::Format(NSLOCTEXT("SaltVoyages", "LoadLine", "{0} · {1}"), SaltPlaces::FromHome(*Near), Info.Boat)
			: FText::Format(NSLOCTEXT("SaltVoyages", "LoadLineHome", "where you left her · {0}"), Info.Boat);
		Picture = Info.Picture;
	}
	TWeakObjectPtr<ASaltPlayerController> Self(this);
	ShowLoading(Place, Line, Picture,
		[Self, Index, bNew]()
		{
			ASaltPlayerController* PC = Self.Get();
			USaltSaveSubsystem* S = PC ? USaltSaveSubsystem::Get(PC) : nullptr;
			if (!S)
			{
				return;
			}
			PC->CloseScreens();
			if (bNew)
			{
				S->StartNewVoyage(PC->GetWorld(), Index);
			}
			else
			{
				S->SetActiveVoyage(Index);
				S->bQuiet = true;
				S->LoadGame(PC->GetWorld());
				S->bQuiet = false;
			}
		},
		[Self, Then]()
		{
			if (Then)
			{
				Then();
			}
			else if (ASaltPlayerController* PC = Self.Get())
			{
				PC->UpdateScreens();
			}
		});
}

void ASaltPlayerController::ReturnToTitle()
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (!Saves)
	{
		return;
	}
	const int32 Active = Saves->GetActiveVoyage();
	const FSaltVoyageInfo Info = Active != INDEX_NONE ? Saves->GetVoyageInfo(Active) : FSaltVoyageInfo();
	TWeakObjectPtr<ASaltPlayerController> Self(this);
	ShowLoading(Info.bExists && Info.bNearPlace ? Info.Place : FText::FromString(TEXT("Saltbound")),
		Info.bExists ? FText::Format(NSLOCTEXT("SaltVoyages", "LoadLineHome", "where you left her · {0}"), Info.Boat) : FText::GetEmpty(), Info.Picture,
		[Self]()
		{
			ASaltPlayerController* PC = Self.Get();
			USaltSaveSubsystem* S = PC ? USaltSaveSubsystem::Get(PC) : nullptr;
			if (!S)
			{
				return;
			}
			PC->CloseScreens();
			// The voyage as last saved goes behind the title (or the world as built when it was never saved).
			if (S->HasSave())
			{
				S->bQuiet = true;
				S->LoadGame(PC->GetWorld());
				S->bQuiet = false;
			}
			else
			{
				S->RestoreNewVoyageSnapshot(PC->GetWorld());
			}
		},
		[Self]()
		{
			if (ASaltPlayerController* PC = Self.Get())
			{
				PC->ShowTitle();
			}
		});
}

void ASaltPlayerController::ShowLoading(const FText& Place, const FText& Line, const FString& Picture, TFunction<void()> Work, TFunction<void()> Then)
{
	if (!IsLocalController())
	{
		return;
	}
	if (LoadingWidget)
	{
		LoadingWidget->RemoveFromParent();
	}
	LoadingWidget = CreateWidget<USaltLoadingWidget>(this, USaltLoadingWidget::StaticClass());
	LoadingWidget->Begin(Place, Line, Picture, MoveTemp(Work), MoveTemp(Then));
	LoadingWidget->AddToViewport(100);
	UpdateScreens();
}

void ASaltPlayerController::RebuildInputMappings()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	USaltInputConfig* Config = GetInputConfig();
	if (Subsystem && Config->DefaultContext)
	{
		Subsystem->RemoveMappingContext(Config->DefaultContext);
	}
	Config->BuildMappings();
	if (Subsystem)
	{
		Subsystem->AddMappingContext(Config->DefaultContext, 0);
	}
}

void ASaltPlayerController::TickPlayClock()
{
	// The timer stops while the game is paused; the title and the loading screen do not count either.
	if (Screens.Num() == 0 && !IsLoading())
	{
		if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
		{
			Saves->PlayTime += 1.0;
		}
	}
}

void ASaltPlayerController::StartFromBoot()
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	USaltGameUserSettings* Settings = USaltGameUserSettings::Get();
	if (!Saves)
	{
		return;
	}
	// The voyage last played goes behind the title (D46's continue, now per voyage).
	Saves->SetActiveVoyage(Settings ? Settings->LastVoyage : 0);
	if (Saves->HasSave())
	{
		Saves->bQuiet = true;
		Saves->LoadGame(GetWorld());
		Saves->bQuiet = false;
	}
	if (Settings && !Settings->bFirstStartDone)
	{
		OpenBrightness(false);
	}
	else
	{
		ShowTitle();
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
	if (IsLoading())
	{
		return;
	}
	if (USaltScreen* Top = GetTopScreen())
	{
		Top->Back();
		return;
	}
	SetMenuOpen(true);
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
	// D66: the brightness and the volume the player chose.
	if (USaltGameUserSettings* Settings = USaltGameUserSettings::Get())
	{
		Settings->ApplyExtras(this);
	}
	GetWorldTimerManager().SetTimer(PlayClock, this, &ASaltPlayerController::TickPlayClock, 1.f, true);

	// Next tick (the boat has set itself up by then): the world as built is kept for new voyages (D66). The packaged game then
	// loads the last voyage and opens the title (the first start before it once). Play-in-editor and tool runs
	// (-SaltNewGame: captures, benchmarks) start from the map as built, in play.
	const bool bBoot = !GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("SaltNewGame"));
	GetWorldTimerManager().SetTimerForNextTick([this, bBoot]()
	{
		if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
		{
			Saves->TakeNewVoyageSnapshot(GetWorld());
		}
		if (bBoot)
		{
			StartFromBoot();
		}
	});
}

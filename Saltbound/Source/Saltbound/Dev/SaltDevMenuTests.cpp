// Saltbound - menu checks (sb.Dev.RunMenuTests): the pause menu pauses, the settings apply, keys rebind, the brightness and the
// title; sb.Dev.Menu opens any menu screen for captures (D66).

#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Core/SaltGameUserSettings.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/IConsoleManager.h"
#include "InputMappingContext.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UI/SaltFirstStartWidget.h"
#include "UI/SaltHUDWidget.h"
#include "UI/SaltKeysWidget.h"
#include "UI/SaltLoadingWidget.h"
#include "UI/SaltMenuWidget.h"
#include "UI/SaltSettingsWidget.h"
#include "UI/SaltStartMenuWidget.h"
#include "UI/SaltVoyagesWidget.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunMenuTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		auto CVarInt = [](const TCHAR* Name) { IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name); return V ? V->GetInt() : -1; };
		auto CVarFloat = [](const TCHAR* Name) { IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name); return V ? V->GetFloat() : -1.f; };

		ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(World->GetFirstPlayerController()) : nullptr;
		USaltGameUserSettings* S = USaltGameUserSettings::Get();
		ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		if (!PC || !S || !Player)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST MenuSuite FAIL reason=no_controller_settings_or_player settings=%s"),
				*GetNameSafe(GEngine ? GEngine->GetGameUserSettings() : nullptr));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST MenuSuite DONE passed=0 failed=1"));
			return;
		}
		PC->CloseScreens();
		const bool bVSync = S->IsVSyncEnabled();
		const float Cap = S->GetFrameRateLimit();
		const float Fov = S->FieldOfView;
		const float Brightness = S->Brightness;
		const TArray<FSaltKeyBinding> Keys = S->KeyBindings;
		float N, Scale, Min, Max;
		S->GetResolutionScaleInformationEx(N, Scale, Min, Max);
		Scale = Scale > 1.f ? Scale : 100.f;   // 0 = the engine's default: native

		// The pause menu.
		PC->SetMenuOpen(true);
		USaltMenuWidget* M = PC->GetMenu();
		Report(TEXT("MenuOpens"), PC->IsMenuOpen() && World->IsPaused() && PC->bShowMouseCursor && M && PC->GetTopScreen() == M && M->GetItemCount() == 6,
			FString::Printf(TEXT("open=%d paused=%d cursor=%d items=%d hud=%d"), PC->IsMenuOpen() ? 1 : 0, World->IsPaused() ? 1 : 0,
				PC->bShowMouseCursor ? 1 : 0, M ? M->GetItemCount() : 0, PC->GetHUDWidget() && PC->GetHUDWidget()->IsVisible() ? 1 : 0));
		if (!M)
		{
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST MenuSuite DONE passed=%d failed=%d"), Passed, Failed);
			return;
		}
		M->ChooseItem(USaltMenuWidget::FindItem(USaltMenuWidget::EItem::Settings));
		Report(TEXT("MenuCard"), !M->DescribeCard().Contains(TEXT("=''")), M->DescribeCard());

		// The settings, opened from it (the pause menu stays under them).
		M->UseItem(USaltMenuWidget::FindItem(USaltMenuWidget::EItem::Settings));
		USaltSettingsWidget* Set = Cast<USaltSettingsWidget>(PC->GetTopScreen());
		Report(TEXT("MenuSettingsOpen"), Set && World->IsPaused() && !M->IsVisible(),
			FString::Printf(TEXT("settings=%d paused=%d menu_visible=%d pages=%d"), Set ? 1 : 0, World->IsPaused() ? 1 : 0, M->IsVisible() ? 1 : 0,
				Set ? Set->GetPageCount() : 0));
		if (Set)
		{
			Set->ShowSetting(ESaltSetting::VSync);
			Set->Change(ESaltSetting::VSync, 1);
			// The editor pins its own vsync off ([SystemSettingsEditor] r.VSync=0), so the cvar is checked in the game only.
			const bool bVSyncApplied = GIsEditor || CVarInt(TEXT("r.VSync")) == (S->IsVSyncEnabled() ? 1 : 0);
			Report(TEXT("MenuVSync"), S->IsVSyncEnabled() != bVSync && bVSyncApplied,
				FString::Printf(TEXT("before=%d after=%d r.VSync=%d shown=%s"), bVSync ? 1 : 0, S->IsVSyncEnabled() ? 1 : 0, CVarInt(TEXT("r.VSync")),
					*Set->Describe(ESaltSetting::VSync)));
			Set->Change(ESaltSetting::VSync, 1);

			Set->Change(ESaltSetting::FrameCap, 1);
			const float Cap1 = S->GetFrameRateLimit();
			Set->Change(ESaltSetting::FrameCap, -1);
			Report(TEXT("MenuFrameCap"), Cap1 != Cap && FMath::IsNearlyEqual(CVarFloat(TEXT("t.MaxFPS")), S->GetFrameRateLimit(), 0.5f)
				&& FMath::IsNearlyEqual(S->GetFrameRateLimit(), Cap, 0.5f),
				FString::Printf(TEXT("before=%.0f next=%.0f back=%.0f t.MaxFPS=%.0f"), Cap, Cap1, S->GetFrameRateLimit(), CVarFloat(TEXT("t.MaxFPS"))));

			Set->Change(ESaltSetting::ResolutionScale, -1);
			float Scale1;
			S->GetResolutionScaleInformationEx(N, Scale1, Min, Max);
			const float Percentage = CVarFloat(TEXT("r.ScreenPercentage"));
			Set->Change(ESaltSetting::ResolutionScale, 1);
			Report(TEXT("MenuResolutionScale"), Scale1 < Scale && FMath::IsNearlyEqual(Percentage, Scale1, 1.f),
				FString::Printf(TEXT("before=%.0f stepped=%.0f r.ScreenPercentage=%.0f shown=%s"), Scale, Scale1, Percentage, *Set->Describe(ESaltSetting::ResolutionScale)));

			Set->Change(ESaltSetting::FieldOfView, Fov < 110.f ? 1 : -1);
			const float CameraFov = Player->GetCamera()->FieldOfView;
			Report(TEXT("MenuFieldOfView"), S->FieldOfView != Fov && FMath::IsNearlyEqual(CameraFov, S->FieldOfView),
				FString::Printf(TEXT("before=%.0f after=%.0f camera=%.0f"), Fov, S->FieldOfView, CameraFov));

			// D60: where the prompt shows, on the object or low on the screen.
			const bool bOnObject = S->bPromptOnObject;
			Set->Change(ESaltSetting::Prompts, 1);
			const FString PromptShown = Set->Describe(ESaltSetting::Prompts);
			const bool bFlipped = S->bPromptOnObject != bOnObject;
			Set->Change(ESaltSetting::Prompts, 1);
			Report(TEXT("MenuPromptPlace"), bFlipped && S->bPromptOnObject == bOnObject,
				FString::Printf(TEXT("before=%d flipped=%d shown='%s' back=%d"), bOnObject ? 1 : 0, bFlipped ? 1 : 0, *PromptShown, S->bPromptOnObject ? 1 : 0));

			// D66: the volume and the quality presets.
			const float Volume = S->MasterVolume;
			Set->Change(ESaltSetting::Volume, Volume > 0.5f ? -1 : 1);
			const float Volume1 = S->MasterVolume;
			S->MasterVolume = Volume;
			S->ApplyExtras();
			Report(TEXT("MenuVolume"), !FMath::IsNearlyEqual(Volume1, Volume), FString::Printf(TEXT("before=%.2f stepped=%.2f"), Volume, Volume1));
			const int32 Quality = S->GetOverallScalabilityLevel();
			Set->ShowSetting(ESaltSetting::Quality);
			const FString QualityShown = Set->Describe(ESaltSetting::Quality);
			Report(TEXT("MenuQualityShown"), !QualityShown.IsEmpty() && Set->GetPage() == 2, FString::Printf(TEXT("level=%d shown=%s page=%d"), Quality, *QualityShown,
				Set->GetPage()));

			// The brightness check from the display page: the display gamma follows, Esc puts it back.
			PC->OpenBrightness(true);
			USaltFirstStartWidget* First = Cast<USaltFirstStartWidget>(PC->GetTopScreen());
			if (First)
			{
				First->Step(S->Brightness < 2.9f ? 2 : -2);
				const float Gamma = GEngine->DisplayGamma;
				const float Stepped = S->Brightness;
				First->Back();
				Report(TEXT("MenuBrightness"), FMath::IsNearlyEqual(Gamma, Stepped, 0.001f) && !FMath::IsNearlyEqual(Stepped, Brightness, 0.01f)
					&& FMath::IsNearlyEqual(S->Brightness, Brightness, 0.01f) && PC->GetTopScreen() == Set,
					FString::Printf(TEXT("before=%.2f stepped=%.2f gamma=%.2f back=%.2f top_settings=%d"), Brightness, Stepped, Gamma, S->Brightness,
						PC->GetTopScreen() == Set ? 1 : 0));
			}
			else
			{
				Report(TEXT("MenuBrightness"), false, TEXT("no_screen"));
			}

			// Keys and buttons: Jump onto J, then onto E (Use's key: swap them), then everything back.
			PC->OpenKeys();
			USaltKeysWidget* K = Cast<USaltKeysWidget>(PC->GetTopScreen());
			const int32 JumpRow = USaltInputConfig::Rebindables().IndexOfByPredicate([](const FSaltRebindable& R) { return R.Id == TEXT("Jump"); });
			if (K && JumpRow != INDEX_NONE)
			{
				const FKey OldJump = USaltInputConfig::KeyOf(TEXT("Jump"));
				K->ChooseRow(JumpRow);
				K->Listen();
				K->Offer(EKeys::J);
				USaltInputConfig* Cfg = PC->GetInputConfig();
				const bool bMapped = Cfg->DefaultContext->GetMappings().ContainsByPredicate([Cfg](const FEnhancedActionKeyMapping& Map) { return Map.Action == Cfg->Jump && Map.Key == EKeys::J; });
				Report(TEXT("MenuRebind"), USaltInputConfig::KeyOf(TEXT("Jump")) == EKeys::J && bMapped && K->GetState() == USaltKeysWidget::EState::Browse,
					FString::Printf(TEXT("jump=%s mapped=%d"), *USaltInputConfig::KeyOf(TEXT("Jump")).ToString(), bMapped ? 1 : 0));
				K->Listen();
				K->Offer(EKeys::E);
				const bool bClash = K->GetState() == USaltKeysWidget::EState::Clash;
				K->Resolve(true);
				Report(TEXT("MenuRebindSwap"), bClash && USaltInputConfig::KeyOf(TEXT("Jump")) == EKeys::E && USaltInputConfig::KeyOf(TEXT("Interact")) == EKeys::J,
					FString::Printf(TEXT("clash=%d jump=%s use=%s"), bClash ? 1 : 0, *USaltInputConfig::KeyOf(TEXT("Jump")).ToString(),
						*USaltInputConfig::KeyOf(TEXT("Interact")).ToString()));
				K->Listen();
				K->Offer(EKeys::Escape);
				Report(TEXT("MenuRebindReserved"), USaltInputConfig::KeyOf(TEXT("Jump")) == EKeys::E && K->GetState() == USaltKeysWidget::EState::Browse,
					FString::Printf(TEXT("jump=%s"), *USaltInputConfig::KeyOf(TEXT("Jump")).ToString()));
				S->KeyBindings = Keys;
				S->SaveSettings();
				PC->RebuildInputMappings();
				Report(TEXT("MenuRebindRestored"), USaltInputConfig::KeyOf(TEXT("Jump")) == OldJump, FString::Printf(TEXT("jump=%s"), *USaltInputConfig::KeyOf(TEXT("Jump")).ToString()));
				PC->PopScreen(K);
			}
			else
			{
				Report(TEXT("MenuRebind"), false, TEXT("no_screen"));
			}

			Set->Back();
			Report(TEXT("MenuSettingsBack"), PC->GetTopScreen() == M && M->IsVisible() && World->IsPaused(),
				FString::Printf(TEXT("top_menu=%d visible=%d"), PC->GetTopScreen() == M ? 1 : 0, M->IsVisible() ? 1 : 0));
		}

		// The voyages, from the pause menu.
		PC->OpenVoyages();
		USaltVoyagesWidget* V = Cast<USaltVoyagesWidget>(PC->GetTopScreen());
		Report(TEXT("MenuVoyages"), V != nullptr && V->bInPlay, FString::Printf(TEXT("screen=%d in_play=%d chosen=%d"), V ? 1 : 0, V && V->bInPlay ? 1 : 0,
			V ? V->GetChosenVoyage() : -1));
		if (V)
		{
			V->Back();
		}

		PC->SetMenuOpen(false);
		Report(TEXT("MenuCloses"), !PC->IsMenuOpen() && !World->IsPaused() && !PC->bShowMouseCursor && PC->GetHUDWidget()->IsVisible(),
			FString::Printf(TEXT("open=%d paused=%d cursor=%d hud=%d"), PC->IsMenuOpen() ? 1 : 0, World->IsPaused() ? 1 : 0, PC->bShowMouseCursor ? 1 : 0,
				PC->GetHUDWidget()->IsVisible() ? 1 : 0));

		// The title over the world: not paused, no HUD; Continue goes back to play.
		PC->ShowTitle();
		USaltStartMenuWidget* T = PC->FindScreen<USaltStartMenuWidget>();
		Report(TEXT("MenuTitle"), T && !World->IsPaused() && !PC->GetHUDWidget()->IsVisible() && PC->bShowMouseCursor,
			FString::Printf(TEXT("title=%d paused=%d hud=%d chosen=%d"), T ? 1 : 0, World->IsPaused() ? 1 : 0, PC->GetHUDWidget()->IsVisible() ? 1 : 0,
				T ? T->GetChosenItem() : -1));
		PC->ContinueVoyage();
		Report(TEXT("MenuTitleContinue"), !PC->IsMenuOpen() && PC->GetHUDWidget()->IsVisible(), FString::Printf(TEXT("open=%d"), PC->IsMenuOpen() ? 1 : 0));

		// Restore what the owner had.
		S->SetVSyncEnabled(bVSync);
		S->SetFrameRateLimit(Cap);
		S->SetResolutionScaleValueEx(Scale);
		S->FieldOfView = Fov;
		S->Brightness = Brightness;
		S->ApplyDisplay();
		S->ApplyExtras();
		Player->ApplyUserSettings();
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST MenuSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunMenuTests(
		TEXT("sb.Dev.RunMenuTests"),
		TEXT("Checks the menus: the pause menu pauses, the settings apply, keys rebind, the brightness, the voyages, the title."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunMenuTests));

	/** Opens a menu screen for captures. */
	void OpenMenu(const TArray<FString>& Args, UWorld* World)
	{
		ASaltPlayerController* PC = World ? Cast<ASaltPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const FString What = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("pause"));
		if (!PC)
		{
			return;
		}
		if (What == TEXT("close"))
		{
			PC->CloseScreens();
		}
		else if (What == TEXT("title"))
		{
			PC->ShowTitle();
		}
		else if (What == TEXT("pause"))
		{
			PC->CloseScreens();
			PC->SetMenuOpen(true);
		}
		else if (What == TEXT("settings"))
		{
			PC->OpenSettings();
			if (USaltSettingsWidget* Set = Cast<USaltSettingsWidget>(PC->GetTopScreen()); Set && Args.Num() > 1)
			{
				Set->SetPage(FCString::Atoi(*Args[1]));
			}
		}
		else if (What == TEXT("voyages"))
		{
			PC->OpenVoyages();
		}
		else if (What == TEXT("keys"))
		{
			PC->OpenKeys();
			USaltKeysWidget* K = Cast<USaltKeysWidget>(PC->GetTopScreen());
			if (K && Args.Num() > 1)
			{
				// "listen": waiting for Jump's key; "clash": C offered for Jump (it dives).
				K->ChooseRow(4);
				K->Listen();
				if (Args[1].ToLower() == TEXT("clash"))
				{
					K->Offer(EKeys::LeftControl);
				}
			}
		}
		else if (What == TEXT("first"))
		{
			PC->OpenBrightness(false);
		}
		else if (What == TEXT("ask"))
		{
			PC->Ask(NSLOCTEXT("SaltDev", "AskTitle", "Return to the title?"),
				NSLOCTEXT("SaltDev", "AskBody", "You last saved 6 minutes ago. What happened since then stays at sea."),
				NSLOCTEXT("SaltDev", "AskYes", "return"), NSLOCTEXT("SaltDev", "AskNo", "stay aboard"), []() {});
		}
		else if (What == TEXT("loading"))
		{
			// The loading screen with nothing to load, held for Args[1] seconds (default 6) by a long work.
			const float Hold = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 6.f;
			PC->ShowLoading(NSLOCTEXT("SaltDev", "LoadPlace", "Ruin Isle"), NSLOCTEXT("SaltDev", "LoadLine", "1.7 km north-north-east of Saltbound Cove · at anchor"),
				FString(), [Hold]() { FPlatformProcess::Sleep(0.f); (void)Hold; }, nullptr);
			if (USaltLoadingWidget* L = PC->GetLoading())
			{
				L->HoldFor = Hold;
			}
		}
		UE_LOG(LogSaltbound, Display, TEXT("SB_MENU opened=%s top=%s"), *What, *GetNameSafe(PC->GetTopScreen()));
	}

	/** sb.Dev.MenuShots [quit]: opens every menu screen in turn and saves a picture of each with its UI (Saved/MenuShots). */
	void MenuShots(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Steps[] = { TEXT("title"), TEXT("pause"), TEXT("settings 0"), TEXT("settings 1"), TEXT("settings 2"), TEXT("settings 4"),
			TEXT("voyages"), TEXT("keys"), TEXT("keys listen"), TEXT("keys clash"), TEXT("ask"), TEXT("first"), TEXT("loading 4") };
		const bool bQuit = Args.Num() > 0 && Args[0] == TEXT("quit");
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TSharedRef<int32> Step = MakeShared<int32>(0);
		TSharedRef<double> Next = MakeShared<double>(FPlatformTime::Seconds() + 0.5);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Step, Next, bQuit](float)
		{
			UWorld* W = WeakWorld.Get();
			if (!W)
			{
				return false;
			}
			if (FPlatformTime::Seconds() < *Next)
			{
				return true;
			}
			const int32 N = UE_ARRAY_COUNT(Steps);
			const int32 I = *Step / 2;
			if (I >= N)
			{
				UE_LOG(LogSaltbound, Display, TEXT("SB_MENU_SHOTS_DONE %d"), N);
				if (bQuit)
				{
					GEngine->Exec(W, TEXT("quit"));
				}
				return false;
			}
			if (*Step % 2 == 0)
			{
				// Open the screen (a fresh pause menu under the pause-menu screens), then wait for its animations.
				TArray<FString> Parts;
				FString(Steps[I]).ParseIntoArray(Parts, TEXT(" "));
				if (Parts[0] != TEXT("title") && Parts[0] != TEXT("first") && Parts[0] != TEXT("loading"))
				{
					OpenMenu({ TEXT("pause") }, W);
				}
				if (Parts[0] != TEXT("pause"))
				{
					if (Parts[0] == TEXT("first") || Parts[0] == TEXT("title") || Parts[0] == TEXT("loading"))
					{
						OpenMenu({ TEXT("close") }, W);
					}
					OpenMenu(Parts, W);
				}
				*Next = FPlatformTime::Seconds() + 1.2;
			}
			else
			{
				const FString Name = FString(Steps[I]).Replace(TEXT(" "), TEXT("_"));
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("MenuShots") / (Name + TEXT(".png")), true, false);
				UE_LOG(LogSaltbound, Display, TEXT("SB_MENU_SHOT %s"), *Name);
				*Next = FPlatformTime::Seconds() + 0.6;
			}
			++*Step;
			return true;
		}), 0.f);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdMenuShots(
		TEXT("sb.Dev.MenuShots"),
		TEXT("sb.Dev.MenuShots [quit]: opens every menu screen in turn and saves a picture of each with its UI (Saved/MenuShots)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MenuShots));

	/** sb.Dev.MenuFlow [quit]: the voyage flow in the running game, with a picture at each step (Saved/MenuShots/flow_*): the title,
	 *  New voyage through the loading screen, play, Save now, the voyages page with its picture, Return to the title. Uses the
	 *  real voyage slots: for a worktree or test install, not a player's game. */
	void MenuFlow(const TArray<FString>& Args, UWorld* World)
	{
		struct FStep
		{
			float Wait;
			TFunction<void(ASaltPlayerController*)> Do;
		};
		auto Shot = [](const TCHAR* Name)
		{
			return [Name](ASaltPlayerController* PC)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("MenuShots") / (FString(TEXT("flow_")) + Name + TEXT(".png")), true, false);
				const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(PC);
				UE_LOG(LogSaltbound, Display, TEXT("SB_MENU_FLOW %s top=%s loading=%d paused=%d hud=%d voyage=%d save=%d playtime=%.0f"), Name, *GetNameSafe(PC->GetTopScreen()),
					PC->IsLoading() ? 1 : 0, PC->GetWorld()->IsPaused() ? 1 : 0, PC->GetHUDWidget() && PC->GetHUDWidget()->IsVisible() ? 1 : 0,
					Saves ? Saves->GetActiveVoyage() : -9, Saves && Saves->HasSave() ? 1 : 0, Saves ? Saves->PlayTime : -1.0);
			};
		};
		const bool bQuit = Args.Num() > 0 && Args[0] == TEXT("quit");
		TSharedRef<TArray<FStep>> Steps = MakeShared<TArray<FStep>>();
		Steps->Add({ 0.5f, [](ASaltPlayerController* PC) { PC->ShowTitle(); } });
		Steps->Add({ 1.5f, Shot(TEXT("1_title")) });
		Steps->Add({ 0.5f, [](ASaltPlayerController* PC) { PC->NewVoyage(); } });
		Steps->Add({ 0.8f, Shot(TEXT("2_loading_new")) });
		Steps->Add({ 3.5f, Shot(TEXT("3_play")) });
		Steps->Add({ 0.5f, [](ASaltPlayerController* PC)
		{
			PC->SetMenuOpen(true);
			PC->GetMenu()->UseItem(USaltMenuWidget::FindItem(USaltMenuWidget::EItem::Save));
		} });
		Steps->Add({ 1.5f, Shot(TEXT("4_pause_saved")) });
		Steps->Add({ 0.6f, [](ASaltPlayerController* PC) { PC->OpenVoyages(); } });
		Steps->Add({ 1.5f, Shot(TEXT("5_voyages")) });
		Steps->Add({ 0.6f, [](ASaltPlayerController* PC) { PC->PopScreen(); PC->ReturnToTitle(); } });
		Steps->Add({ 0.8f, Shot(TEXT("6_loading_title")) });
		Steps->Add({ 3.5f, Shot(TEXT("7_title_continue")) });
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TSharedRef<int32> Index = MakeShared<int32>(0);
		TSharedRef<double> Next = MakeShared<double>(FPlatformTime::Seconds() + (*Steps)[0].Wait);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Steps, Index, Next, bQuit](float)
		{
			UWorld* W = WeakWorld.Get();
			ASaltPlayerController* PC = W ? Cast<ASaltPlayerController>(W->GetFirstPlayerController()) : nullptr;
			if (!PC)
			{
				return false;
			}
			if (FPlatformTime::Seconds() < *Next)
			{
				return true;
			}
			if (*Index >= Steps->Num())
			{
				UE_LOG(LogSaltbound, Display, TEXT("SB_MENU_FLOW_DONE"));
				if (bQuit)
				{
					GEngine->Exec(W, TEXT("quit"));
				}
				return false;
			}
			(*Steps)[*Index].Do(PC);
			++*Index;
			*Next = FPlatformTime::Seconds() + (Steps->IsValidIndex(*Index) ? (*Steps)[*Index].Wait : 0.5f);
			return true;
		}), 0.f);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdMenuFlow(
		TEXT("sb.Dev.MenuFlow"),
		TEXT("sb.Dev.MenuFlow [quit]: title, new voyage, play, save, voyages, back to the title, with pictures (uses the real voyage slots)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MenuFlow));

	FAutoConsoleCommandWithWorldAndArgs CmdMenu(
		TEXT("sb.Dev.Menu"),
		TEXT("sb.Dev.Menu <title|pause|settings [page]|voyages|keys [listen|clash]|first|ask|loading [seconds]|close>: opens a menu screen (captures)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&OpenMenu));
}
#endif

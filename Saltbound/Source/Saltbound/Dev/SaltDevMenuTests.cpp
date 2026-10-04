// Saltbound - pause menu checks (sb.Dev.RunMenuTests): open and pause, change the settings, close and resume.

#include "Camera/CameraComponent.h"
#include "Core/SaltGameUserSettings.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UI/SaltMenuWidget.h"

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
		const bool bVSync = S->IsVSyncEnabled();
		const float Cap = S->GetFrameRateLimit();
		const float Fov = S->FieldOfView;
		float N, Scale, Min, Max;
		S->GetResolutionScaleInformationEx(N, Scale, Min, Max);
		Scale = Scale > 1.f ? Scale : 100.f;   // 0 = the engine's default: native

		PC->SetMenuOpen(true);
		USaltMenuWidget* M = PC->GetMenu();
		Report(TEXT("MenuOpens"), PC->IsMenuOpen() && World->IsPaused() && PC->bShowMouseCursor && M && M->GetRowCount() == 10,
			FString::Printf(TEXT("open=%d paused=%d cursor=%d rows=%d"), PC->IsMenuOpen() ? 1 : 0, World->IsPaused() ? 1 : 0,
				PC->bShowMouseCursor ? 1 : 0, M ? M->GetRowCount() : 0));
		if (!M)
		{
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST MenuSuite DONE passed=%d failed=%d"), Passed, Failed);
			return;
		}

		const int32 VRow = M->FindRow(USaltMenuWidget::ERow::VSync);
		M->ChangeRow(VRow, 1);
		// The editor pins its own vsync off ([SystemSettingsEditor] r.VSync=0), so the cvar is checked in the game only.
		const bool bVSyncApplied = GIsEditor || CVarInt(TEXT("r.VSync")) == (S->IsVSyncEnabled() ? 1 : 0);
		Report(TEXT("MenuVSync"), S->IsVSyncEnabled() != bVSync && bVSyncApplied,
			FString::Printf(TEXT("before=%d after=%d r.VSync=%d shown=%s"), bVSync ? 1 : 0, S->IsVSyncEnabled() ? 1 : 0,
				CVarInt(TEXT("r.VSync")), *M->DescribeRow(VRow)));
		M->ChangeRow(VRow, 1);

		const int32 CapRow = M->FindRow(USaltMenuWidget::ERow::FrameCap);
		M->ChangeRow(CapRow, 1);
		const float Cap1 = S->GetFrameRateLimit();
		M->ChangeRow(CapRow, -1);
		Report(TEXT("MenuFrameCap"), Cap1 != Cap && FMath::IsNearlyEqual(CVarFloat(TEXT("t.MaxFPS")), S->GetFrameRateLimit(), 0.5f) && FMath::IsNearlyEqual(S->GetFrameRateLimit(), Cap, 0.5f),
			FString::Printf(TEXT("before=%.0f next=%.0f back=%.0f t.MaxFPS=%.0f"), Cap, Cap1, S->GetFrameRateLimit(), CVarFloat(TEXT("t.MaxFPS"))));

		const int32 ScaleRow = M->FindRow(USaltMenuWidget::ERow::ResolutionScale);
		M->ChangeRow(ScaleRow, -1);
		float Scale1;
		S->GetResolutionScaleInformationEx(N, Scale1, Min, Max);
		const float Percentage = CVarFloat(TEXT("r.ScreenPercentage"));
		M->ChangeRow(ScaleRow, 1);
		Report(TEXT("MenuResolutionScale"), Scale1 < Scale && FMath::IsNearlyEqual(Percentage, Scale1, 1.f),
			FString::Printf(TEXT("before=%.0f stepped=%.0f r.ScreenPercentage=%.0f"), Scale, Scale1, Percentage));

		const int32 FovRow = M->FindRow(USaltMenuWidget::ERow::FieldOfView);
		M->ChangeRow(FovRow, Fov < 110.f ? 1 : -1);
		const float CameraFov = Player->GetCamera()->FieldOfView;
		Report(TEXT("MenuFieldOfView"), S->FieldOfView != Fov && FMath::IsNearlyEqual(CameraFov, S->FieldOfView),
			FString::Printf(TEXT("before=%.0f after=%.0f camera=%.0f"), Fov, S->FieldOfView, CameraFov));

		M->SelectRow(3);
		Report(TEXT("MenuSelect"), M->GetSelectedRow() == 3, FString::Printf(TEXT("selected=%d"), M->GetSelectedRow()));

		PC->SetMenuOpen(false);
		Report(TEXT("MenuCloses"), !PC->IsMenuOpen() && !World->IsPaused() && !PC->bShowMouseCursor,
			FString::Printf(TEXT("open=%d paused=%d cursor=%d"), PC->IsMenuOpen() ? 1 : 0, World->IsPaused() ? 1 : 0, PC->bShowMouseCursor ? 1 : 0));

		// Restore what the owner had.
		S->SetVSyncEnabled(bVSync);
		S->SetFrameRateLimit(Cap);
		S->SetResolutionScaleValueEx(Scale);
		S->FieldOfView = Fov;
		S->ApplyDisplay();
		Player->ApplyUserSettings();
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST MenuSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunMenuTests(
		TEXT("sb.Dev.RunMenuTests"),
		TEXT("Checks the pause menu: opens and pauses, applies V-Sync, frame cap, resolution scale and field of view, closes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunMenuTests));
}
#endif

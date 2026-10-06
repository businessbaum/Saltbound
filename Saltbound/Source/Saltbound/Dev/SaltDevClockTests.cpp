// Saltbound - checks of the day's clock (sb.Dev.RunClockTests, D72): the preset hold, night and the moon, the clock's rate, the
// faster night, the new day at midnight, and a lantern the player doused staying doused until the light changes.

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Props/SaltLantern.h"
#include "Saltbound.h"
#include "World/SaltLightingRig.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunClockTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		ASaltLightingRig* Rig = nullptr;
		for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
		{
			Rig = *It;
			break;
		}
		IConsoleVariable* Scale = IConsoleManager::Get().FindConsoleVariable(TEXT("sb.Time.Scale"));
		if (!Rig || Rig->DayCycle.Num() == 0 || !Scale)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST ClockSuite FAIL reason=no_rig_or_no_clock"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST ClockSuite DONE passed=0 failed=1"));
			return;
		}
		const float OwnHour = Rig->GetTimeOfDay();
		const int32 OwnDay = Rig->GetDay();
		const bool bOwnHeld = Rig->IsClockHeld();
		const float OwnScale = Scale->GetFloat();
		Scale->Set(1.f, ECVF_SetByConsole);
		// The clock's rate at the normal speed: 24 hours in DayLengthMinutes.
		const float HoursPerSecond = 24.f / (Rig->DayLengthMinutes * 60.f);

		// sb.Lighting Day: the reviewed look, the clock held there.
		Rig->ApplyPreset(TEXT("Day"));
		Report(TEXT("PresetHolds"), FMath::IsNearlyEqual(Rig->GetTimeOfDay(), Rig->GetLookHour(TEXT("Day")), 0.001f) && Rig->IsClockHeld()
			&& FMath::IsNearlyEqual(Rig->GetSunElevation(), 32.f, 0.1f) && !Rig->ShouldLanternsBeLit() && Rig->GetNightAmount() < 0.01f,
			FString::Printf(TEXT("hour=%.2f held=%d sun=%.1f lanterns=%d night=%.2f"), Rig->GetTimeOfDay(), Rig->IsClockHeld() ? 1 : 0,
			Rig->GetSunElevation(), Rig->ShouldLanternsBeLit() ? 1 : 0, Rig->GetNightAmount()));

		// Night: the moon rules, the lanterns burn.
		Rig->SetTimeOfDay(23.f, true);
		Report(TEXT("NightMoon"), Rig->GetDayPhase() == TEXT("Night") && Rig->GetNightAmount() > 0.99f && Rig->ShouldLanternsBeLit()
			&& Rig->GetSunElevation() < -20.f, FString::Printf(TEXT("phase=%s night=%.2f lanterns=%d sun=%.1f"),
			*Rig->GetDayPhase().ToString(), Rig->GetNightAmount(), Rig->ShouldLanternsBeLit() ? 1 : 0, Rig->GetSunElevation()));

		// The clock runs (in steps of a second, as frames would).
		auto Run = [Rig](float Seconds) { for (float T = 0.f; T < Seconds - 0.001f; T += 0.5f) { Rig->Tick(0.5f); } };
		Rig->SetTimeOfDay(10.f, false);
		Run(10.f);
		const float DayRun = Rig->GetTimeOfDay() - 10.f;
		Report(TEXT("ClockRuns"), FMath::IsNearlyEqual(DayRun, 10.f * HoursPerSecond, 0.002f), FString::Printf(
			TEXT("advanced=%.4f h in 10 s (want %.4f)"), DayRun, 10.f * HoursPerSecond));
		Rig->SetTimeOfDay(23.f, false);
		Run(10.f);
		const float NightRun = Rig->GetTimeOfDay() - 23.f;
		Report(TEXT("NightFaster"), FMath::IsNearlyEqual(NightRun, 10.f * HoursPerSecond * Rig->NightSpeed, 0.003f), FString::Printf(
			TEXT("advanced=%.4f h in 10 s (want %.4f)"), NightRun, 10.f * HoursPerSecond * Rig->NightSpeed));

		// Midnight starts a new day.
		Rig->SetDay(3);
		Rig->SetTimeOfDay(23.99f, false);
		Run(10.f);
		Report(TEXT("NewDay"), Rig->GetDay() == 4 && Rig->GetTimeOfDay() < 0.5f, FString::Printf(TEXT("day=%d hour=%.3f"), Rig->GetDay(),
			Rig->GetTimeOfDay()));

		// A doused lantern stays doused while the night goes on, and the dawn's change of light is what puts it out for good.
		ASaltLantern* Lantern = nullptr;
		for (TActorIterator<ASaltLantern> It(World); It; ++It)
		{
			Lantern = *It;
			break;
		}
		if (Lantern)
		{
			Rig->SetTimeOfDay(22.f, false);
			const bool bLitAtNight = Lantern->IsLit();
			Lantern->SetLit(false);   // the player douses it
			Run(20.f);
			const bool bStillOut = !Lantern->IsLit();
			Rig->SetTimeOfDay(23.f, false);   // setting the clock lights every lantern again
			const bool bRelit = Lantern->IsLit();
			Report(TEXT("LanternChoiceKept"), bLitAtNight && bStillOut && bRelit, FString::Printf(TEXT("litAtNight=%d stillOut=%d relitBySet=%d"),
				bLitAtNight ? 1 : 0, bStillOut ? 1 : 0, bRelit ? 1 : 0));
		}
		else
		{
			Report(TEXT("LanternChoiceKept"), true, TEXT("no lantern in this map (skipped)"));
		}

		Scale->Set(OwnScale, ECVF_SetByConsole);
		Rig->SetDay(OwnDay);
		Rig->SetTimeOfDay(OwnHour, bOwnHeld);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST ClockSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunClockTests(
		TEXT("sb.Dev.RunClockTests"),
		TEXT("D72: checks the day's clock (preset hold, night and moon, rate, faster night, new day, a doused lantern)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunClockTests));
}
#endif

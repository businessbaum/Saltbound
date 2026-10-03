// Saltbound - the wind: one global wind with slow gusts and direction shifts.

#include "World/SaltWindSubsystem.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Saltbound.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdWind(
		TEXT("sb.Wind"),
		TEXT("Set the wind: sb.Wind <FromDeg> <Speed m/s> [Gustiness 0..1]. Without arguments, print the current wind."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltWindSubsystem* Wind = World ? World->GetSubsystem<USaltWindSubsystem>() : nullptr;
			if (!Wind)
			{
				return;
			}
			if (Args.Num() >= 2)
			{
				const float Gust = Args.Num() >= 3 ? FCString::Atof(*Args[2]) : Wind->GetGustiness();
				Wind->SetBaseWind(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]) * 100.f, Gust);
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_WIND from=%.1fdeg speed=%.2fm/s (base from=%.1f speed=%.2fm/s gust=%.2f)"),
				Wind->GetFromDirectionDeg(), Wind->GetSpeed() / 100.f, Wind->GetBaseFromDeg(), Wind->GetBaseSpeed() / 100.f, Wind->GetGustiness());
		}));
}

TStatId USaltWindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltWindSubsystem, STATGROUP_Tickables);
}

void USaltWindSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Time += DeltaTime;
	Evaluate();
}

void USaltWindSubsystem::Evaluate()
{
	constexpr double TwoPi = 2.0 * UE_DOUBLE_PI;
	// Incommensurate periods: gusts of ~20 s with faster flutter; shifts over about a minute.
	const double Gust = 0.6 * FMath::Sin(TwoPi * Time / 23.0) + 0.3 * FMath::Sin(TwoPi * Time / 9.1 + 1.0)
		+ 0.1 * FMath::Sin(TwoPi * Time / 3.7 + 2.0);
	const double Shift = 0.7 * FMath::Sin(TwoPi * Time / 61.0 + 0.4) + 0.3 * FMath::Sin(TwoPi * Time / 17.0 + 0.5);
	CurrentSpeed = FMath::Max(0.f, BaseSpeed * (1.f + Gustiness * (float)Gust));
	CurrentFromDeg = BaseFromDeg + ShiftDeg * Gustiness * (float)Shift;
}

void USaltWindSubsystem::SetBaseWind(float FromDeg, float SpeedCmPerSecond, float InGustiness)
{
	BaseFromDeg = FRotator::NormalizeAxis(FromDeg);
	BaseSpeed = FMath::Max(0.f, SpeedCmPerSecond);
	Gustiness = FMath::Clamp(InGustiness, 0.f, 1.f);
	Evaluate();
}

FVector2D USaltWindSubsystem::GetWindVelocity(const FVector2D& WorldXY) const
{
	// The air moves towards the opposite of where it comes from.
	const float Rad = FMath::DegreesToRadians(CurrentFromDeg);
	return FVector2D(-FMath::Cos(Rad), -FMath::Sin(Rad)) * CurrentSpeed;
}

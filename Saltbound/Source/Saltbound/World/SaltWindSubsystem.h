// Saltbound - the wind: one global wind with slow gusts and direction shifts.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltWindSubsystem.generated.h"

/**
 * Global wind for sailing (and later flags, sounds, clouds). Deterministic: speed and direction vary with
 * smooth sums of slow sines, so tests and replays see the same wind.
 * Directions follow the meteorological convention: the direction the wind blows FROM, in degrees
 * (0 = from +X, 90 = from +Y). Speeds are stored in cm/s (engine units).
 * Console: sb.Wind <FromDeg> <Speed m/s> [Gustiness 0..1]   (no arguments: print the current wind)
 */
UCLASS()
class SALTBOUND_API USaltWindSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Air velocity (where the air moves to), cm/s, at a world position. */
	FVector2D GetWindVelocity(const FVector2D& WorldXY) const;
	/** Current direction the wind blows from, degrees. */
	float GetFromDirectionDeg() const { return CurrentFromDeg; }
	/** Current wind speed, cm/s. */
	float GetSpeed() const { return CurrentSpeed; }

	/** Sets the mean wind. Gustiness scales speed variation (0 = steady); shifts scale with it too. */
	void SetBaseWind(float FromDeg, float SpeedCmPerSecond, float InGustiness);
	float GetBaseFromDeg() const { return BaseFromDeg; }
	float GetBaseSpeed() const { return BaseSpeed; }
	float GetGustiness() const { return Gustiness; }

private:
	void Evaluate();

	/** Default: from 200 deg (the swell of the default ocean travels towards 20 deg), 7 m/s (~14 kn). */
	float BaseFromDeg = 200.f;
	float BaseSpeed = 700.f;
	float Gustiness = 0.2f;
	/** Direction wander at Gustiness 1, degrees. */
	float ShiftDeg = 30.f;

	double Time = 0.0;
	float CurrentFromDeg = 200.f;
	float CurrentSpeed = 700.f;
};

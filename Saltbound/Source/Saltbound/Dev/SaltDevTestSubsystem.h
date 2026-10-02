// Saltbound - development: automated movement tests and dev console commands.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltDevTestSubsystem.generated.h"

class ASaltPlayerCharacter;
class ASaltDevMovingPlatform;

/**
 * Runs scripted test sequences in a game world and logs results as "SB_TEST <name> PASS|FAIL key=value ..." lines.
 * Console (non-shipping builds):
 *   sb.Dev.RunMovementTests [MotionScale=1] [Speed=300] [TurnRate=8]
 *   sb.Dev.Platform <MotionScale> [Speed] [TurnRate]   (Speed/TurnRate force a constant drive; omit to use the helm)
 *   sb.Dev.ResetPlayer
 */
UCLASS()
class SALTBOUND_API USaltDevTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RunMovementSuite(float MotionScale, float Speed, float TurnRate);
	void ConfigurePlatform(float MotionScale, bool bForce, float Speed, float TurnRate);
	void ResetPlayer();
	bool IsRunning() const { return Steps.IsValidIndex(StepIndex); }

private:
	struct FStep
	{
		FString Name;
		float Duration = 0.f;
		TFunction<void()> OnStart;
		TFunction<void(float)> OnTick;
		TFunction<void()> OnEnd;
	};

	/** Per-frame statistics of a point expressed in deck space. */
	struct FDeckTrack
	{
		void Reset() { *this = FDeckTrack(); }
		void Add(const FVector& Local, float Dt);
		int32 Frames = 0;
		FVector First = FVector::ZeroVector;
		FVector Prev = FVector::ZeroVector;
		FVector PrevDelta = FVector::ZeroVector;
		double SumSqAccel = 0.0;
		float MaxDrift = 0.f;
		float MaxAccelStep = 0.f;
		float LastSpeed = 0.f;
	};

	ASaltPlayerCharacter* GetPlayer() const;
	ASaltDevMovingPlatform* FindPlatform() const;
	FVector FeetInDeckSpace() const;
	FVector EyeInDeckSpace() const;
	bool IsPlayerOnDeck() const;
	void PlacePlayerOnDeck(const FVector& DeckLocalFeet, float RelativeYaw);
	void Report(const FString& Name, bool bPass, const FString& Details);

	TArray<FStep> Steps;
	int32 StepIndex = INDEX_NONE;
	float StepTime = 0.f;
	bool bStepStarted = false;

	TWeakObjectPtr<ASaltDevMovingPlatform> Platform;
	FDeckTrack Feet;
	FDeckTrack Eye;
	int32 OffDeckFrames = 0;
	int32 Passed = 0;
	int32 Failed = 0;
	FVector MarkLocal = FVector::ZeroVector;
	FVector LandLocal = FVector::ZeroVector;
	bool bWasAirborne = false;
	bool bLanded = false;
};

// Saltbound - the sea's events (D88, FH5: the light under the water).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltSeaEvents.generated.h"

class ASaltBoat;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * D88 (FirstHour.md 0:37, FH5: "Home by lantern light. Halfway, a light under the water, there and gone."): something long and
 * turquoise passes under the boat: it comes up out of the deep on one side, glides under the keel 8 m down and sinks away on the
 * other, its glow breathing slowly, in 11 s; then it is gone. Seen only as a glow through the dark water (SM_SeaLight, 11 m, on
 * MI_SeaGlow: brightest facing the eye, fading to its outline; driven here) with a light of its own that catches the hull. It never
 * touches the boat.
 */
UCLASS()
class SALTBOUND_API ASaltSeaLight : public AActor
{
	GENERATED_BODY()

public:
	ASaltSeaLight();

	/** Starts the pass under Boat (its place and heading now; the path then stays where the boat was). */
	void Pass(const ASaltBoat* Boat);
	/** 0 .. 1 through the pass (negative before it begins). */
	float GetProgress() const { return Duration > 0.f ? Time / Duration : -1.f; }

	static constexpr float Duration = 11.f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;

	/** The path in the world: from, under, to (cm), and the way across. */
	FVector From = FVector::ZeroVector;
	FVector Under = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float Time = -1.f;
};

/**
 * D88: when the sea's events happen. The light under the water: once, on the first night at sea after casting off (the story's
 * CastOff beat): the boat under way (over 1.5 knots, not moored or anchored), full night, the boat at least 300 m from her home
 * mooring; it is saved as the beat SeaLight. sb.Event.SeaLight passes it under the boat now.
 */
UCLASS()
class SALTBOUND_API USaltSeaEventsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Whether the light under the water would come now (the conditions above, the beat aside). */
	bool SeaLightConditions(FString* OutWhyNot = nullptr) const;
	/** Passes the light under the player's boat now (sb.Event.SeaLight); bRecord saves the beat. */
	ASaltSeaLight* PassSeaLight(bool bRecord);

	static const FName SeaLightBeat;

private:
	ASaltBoat* FindBoat() const;

	float NextCheck = 0.f;
	float Clock = 0.f;
	UPROPERTY(Transient) TObjectPtr<ASaltSeaLight> Passing;
};

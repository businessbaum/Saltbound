// Saltbound - control station (helm, winch, ...) that the player can step into and operate.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/SaltInteractableComponent.h"
#include "SaltStationComponent.generated.h"

/** One group of keys and what they do (D58): "A D Steer". */
USTRUCT(BlueprintType)
struct FSaltControlHint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station") TArray<FString> Keys;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station") FText Action;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSaltStationOccupancySignature, class USaltStationComponent*, Station, ASaltPlayerCharacter*, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSaltStationActionSignature, class USaltStationComponent*, Station, int32, Action);

/**
 * A focus volume that, when used, puts the player into "station mode":
 * the player stands at StandOffset (feet position, in this component's space), looks around within
 * limits relative to the station's facing, and movement input is routed to the station instead of walking.
 * The owner reads GetControlInput() each tick. Interact again to step back to ExitOffset.
 */
UCLASS(ClassGroup = (Saltbound), meta = (BlueprintSpawnableComponent))
class SALTBOUND_API USaltStationComponent : public USaltInteractableComponent
{
	GENERATED_BODY()

public:
	USaltStationComponent();

	/** Player feet position while operating, relative to this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FVector StandOffset = FVector(-60.f, 0.f, -100.f);

	/** Player feet position after leaving, relative to this component. Falls back to the entry point if blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FVector ExitOffset = FVector(-110.f, 0.f, -100.f);

	/** Look limits relative to the station facing (+X of this component), in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station", meta = (ClampMin = "0", ClampMax = "180"))
	float YawLimit = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	float PitchMin = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	float PitchMax = 60.f;

	/** Prompt shown while operating the station. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FText LeavePromptText;
	/** False while the player may not step out (D62: the hoist between two decks). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Station")
	bool bLeaveAllowed = true;

	/** The station's controls, shown while operating it in the key-scrap prompt (D58), side by side: keys (letters, e.g.
	 *  A and D) and what they do ("Steer"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	TArray<FSaltControlHint> Controls;

	/** Optional status line under the controls (the boat's speed, the sail and the wind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FText ControlsHintText;

	UPROPERTY(BlueprintAssignable, Category = "Station")
	FSaltStationOccupancySignature OnEntered;

	UPROPERTY(BlueprintAssignable, Category = "Station")
	FSaltStationOccupancySignature OnLeft;

	/** Button presses while operating the station: 0 = primary (Jump key), 1 = secondary (Dive key). */
	UPROPERTY(BlueprintAssignable, Category = "Station")
	FSaltStationActionSignature OnAction;

	//~ USaltInteractableComponent
	virtual bool CanInteract(const ASaltPlayerCharacter* Player) const override;
	virtual void Interact(ASaltPlayerCharacter* Player) override;

	UFUNCTION(BlueprintPure, Category = "Station")
	ASaltPlayerCharacter* GetOccupant() const { return Occupant; }

	/** Control axes from the occupant: X = right/left (steer), Y = forward/back. Zero when unoccupied. */
	UFUNCTION(BlueprintPure, Category = "Station")
	FVector2D GetControlInput() const { return Occupant ? ControlInput : FVector2D::ZeroVector; }

	// Called by the player character.
	void NotifyEntered(ASaltPlayerCharacter* Player);
	void NotifyLeft(ASaltPlayerCharacter* Player);
	void SetControlInput(const FVector2D& Input) { ControlInput = Input; }
	void TriggerAction(int32 Action) { OnAction.Broadcast(this, Action); }

	FVector GetStandLocation() const { return GetComponentTransform().TransformPosition(StandOffset); }
	FVector GetExitLocation() const { return GetComponentTransform().TransformPosition(ExitOffset); }

private:
	UPROPERTY(Transient)
	TObjectPtr<ASaltPlayerCharacter> Occupant;

	FVector2D ControlInput = FVector2D::ZeroVector;
};

// Saltbound - the hub ship's hanging ropes (D69, owner: "loose ends and no physics in the ropes"): real cables that sag
// and move in the wind, and her lanterns' flicker.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubRopes.generated.h"

class UCableComponent;
class UMaterialInterface;
class UPointLightComponent;

/** One rope between two fixed points (the actor's frame, cm): a yard's lift, a brace, a buntline, a fall to its pin. */
USTRUCT(BlueprintType)
struct FSaltRopeSpan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope") FVector Start = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope") FVector End = FVector::ZeroVector;
	/** The rope's length over the straight distance (1 = taut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope", meta = (ClampMin = "1")) float Slack = 1.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope", meta = (Units = "cm")) float Width = 6.f;
	/** Index into ASaltHubRopes::Materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope") int32 Material = 0;
	/** How much the wind moves it (a slack fall more than a taut brace). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope") float Wind = 1.f;
};

/**
 * Builds a UCableComponent (verlet rope, the boat's sheets are the same) for every span, in the editor as well, and blows
 * on them: a steady wind along WindDirection (she lies head to wind, so it blows aft) with gusts and a sideways flutter,
 * each rope in its own phase. The level's point lights tagged "HubLantern" flicker a little (the game's lantern does the
 * same, ASaltLantern).
 */
UCLASS()
class SALTBOUND_API ASaltHubRopes : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubRopes();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ropes") TArray<FSaltRopeSpan> Spans;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ropes") TArray<TObjectPtr<UMaterialInterface>> Materials;
	/** Where the wind blows to, in the actor's frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ropes") FVector WindDirection = FVector(-1.f, 0.f, 0.f);
	/** The steady wind's push on a rope (cm/s2; gravity is 980). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ropes") float WindAccel = 140.f;
	/** Gusts: how much the push varies (0..1), and how fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ropes") float GustAmount = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ropes") float GustRate = 0.35f;
	/** The lanterns' flicker (fraction of their intensity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanterns") float FlickerAmount = 0.07f;

	int32 NumCables() const { return Cables.Num(); }

private:
	void BuildCables();
	void FindLanterns();

	UPROPERTY(Transient) TArray<TObjectPtr<UCableComponent>> Cables;
	TArray<float> Phases;
	struct FLamp
	{
		TWeakObjectPtr<UPointLightComponent> Light;
		float Base = 0.f;
		float Seed = 0.f;
	};
	TArray<FLamp> Lamps;
	float Time = 0.f;
	int32 BuiltHash = 0;
};

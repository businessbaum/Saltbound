// Saltbound - development: marker that sits on the CPU-evaluated water surface.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltDevWaterProbe.generated.h"

class UStaticMeshComponent;

/**
 * Visual consistency check between gameplay water queries and the rendered ocean:
 * each tick the probe moves to the CPU-sampled surface at its XY. If the rendered waves and the CPU
 * wave field agree, the sphere stays half-submerged on every crest and trough.
 */
UCLASS()
class SALTBOUND_API ASaltDevWaterProbe : public AActor
{
	GENERATED_BODY()

public:
	ASaltDevWaterProbe();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UPROPERTY(EditAnywhere, Category = "Probe")
	FVector2D AnchorXY = FVector2D::ZeroVector;

	/** Use the actor's placed XY as the anchor. */
	UPROPERTY(EditAnywhere, Category = "Probe")
	bool bAnchorAtPlacement = true;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Marker;

	bool bAnchored = false;
};

// Saltbound - world access point for water queries.

#pragma once

#include "CoreMinimal.h"
#include "Ocean/SaltWaveTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltOceanSubsystem.generated.h"

class ASaltOcean;

/** Finds the active ocean and answers water queries (flat water at Z = 0 when there is no ocean). */
UCLASS()
class SALTBOUND_API USaltOceanSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterOcean(ASaltOcean* Ocean);
	void UnregisterOcean(ASaltOcean* Ocean);

	UFUNCTION(BlueprintPure, Category = "Ocean")
	ASaltOcean* GetOcean() const;

	UFUNCTION(BlueprintPure, Category = "Ocean")
	FSaltWaterSample SampleWater(FVector2D WorldXY) const;

private:
	TWeakObjectPtr<ASaltOcean> ActiveOcean;
};

// Saltbound - a mooring of the hub ship (D62: the fitting berth, the fleet berths; spaces.dock in HubShip_Data.json).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubMooring.generated.h"

class USaltMooringPointComponent;

/** Its root is the mooring point: the boat's moored pose (+X its bow), the quay's edge Reach to starboard (+) or port (-). */
UCLASS()
class SALTBOUND_API ASaltHubMooring : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubMooring();

	USaltMooringPointComponent* GetPoint() const { return Point; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltMooringPointComponent> Point;
};

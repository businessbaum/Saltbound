// Saltbound - a mooring of the hub ship (D62).

#include "Hub/SaltHubMooring.h"

#include "Boat/SaltMooringPoint.h"

ASaltHubMooring::ASaltHubMooring()
{
	PrimaryActorTick.bCanEverTick = false;
	Point = CreateDefaultSubobject<USaltMooringPointComponent>(TEXT("Point"));
	SetRootComponent(Point);
}

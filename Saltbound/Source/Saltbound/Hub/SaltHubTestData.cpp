// Saltbound - what the hub tests measure against (D63; see SaltHubTestData.h).

#include "Hub/SaltHubTestData.h"

ASaltHubTestData::ASaltHubTestData()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SetActorHiddenInGame(true);
}

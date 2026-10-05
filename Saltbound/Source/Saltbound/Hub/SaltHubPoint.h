// Saltbound - an interaction point of the hub ship (D62, spaces.<space>.e in HubShip_Data.json): its prompt from the data; it
// does nothing yet but say so in the log (the NPCs and their screens come later).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltHubPoint.generated.h"

class USaltInteractableComponent;
class ASaltPlayerCharacter;

UCLASS()
class SALTBOUND_API ASaltHubPoint : public AActor
{
	GENERATED_BODY()

public:
	ASaltHubPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName Space;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FName PointId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FText Prompt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub") FVector UseExtent = FVector(40.f, 40.f, 50.f);

	USaltInteractableComponent* GetUse() const { return Use; }
	/** How often it has been used this session (tests). */
	int32 GetUses() const { return Uses; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UFUNCTION() void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltInteractableComponent> Use;
	int32 Uses = 0;
};

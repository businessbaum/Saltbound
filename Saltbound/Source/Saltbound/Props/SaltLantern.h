// Saltbound - hanging/standing oil lantern: warm light, subtle flicker, can be lit or doused.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltLantern.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class USaltInteractableComponent;
class UMaterialInstanceDynamic;
class ASaltPlayerCharacter;

UCLASS()
class SALTBOUND_API ASaltLantern : public AActor
{
	GENERATED_BODY()

public:
	ASaltLantern();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Lantern")
	void SetLit(bool bNewLit);

	UFUNCTION(BlueprintPure, Category = "Lantern")
	bool IsLit() const { return bLit; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lantern")
	bool bLit = true;

	/** Luminous intensity when lit (a real oil lantern is roughly 10-20 cd). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lantern", meta = (Units = "cd"))
	float Intensity = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lantern", meta = (Units = "K"))
	float Temperature = 2100.f;

	/** Relative flicker amplitude (0 = steady). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lantern", meta = (ClampMin = "0", ClampMax = "0.5"))
	float FlickerAmount = 0.08f;

	/** Emissive strength of the glass when lit (material parameter GlowIntensity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lantern")
	float GlassGlow = 80.f;

private:
	UFUNCTION()
	void HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);

	void ApplyState();

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltInteractableComponent> Use;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlassMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlameMID;

	float FlickerTime = 0.f;
	float FlickerSeed = 0.f;
};

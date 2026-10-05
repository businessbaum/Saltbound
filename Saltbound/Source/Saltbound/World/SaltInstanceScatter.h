// Saltbound - a set of instanced meshes placed by a content script (sea grass, shells, small rocks, the home island's
// palms, trees and bushes).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltInstanceScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Decorative scatter: one mesh, many instances, culled at a distance (0 = never); no collision unless asked for (the
 * trees' trunks, D60: the mesh's simple collision). The instance list is written by Tools/ue_scripts, so the placement
 * is reproducible.
 */
UCLASS()
class SALTBOUND_API ASaltInstanceScatter : public AActor
{
	GENERATED_BODY()

public:
	ASaltInstanceScatter();

	/** Replaces all instances (world-space transforms). */
	UFUNCTION(BlueprintCallable, Category = "Scatter")
	void SetInstances(UStaticMesh* Mesh, UMaterialInterface* Material, const TArray<FTransform>& WorldTransforms,
		float CullStart = 4000.f, float CullEnd = 6000.f, bool bCastShadow = false, bool bCollide = false);

	UFUNCTION(BlueprintPure, Category = "Scatter")
	int32 GetInstanceCount() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};

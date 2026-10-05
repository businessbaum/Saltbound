// Saltbound - a set of instanced meshes placed by a content script (sea grass, shells, small rocks).

#include "World/SaltInstanceScatter.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ASaltInstanceScatter::ASaltInstanceScatter()
{
	PrimaryActorTick.bCanEverTick = false;
	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	SetRootComponent(Instances);
	Instances->SetMobility(EComponentMobility::Static);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false);
	Instances->bAffectDistanceFieldLighting = false;
}

void ASaltInstanceScatter::SetInstances(UStaticMesh* Mesh, UMaterialInterface* Material, const TArray<FTransform>& WorldTransforms,
	float CullStart, float CullEnd, bool bCastShadow, bool bCollide)
{
	Instances->ClearInstances();
	Instances->SetStaticMesh(Mesh);
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
	Instances->SetCullDistances((int32)CullStart, (int32)CullEnd);
	Instances->SetCastShadow(bCastShadow);
	if (bCollide)
	{
		Instances->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Instances->AddInstances(WorldTransforms, false, true, false);
	Instances->MarkRenderStateDirty();
}

int32 ASaltInstanceScatter::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}

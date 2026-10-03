// Saltbound - hanging/standing oil lantern: warm light, subtle flicker, can be lit or doused.

#include "Props/SaltLantern.h"

#include "Components/PointLightComponent.h"
#include "Core/SaltContent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName GlassSlot(TEXT("MI_LanternGlass"));
	const FName FlameSlot(TEXT("MI_CandleFlame"));
	const FName GlowParam(TEXT("GlowIntensity"));
}

ASaltLantern::ASaltLantern()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 30.f;

	// The mesh (SM_Lantern) is assigned in OnConstruction; see SaltContent.h.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The light sits inside opaque glass panes; a shadow-casting shell would trap it.
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 13.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->SetUseTemperature(true);
	Light->SetTemperature(Temperature);
	Light->SetAttenuationRadius(900.f);
	Light->SetSourceRadius(3.f);
	Light->SetSoftSourceRadius(4.f);
	Light->SetCastShadows(true);
	Light->bUseInverseSquaredFalloff = true;

	Use = CreateDefaultSubobject<USaltInteractableComponent>(TEXT("Use"));
	Use->SetupAttachment(Mesh);
	Use->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
	Use->SetBoxExtent(FVector(12.f, 12.f, 22.f));
}

void ASaltLantern::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!Mesh->GetStaticMesh())
	{
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SaltContent::LanternMesh));
	}
	Light->SetTemperature(Temperature);
	ApplyState();
}

void ASaltLantern::BeginPlay()
{
	Super::BeginPlay();
	if (!Mesh->GetStaticMesh())
	{
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SaltContent::LanternMesh));
	}
	FlickerSeed = FMath::FRandRange(0.f, 100.f);
	Use->OnInteract.AddDynamic(this, &ASaltLantern::HandleInteract);
	const int32 GlassIndex = Mesh->GetMaterialIndex(GlassSlot);
	const int32 FlameIndex = Mesh->GetMaterialIndex(FlameSlot);
	if (GlassIndex != INDEX_NONE)
	{
		GlassMID = Mesh->CreateDynamicMaterialInstance(GlassIndex);
	}
	if (FlameIndex != INDEX_NONE)
	{
		FlameMID = Mesh->CreateDynamicMaterialInstance(FlameIndex);
	}
	ApplyState();
}

void ASaltLantern::SetLit(bool bNewLit)
{
	bLit = bNewLit;
	ApplyState();
}

void ASaltLantern::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	SetLit(!bLit);
}

void ASaltLantern::ApplyState()
{
	Light->SetVisibility(bLit);
	Light->SetIntensity(Intensity);
	Use->PromptText = bLit ? NSLOCTEXT("Saltbound", "LanternDouse", "Douse the lantern") : NSLOCTEXT("Saltbound", "LanternLight", "Light the lantern");
	if (GlassMID)
	{
		GlassMID->SetScalarParameterValue(GlowParam, bLit ? GlassGlow : 0.f);
	}
	if (FlameMID)
	{
		FlameMID->SetScalarParameterValue(GlowParam, bLit ? 600.f : 0.f);
	}
}

void ASaltLantern::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLit || FlickerAmount <= 0.f)
	{
		return;
	}
	FlickerTime += DeltaSeconds;
	// Two layered noise rates: slow breathing plus small fast flutter.
	const float Slow = FMath::PerlinNoise1D(FlickerTime * 0.9f + FlickerSeed);
	const float Fast = FMath::PerlinNoise1D(FlickerTime * 7.3f + FlickerSeed * 2.f);
	const float Factor = 1.f + FlickerAmount * (0.7f * Slow + 0.3f * Fast);
	Light->SetIntensity(Intensity * Factor);
	if (GlassMID)
	{
		GlassMID->SetScalarParameterValue(GlowParam, GlassGlow * Factor);
	}
}

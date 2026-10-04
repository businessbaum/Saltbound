// Saltbound - something lying about that the player can pick up into the satchel (D50).

#include "World/SaltPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Core/SaltItems.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"

ASaltPickup::ASaltPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // never trips the player; the box takes the focus
	Mesh->SetCanEverAffectNavigation(false);
	SetRootComponent(Mesh);
	Interaction = CreateDefaultSubobject<USaltInteractableComponent>(TEXT("Interaction"));
	Interaction->SetupAttachment(Mesh);
	Interaction->SetRelativeLocation(FVector(0.0, 0.0, 15.0));
	Interaction->SetBoxExtent(FVector(35.f, 35.f, 25.f));
}

void ASaltPickup::Resolve()
{
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	const FSaltItemDef* Def = SaltItems::Find(ItemId);
	if (Def && Def->Mesh && (!Mesh->GetStaticMesh() || Mesh->GetStaticMesh()->GetPathName() != Def->Mesh))
	{
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Def->Mesh));
	}
	if (Def)
	{
		Interaction->PromptText = FText::Format(NSLOCTEXT("Saltbound", "PickUp", "Pick up {0}"), Def->Name);
	}
}

void ASaltPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Resolve();
}

void ASaltPickup::BeginPlay()
{
	Super::BeginPlay();
	Resolve();
	Interaction->OnInteract.AddDynamic(this, &ASaltPickup::HandleInteract);
	if (const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		SetCollected(Saves->IsCollected(PickupId));
	}
}

void ASaltPickup::SetCollected(bool bInCollected)
{
	bCollected = bInCollected;
	SetActorHiddenInGame(bCollected);
	Interaction->SetCollisionEnabled(bCollected ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

bool ASaltPickup::Collect(ASaltPlayerCharacter* Player)
{
	USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	const FSaltItemDef* Def = SaltItems::Find(ItemId);
	if (bCollected || !Satchel || !Def)
	{
		return false;
	}
	ASaltPlayerController* PC = Cast<ASaltPlayerController>(Player->GetController());
	if (Satchel->Add(ItemId, Count) <= 0)
	{
		if (PC)
		{
			PC->ShowNotice(NSLOCTEXT("Saltbound", "SatchelFull", "Your satchel is full"));
		}
		return false;
	}
	SetCollected(true);
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		Saves->MarkCollected(PickupId);
	}
	if (PC)
	{
		PC->ShowNotice(FText::Format(NSLOCTEXT("Saltbound", "PickedUp", "+{0}  {1}"), FText::AsNumber(Count), Def->Name));
	}
	return true;
}

void ASaltPickup::HandleInteract(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	Collect(Player);
}

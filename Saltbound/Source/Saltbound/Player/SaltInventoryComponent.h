// Saltbound - the player's satchel (D50): a fixed number of slots holding item stacks.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SaltItems.h"
#include "SaltInventoryComponent.generated.h"

UCLASS(ClassGroup = (Saltbound), meta = (BlueprintSpawnableComponent))
class SALTBOUND_API USaltInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USaltInventoryComponent();

	static constexpr int32 Capacity = 24;

	/** Adds up to Count (filling stacks of the same item first); returns how many fitted. */
	int32 Add(FName Id, int32 Count);
	/** Removes up to Count; returns how many were removed. */
	int32 Remove(FName Id, int32 Count);
	int32 CountOf(FName Id) const;

	const TArray<FSaltItemStack>& GetSlots() const { return Slots; }
	/** Replaces the contents (loading a save). */
	void SetSlots(const TArray<FSaltItemStack>& InSlots);

	/** Broadcast whenever the contents change. */
	FSimpleMulticastDelegate OnChanged;

private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TArray<FSaltItemStack> Slots;
};

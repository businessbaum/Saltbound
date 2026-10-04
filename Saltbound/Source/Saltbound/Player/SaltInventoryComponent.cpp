// Saltbound - the player's satchel (D50): a fixed number of slots holding item stacks.

#include "Player/SaltInventoryComponent.h"

USaltInventoryComponent::USaltInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Slots.SetNum(Capacity);
	// Starting gear: two coils of rope.
	Slots[0] = { TEXT("RopeCoil"), 2 };
}

int32 USaltInventoryComponent::Add(FName Id, int32 Count, int32* OutFirstSlot)
{
	if (OutFirstSlot)
	{
		*OutFirstSlot = INDEX_NONE;
	}
	const FSaltItemDef* Def = SaltItems::Find(Id);
	if (!Def || Count <= 0)
	{
		return 0;
	}
	int32 Left = Count;
	auto Took = [OutFirstSlot](int32 I)
	{
		if (OutFirstSlot && *OutFirstSlot == INDEX_NONE)
		{
			*OutFirstSlot = I;
		}
	};
	// Top up stacks of the same item, then fill empty slots.
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		FSaltItemStack& S = Slots[I];
		if (Left > 0 && S.Id == Id && S.Count < Def->MaxStack)
		{
			const int32 N = FMath::Min(Left, Def->MaxStack - S.Count);
			S.Count += N;
			Left -= N;
			Took(I);
		}
	}
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		FSaltItemStack& S = Slots[I];
		if (Left > 0 && S.IsEmpty())
		{
			const int32 N = FMath::Min(Left, Def->MaxStack);
			S = { Id, N };
			Left -= N;
			Took(I);
		}
	}
	const int32 Added = Count - Left;
	if (Added > 0)
	{
		OnChanged.Broadcast();
	}
	return Added;
}

int32 USaltInventoryComponent::Remove(FName Id, int32 Count)
{
	int32 Left = Count;
	for (int32 I = Slots.Num() - 1; I >= 0 && Left > 0; --I)
	{
		FSaltItemStack& S = Slots[I];
		if (S.Id == Id && S.Count > 0)
		{
			const int32 N = FMath::Min(Left, S.Count);
			S.Count -= N;
			Left -= N;
			if (S.Count <= 0)
			{
				S = FSaltItemStack();
			}
		}
	}
	const int32 Removed = Count - Left;
	if (Removed > 0)
	{
		OnChanged.Broadcast();
	}
	return Removed;
}

int32 USaltInventoryComponent::CountOf(FName Id) const
{
	int32 N = 0;
	for (const FSaltItemStack& S : Slots)
	{
		N += S.Id == Id ? S.Count : 0;
	}
	return N;
}

void USaltInventoryComponent::SetSlots(const TArray<FSaltItemStack>& InSlots)
{
	const int32 N = Slots.Num();
	Slots = InSlots;
	Slots.SetNum(N);
	OnChanged.Broadcast();
}

void USaltInventoryComponent::InitStore(FName InStorageId, const FText& InName, int32 InCapacity)
{
	StorageId = InStorageId;
	DisplayName = InName;
	Slots.Reset();
	Slots.SetNum(FMath::Max(InCapacity, 1));
}

int32 USaltInventoryComponent::MoveStack(int32 Index, USaltInventoryComponent* To, int32 MaxCount, int32* OutToSlot)
{
	if (OutToSlot)
	{
		*OutToSlot = INDEX_NONE;
	}
	if (!To || To == this || !Slots.IsValidIndex(Index) || Slots[Index].IsEmpty() || MaxCount <= 0)
	{
		return 0;
	}
	const FSaltItemStack Stack = Slots[Index];
	const int32 Moved = To->Add(Stack.Id, FMath::Min(Stack.Count, MaxCount), OutToSlot);
	if (Moved > 0)
	{
		Slots[Index].Count -= Moved;
		if (Slots[Index].Count <= 0)
		{
			Slots[Index] = FSaltItemStack();
		}
		OnChanged.Broadcast();
	}
	return Moved;
}

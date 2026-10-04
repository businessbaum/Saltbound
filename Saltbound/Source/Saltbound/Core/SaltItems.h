// Saltbound - items the player can carry (D50), defined in code like the input and the UI.

#pragma once

#include "CoreMinimal.h"
#include "SaltItems.generated.h"

/** A stack in a satchel slot (Id None = empty slot). */
USTRUCT(BlueprintType)
struct SALTBOUND_API FSaltItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item") int32 Count = 0;

	bool IsEmpty() const { return Id.IsNone() || Count <= 0; }
};

/** What an item is: its name and description, satchel icon, the mesh it lies about as, and how many stack. */
struct FSaltItemDef
{
	FName Id;
	FText Name;
	FText Description;
	const TCHAR* Icon = nullptr;    // texture object path
	const TCHAR* Mesh = nullptr;    // pickup mesh object path
	int32 MaxStack = 1;
};

namespace SaltItems
{
	SALTBOUND_API const TArray<FSaltItemDef>& All();
	SALTBOUND_API const FSaltItemDef* Find(FName Id);
}

// Saltbound - digging (D73, M6 step 2): places where something lies buried, the ground a shovel goes into, and the shovel
// the Harbour Keeper hands out.

#pragma once

#include "CoreMinimal.h"
#include "Core/SaltItems.h"
#include "GameFramework/Actor.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltDig.generated.h"

class ASaltPlayerCharacter;
class UStaticMeshComponent;

/** How a place shows that something lies buried there. */
enum class ESaltDigSign : uint8
{
	Mound,   // a low mound of loose sand
	Stake,   // a sailor's stake with a rag tied round it
};

/** What comes up. */
enum class ESaltDigFind : uint8
{
	Bundle,  // an oilcloth bundle (the Cartographer's caches)
	Box,     // a small planked box (sailors' caches)
};

/** A place where something lies buried (a table in code, like the places and the stats, D59). */
struct FSaltDigSpotDef
{
	FName Id;
	/** World metres (north +X, east +Y); the ground's height is found by a trace when it is placed. */
	FVector2D LocationM = FVector2D::ZeroVector;
	float YawDeg = 0.f;
	ESaltDigSign Sign = ESaltDigSign::Mound;
	ESaltDigFind Find = ESaltDigFind::Box;
	TArray<FSaltItemStack> Loot;
	/** Salt for the purse (D63), on top of the loot. */
	int32 Salt = 0;
	/** A line shown when it is opened (a note in the cache), or empty. */
	FText Note;
};

namespace SaltDig
{
	SALTBOUND_API const TArray<FSaltDigSpotDef>& Spots();
	SALTBOUND_API const FSaltDigSpotDef* Find(FName Id);
	/** Ground a shovel goes into: the islands' sand and earth (their ground and collision meshes), never rock, wood or a
	 *  deck; not too steep, and above the water. */
	SALTBOUND_API bool IsDiggable(const FHitResult& Hit);

	inline const FName ShovelTool = TEXT("Shovel");
	/** The interaction volumes that dig carry this tag (the held shovel strokes while one is held). */
	inline const FName DigTag = TEXT("Dig");
	/** Holding E this long digs (three strokes). */
	constexpr float HoldSeconds = 2.4f;
	/** How deep a spot's meshes sit under the ground's collision (cm): their rims stay under the sculpted sand. */
	constexpr float SinkCm = 5.f;
	/** Loose ground (not a buried thing) gives one small find per cell of this size (m). */
	constexpr float GroundCellM = 3.f;
	/** The holes dug in loose ground that stay to be seen (the oldest fills in). */
	constexpr int32 GroundHolesKept = 12;
}

/** The E volume of a dig: asks its owner (a dig spot or the loose ground) what it offers. */
UCLASS(ClassGroup = (Saltbound))
class SALTBOUND_API USaltDigUseComponent : public USaltInteractableComponent
{
	GENERATED_BODY()

public:
	USaltDigUseComponent();
	virtual bool CanInteract(const ASaltPlayerCharacter* Player) const override;
	virtual FText GetPromptText(const ASaltPlayerCharacter* Player) const override;
	virtual void Interact(ASaltPlayerCharacter* Player) override;
};

/**
 * A place where something lies buried: its sign (a mound or a stake), the hole and the heap that grow while the player digs
 * (hold E with the shovel in hand: three strokes), and the find that rises out of the hole; E then takes what is in it. The
 * save remembers each spot's stage by its id (USaltSaveSubsystem::DigStages). Spawned by USaltDigSubsystem from the table.
 */
UCLASS()
class SALTBOUND_API ASaltDigSpot : public AActor
{
	GENERATED_BODY()

public:
	ASaltDigSpot();

	void Setup(const FSaltDigSpotDef& Def);
	FName GetSpotId() const { return SpotId; }
	/** 0 buried, 1 dug up (the find lies in the hole), 2 emptied. */
	int32 GetStage() const { return Stage; }
	void SetStage(int32 InStage, bool bInstant);
	USaltDigUseComponent* GetUse() const { return Use; }

	/** What E does here now (the use component asks). */
	bool CanUse(const ASaltPlayerCharacter* Player) const;
	FText GetPrompt(const ASaltPlayerCharacter* Player) const;
	void Use_(ASaltPlayerCharacter* Player);
	/** Digs it up at once (the hold's end; tests). */
	bool DigUp(ASaltPlayerCharacter* Player);
	/** Takes the find's contents into the satchel (and the salt into the purse). False when nothing fitted. */
	bool TakeFind(ASaltPlayerCharacter* Player);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyLook();

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> SignMesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> HoleMesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> HeapMesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> FindMesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltDigUseComponent> Use;

	FName SpotId;
	ESaltDigSign Sign = ESaltDigSign::Mound;
	ESaltDigFind FindKind = ESaltDigFind::Box;
	TArray<FSaltItemStack> Loot;
	int32 Salt = 0;
	FText Note;
	int32 Stage = 0;
	/** How far the hole has come (0..1) and how far the find has risen (0..1); both shown, both eased. */
	float Dug = 0.f;
	float Rise = 0.f;
};

/**
 * Loose ground: while the player has the shovel in hand and looks at diggable ground within reach, an E volume follows the
 * spot looked at ("Dig", hold E). Digging leaves a hole and a heap there and turns up something small (a shell, a nail, a
 * piece of driftwood: never nothing); each cell of ground gives one find (this session).
 */
UCLASS()
class SALTBOUND_API ASaltGroundDig : public AActor
{
	GENERATED_BODY()

public:
	ASaltGroundDig();

	bool CanUse(const ASaltPlayerCharacter* Player) const;
	FText GetPrompt(const ASaltPlayerCharacter* Player) const;
	void Use_(ASaltPlayerCharacter* Player);
	/** Digs at a world point at once (tests); returns the item found (None if nothing could be dug there). */
	FName DigAt(ASaltPlayerCharacter* Player, const FVector& Point);
	USaltDigUseComponent* GetUse() const { return Use; }
	bool IsCellDug(const FVector& Point) const;

	virtual void Tick(float DeltaSeconds) override;

private:
	FIntPoint CellOf(const FVector& Point) const;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USaltDigUseComponent> Use;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Holes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Heaps;
	TSet<FIntPoint> DugCells;
	bool bOnGround = false;
	int32 NextHole = 0;
};

/**
 * Places the dig spots of the table in the game's map (with the ground's height by a trace), keeps the loose-ground digger,
 * and lets the Harbour Keeper hand out the shovel (the island's ASaltHubPoint with PointId "keeper", D71) until the people
 * come (M6 step 5).
 */
UCLASS()
class SALTBOUND_API USaltDigSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	ASaltDigSpot* FindSpot(FName Id) const;
	ASaltGroundDig* GetGroundDig() const { return GroundDig; }
	/** The keeper gives the shovel (once; a second time he says the player has it). */
	UFUNCTION() void HandleKeeper(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);

private:
	void PlaceSpots();
	void TryBindKeeper();
	void ApplySavedStages();

	UPROPERTY(Transient) TArray<TObjectPtr<ASaltDigSpot>> Spots;
	UPROPERTY(Transient) TObjectPtr<ASaltGroundDig> GroundDig;
	bool bKeeperBound = false;
	float KeeperRetry = 0.f;
	int32 KeeperTries = 0;
	FDelegateHandle LoadedHandle;
};

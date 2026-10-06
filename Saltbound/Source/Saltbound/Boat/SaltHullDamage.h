// Saltbound - the ship repair system (D74, M6 step 3): holes from hard knocks, the sea coming in, bailing, patching, a sail
// torn by too much wind, and the Shipwright's full repair.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/SaltInteractableComponent.h"
#include "SaltHullDamage.generated.h"

class ASaltBoat;
class ASaltPlayerCharacter;
class UStaticMeshComponent;

/** A hole in the boat's side (body metres along her; the side; how big), saved with the game. */
USTRUCT()
struct SALTBOUND_API FSaltHullHole
{
	GENERATED_BODY()

	UPROPERTY() float X = 0.f;
	/** +1 starboard, -1 port. */
	UPROPERTY() int32 Side = 1;
	/** 1 small, 2 medium, 3 large. */
	UPROPERTY() int32 Size = 1;
	UPROPERTY() bool bPatched = false;
};

/** What a repair volume on the boat is for. */
enum class ESaltRepairUse : uint8
{
	Hole,    // patch it (hold E with a plank and nails)
	Bail,    // the bucket (hold E: one bucketful)
	Mend,    // the sail (hold E with sailcloth)
};

/** An E volume of the repair system: asks the boat's hull damage what it offers. */
UCLASS(ClassGroup = (Saltbound))
class SALTBOUND_API USaltRepairUseComponent : public USaltInteractableComponent
{
	GENERATED_BODY()

public:
	ESaltRepairUse Use = ESaltRepairUse::Hole;
	int32 HoleIndex = INDEX_NONE;

	virtual bool CanInteract(const ASaltPlayerCharacter* Player) const override;
	virtual FText GetPromptText(const ASaltPlayerCharacter* Player) const override;
	virtual void Interact(ASaltPlayerCharacter* Player) override;
};

/**
 * The boat's wear and tear (on ASaltBoat). A knock faster than HoleSpeed against rock, a reef, a pier or a quay springs her
 * planking where she was hit: a hole of three sizes on the bulwark's inner face, the sea spurting in. Every open hole lets in
 * water (LeakLitresPerSecond); the water in her bilge weighs her down (ASaltBoat::BilgeWaterKg) and slows her, rises over the
 * deck past half and at FounderKg she founders. The player patches holes (hold E with a piece of driftwood and two nails; the
 * patch stays to be seen), bails with the bucket (hold E: a bucketful) and mends the sail with sailcloth. Too much wind on a
 * full sail tears it (three steps: a tear, a rip, shreds); shreds and everything else need the Shipwright (RepairAll).
 * No bars: the boat shows her state (the holes, the water, the sail), the pause card says it in words.
 */
UCLASS(ClassGroup = (Saltbound))
class SALTBOUND_API USaltHullDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USaltHullDamageComponent();

	/** A knock this fast (m/s, closing) springs a plank (Stats: the boat's table). */
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "m/s")) float HoleSpeed = 2.f;
	/** Faster than these it is a medium / a large hole. */
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "m/s")) float MediumSpeed = 3.2f;
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "m/s")) float LargeSpeed = 4.5f;
	/** Water in through an open small, medium, large hole (litres a second). */
	UPROPERTY(EditAnywhere, Category = "Repair") FVector LeakLitresPerSecond = FVector(2.0, 6.0, 15.0);
	/** She founders with this much water aboard (kg = litres). */
	UPROPERTY(EditAnywhere, Category = "Repair") float FounderKg = 4000.f;
	/** The water over the deck from this share of FounderKg on. */
	UPROPERTY(EditAnywhere, Category = "Repair") float DeckWaterFrom = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Repair") float BucketLitres = 14.f;
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "s")) float BailSeconds = 1.f;
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "s")) float PatchSeconds = 2.f;
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "s")) float MendSeconds = 3.f;
	/** Wind this strong (m/s, true) on a sail set more than SailTearShare tears it after TearSeconds. */
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "m/s")) float TearWind = 12.f;
	UPROPERTY(EditAnywhere, Category = "Repair") float SailTearShare = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Repair", meta = (Units = "s")) float TearSeconds = 8.f;
	UPROPERTY(EditAnywhere, Category = "Repair") int32 MaxHoles = 8;

	const TArray<FSaltHullHole>& GetHoles() const { return Holes; }
	int32 CountOpenHoles() const;
	float GetWaterKg() const { return WaterKg; }
	/** 0 dry .. 1 foundering. */
	float GetFill() const { return FounderKg > 0.f ? FMath::Clamp(WaterKg / FounderKg, 0.f, 1.f) : 0.f; }
	/** 0 whole, 1 a tear, 2 a rip, 3 in shreds. */
	int32 GetSailTear() const { return SailTear; }
	/** Litres a second coming in now. */
	float GetLeakRate() const;
	/** The sail's drive left (the tear) and the water's drag, for the boat. */
	float GetDriveFactor() const;

	/** A knock (ASaltBoat::OnHullKnock): body metres, closing speed (m/s). */
	void Knock(const FVector& BodyPointM, float ClosingSpeed);
	/** Springs a plank at X on Side (tests and the knocks). False when she has MaxHoles. */
	bool AddHole(float X, int32 Side, int32 Size);
	/** Patches hole Index with the player's driftwood and nails (bFree: no materials, tests). */
	bool PatchHole(int32 Index, ASaltPlayerCharacter* Player, bool bFree = false);
	void Bail(float Litres);
	bool MendSail(ASaltPlayerCharacter* Player, bool bFree = false);
	void TearSail();
	/** The Shipwright's work: no holes or patches, dry, a whole sail. */
	void RepairAll();
	/** From a save. */
	void RestoreState(const TArray<FSaltHullHole>& InHoles, float InWaterKg, int32 InSailTear);

	/** The repair volumes ask these. */
	bool CanUse(const USaltRepairUseComponent* Use, const ASaltPlayerCharacter* Player) const;
	FText GetPrompt(const USaltRepairUseComponent* Use, const ASaltPlayerCharacter* Player) const;
	void UseBy(USaltRepairUseComponent* Use, ASaltPlayerCharacter* Player);

	/** Where a hole sits on the bulwark's inner face (cm, the boat's frame) and how it faces. */
	static FTransform HoleTransform(float X, int32 Side);
	/** False where a fitting (a pin rail, a cask, the bucket) stands in front of the bulwark at X on Side within HalfWidth (m):
	 *  a hole there would be hidden behind it and its boards would cut through it. Protrusion (cm): how far the furthest-out
	 *  thing stands in front of the face (0 = nothing). */
	bool IsHoleSpotFree(float X, int32 Side, float HalfWidth, float* Protrusion = nullptr) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsureParts();
	void RebuildHoles();
	void PushToBoat();
	void Say(const FText& Text) const;

	TWeakObjectPtr<ASaltBoat> Boat;
	FDelegateHandle KnockHandle;
	TArray<FSaltHullHole> Holes;
	float WaterKg = 0.f;
	int32 SailTear = 0;
	float TearTimer = 0.f;
	float KnockCooldown = 0.f;
	/** The leak's notice is said once while she takes water. */
	bool bSaidTakingWater = false;
	bool bSaidLow = false;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HoleMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> SpurtMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<USaltRepairUseComponent>> HoleUses;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Bucket;
	UPROPERTY(Transient) TObjectPtr<USaltRepairUseComponent> BucketUse;
	UPROPERTY(Transient) TObjectPtr<USaltRepairUseComponent> MendUse;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> DeckWater;
};

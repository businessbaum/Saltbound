// Saltbound - digging (D73, M6 step 2).

#include "World/SaltDig.h"
#include "World/SaltPrologue.h"
#include "World/SaltTalk.h"

#include "Components/StaticMeshComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Hub/SaltHubPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

namespace
{
	FString MeshPath(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/Saltbound/Props/Dig/%s.%s"), Name, Name);
	}

	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* M = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		M->SetupAttachment(Parent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		return M;
	}

	FSaltItemStack Stack(const TCHAR* Id, int32 Count)
	{
		FSaltItemStack S;
		S.Id = Id;
		S.Count = Count;
		return S;
	}

	ASaltPlayerController* ControllerOf(ASaltPlayerCharacter* Player)
	{
		return Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr;
	}

	bool HasShovelInHand(const ASaltPlayerCharacter* Player)
	{
		return Player && Player->GetHeldTool() == SaltDig::ShovelTool;
	}

	bool OwnsShovel(const ASaltPlayerCharacter* Player)
	{
		return Player && Player->IsToolOwned(SaltDig::ShovelTool);
	}
}

// --- The table -----------------------------------------------------------------------------------------------------

const TArray<FSaltDigSpotDef>& SaltDig::Spots()
{
	// World metres (north +X, east +Y). The home island's places come from its shape (island_shape.py): sand 0.35-0.45 m
	// above the water on the lagoon's beaches, the islet's cay, the west hill's top beside the path's end.
	auto Spot = [](const TCHAR* Id, double X, double Y, float Yaw, ESaltDigSign Sign, ESaltDigFind Find, TArray<FSaltItemStack> Loot,
		int32 Salt, FText Note = FText::GetEmpty())
	{
		FSaltDigSpotDef D;
		D.Id = Id;
		D.LocationM = FVector2D(X, Y);
		D.YawDeg = Yaw;
		D.Sign = Sign;
		D.Find = Find;
		D.Loot = MoveTemp(Loot);
		D.Salt = Salt;
		D.Note = Note;
		return D;
	};
	static const TArray<FSaltDigSpotDef> All = {
		// The prologue's cache (the concept's 8.1): two paces seaward of the palm by the pier's beach; the log's first page
		// will point here.
		Spot(TEXT("CartographerCanvas"), -18.4, 15.9, 30.f, ESaltDigSign::Mound, ESaltDigFind::Bundle,
			{ Stack(TEXT("Canvas"), 2), Stack(TEXT("PaleFloat"), 1) }, 0,
			NSLOCTEXT("SaltDig", "NoteCanvas", "Wrapped in the canvas, a scrap of paper: \"For the sail. Never trust a store.\"")),
		Spot(TEXT("IsletCay"), 150.0, -35.0, 110.f, ESaltDigSign::Stake, ESaltDigFind::Box,
			{ Stack(TEXT("Tar"), 1), Stack(TEXT("IronNails"), 6) }, 25),
		Spot(TEXT("LagoonEast"), 17.2, 82.8, 200.f, ESaltDigSign::Mound, ESaltDigFind::Box,
			{ Stack(TEXT("RopeCoil"), 1), Stack(TEXT("IronNails"), 4) }, 0),
		Spot(TEXT("LagoonNorth"), 209.6, 46.7, 15.f, ESaltDigSign::Mound, ESaltDigFind::Box,
			{ Stack(TEXT("Canvas"), 1), Stack(TEXT("Tar"), 1) }, 15),
		Spot(TEXT("LagoonWest"), 56.0, -102.1, 290.f, ESaltDigSign::Stake, ESaltDigFind::Box,
			{ Stack(TEXT("IronNails"), 8) }, 30),
		Spot(TEXT("LagoonNorthEast"), 174.2, 84.2, 75.f, ESaltDigSign::Mound, ESaltDigFind::Bundle,
			{ Stack(TEXT("GlassFloat"), 1), Stack(TEXT("Seashell"), 3) }, 20),
		Spot(TEXT("HillTop"), 95.5, -142.0, 160.f, ESaltDigSign::Stake, ESaltDigFind::Box,
			{ Stack(TEXT("Canvas"), 1), Stack(TEXT("RopeCoil"), 1) }, 60,
			NSLOCTEXT("SaltDig", "NoteHill", "Scratched inside the lid: a ring of seven marks, one of them crossed out.")),
	};
	return All;
}

const FSaltDigSpotDef* SaltDig::Find(FName Id)
{
	return Spots().FindByPredicate([Id](const FSaltDigSpotDef& D) { return D.Id == Id; });
}

bool SaltDig::IsDiggable(const FHitResult& Hit)
{
	const UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Hit.GetComponent());
	const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	if (!Mesh || Hit.ImpactNormal.Z < 0.8f || Hit.ImpactPoint.Z < 20.f)
	{
		return false;
	}
	// The islands' ground pieces (visible and their hidden collision); rocks, plants, timber and decks are other meshes.
	const FString Name = Mesh->GetName();
	static const TCHAR* Prefixes[] = { TEXT("SM_Home_"), TEXT("SM_Island_Ruin") };
	for (const TCHAR* P : Prefixes)
	{
		if (Name.StartsWith(P))
		{
			return true;
		}
	}
	return false;
}

// --- The E volume ----------------------------------------------------------------------------------------------------

USaltDigUseComponent::USaltDigUseComponent()
{
	ComponentTags.Add(SaltDig::DigTag);
	// Constructor-safe (SetBoxExtent makes a body setup with NewObject, which a class default object may not do).
	InitBoxExtent(FVector(45.f, 45.f, 30.f));
}

bool USaltDigUseComponent::CanInteract(const ASaltPlayerCharacter* Player) const
{
	if (!Super::CanInteract(Player))
	{
		return false;
	}
	if (const ASaltDigSpot* Spot = Cast<ASaltDigSpot>(GetOwner()))
	{
		return Spot->CanUse(Player);
	}
	if (const ASaltGroundDig* Ground = Cast<ASaltGroundDig>(GetOwner()))
	{
		return Ground->CanUse(Player);
	}
	return false;
}

FText USaltDigUseComponent::GetPromptText(const ASaltPlayerCharacter* Player) const
{
	if (const ASaltDigSpot* Spot = Cast<ASaltDigSpot>(GetOwner()))
	{
		return Spot->GetPrompt(Player);
	}
	if (const ASaltGroundDig* Ground = Cast<ASaltGroundDig>(GetOwner()))
	{
		return Ground->GetPrompt(Player);
	}
	return Super::GetPromptText(Player);
}

void USaltDigUseComponent::Interact(ASaltPlayerCharacter* Player)
{
	if (ASaltDigSpot* Spot = Cast<ASaltDigSpot>(GetOwner()))
	{
		Spot->Use_(Player);
	}
	else if (ASaltGroundDig* Ground = Cast<ASaltGroundDig>(GetOwner()))
	{
		Ground->Use_(Player);
	}
	Super::Interact(Player);
}

// --- A dig spot ------------------------------------------------------------------------------------------------------

ASaltDigSpot::ASaltDigSpot()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SignMesh = MakeMesh(this, Root, TEXT("Sign"));
	HoleMesh = MakeMesh(this, Root, TEXT("Hole"));
	HeapMesh = MakeMesh(this, Root, TEXT("Heap"));
	FindMesh = MakeMesh(this, Root, TEXT("Find"));
	// The heap lies beside the hole, on the side away from where the spot faces.
	HeapMesh->SetRelativeLocation(FVector(-62.f, 18.f, 0.f));
	HeapMesh->SetRelativeRotation(FRotator(0.f, 25.f, 0.f));
	Use = CreateDefaultSubobject<USaltDigUseComponent>(TEXT("Use"));
	Use->SetupAttachment(Root);
	Use->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
	Use->PromptOffset = FVector(0.f, 0.f, 20.f);
}

void ASaltDigSpot::Setup(const FSaltDigSpotDef& Def)
{
	SpotId = Def.Id;
	Sign = Def.Sign;
	FindKind = Def.Find;
	Loot = Def.Loot;
	Salt = Def.Salt;
	Note = Def.Note;
	// Content is resolved here, never in the constructor (Core/SaltContent.h).
	SignMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath(Sign == ESaltDigSign::Stake ? TEXT("SM_Dig_Stake") : TEXT("SM_Dig_Mound"))));
	HoleMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath(TEXT("SM_Dig_Hole"))));
	HeapMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath(TEXT("SM_Dig_Heap"))));
	FindMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath(FindKind == ESaltDigFind::Bundle ? TEXT("SM_Dig_Bundle") : TEXT("SM_Dig_Box"))));
	if (Sign == ESaltDigSign::Stake)
	{
		// The stake stands at the edge of where to dig; a low mound still shows the spot.
		SignMesh->SetRelativeLocation(FVector(48.f, -20.f, 0.f));
	}
	ApplyLook();
}

void ASaltDigSpot::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

void ASaltDigSpot::SetStage(int32 InStage, bool bInstant)
{
	Stage = FMath::Clamp(InStage, 0, 2);
	if (bInstant)
	{
		Dug = Stage > 0 ? 1.f : 0.f;
		Rise = Stage == 1 ? 1.f : 0.f;
	}
	ApplyLook();
}

void ASaltDigSpot::ApplyLook()
{
	// The sign shows until the digging has opened the ground; the hole and the heap grow with the digging; the find rises
	// from under the ground (the ground hides it below its surface) and settles half out of the hole.
	const bool bMound = Sign == ESaltDigSign::Mound;
	SignMesh->SetVisibility(Stage == 0 && (bMound ? Dug < 0.5f : true));
	if (!bMound)
	{
		SignMesh->SetVisibility(true);
	}
	const float D = FMath::Clamp(Dug, 0.f, 1.f);
	HoleMesh->SetVisibility(D > 0.01f);
	HoleMesh->SetRelativeScale3D(FVector(0.55f + 0.45f * D, 0.55f + 0.45f * D, FMath::Max(D, 0.05f)));
	HeapMesh->SetVisibility(D > 0.01f);
	HeapMesh->SetRelativeScale3D(FVector(0.35f + 0.65f * D));
	FindMesh->SetVisibility(Stage == 1);
	const float Up = 1.f - FMath::Square(1.f - FMath::Clamp(Rise, 0.f, 1.f));
	FindMesh->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(-55.f, -6.f, Up)));
	FindMesh->SetRelativeRotation(FRotator(FMath::Lerp(-12.f, 4.f, Up), 20.f, FMath::Lerp(8.f, -3.f, Up)));
	Use->HoldTime = 0.f;
	if (Stage == 0)
	{
		Use->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
	}
}

bool ASaltDigSpot::CanUse(const ASaltPlayerCharacter* Player) const
{
	return Stage < 2;
}

FText ASaltDigSpot::GetPrompt(const ASaltPlayerCharacter* Player) const
{
	if (Stage == 1)
	{
		return FindKind == ESaltDigFind::Bundle ? NSLOCTEXT("SaltDig", "OpenBundle", "Untie the bundle")
			: NSLOCTEXT("SaltDig", "OpenBox", "Open the box");
	}
	if (!OwnsShovel(Player))
	{
		return NSLOCTEXT("SaltDig", "Buried", "Something is buried here");
	}
	return HasShovelInHand(Player) ? NSLOCTEXT("SaltDig", "DigHere", "Dig") : NSLOCTEXT("SaltDig", "TakeShovel", "Take the shovel");
}

void ASaltDigSpot::Use_(ASaltPlayerCharacter* Player)
{
	if (Stage == 1)
	{
		TakeFind(Player);
		return;
	}
	if (!OwnsShovel(Player))
	{
		if (ASaltPlayerController* PC = ControllerOf(Player))
		{
			PC->ShowNotice(NSLOCTEXT("SaltDig", "NeedShovel", "You need a shovel to dig it up"));
		}
		return;
	}
	if (!HasShovelInHand(Player))
	{
		Player->SetHeldTool(SaltDig::ShovelTool);
		return;
	}
	DigUp(Player);
}

bool ASaltDigSpot::DigUp(ASaltPlayerCharacter* Player)
{
	if (Stage != 0)
	{
		return false;
	}
	Stage = 1;
	Dug = FMath::Max(Dug, 1.f);
	Rise = 0.f;
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		Saves->SetDigStage(SpotId, Stage);
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_DIG spot=%s dug"), *SpotId.ToString());
	ApplyLook();
	return true;
}

bool ASaltDigSpot::TakeFind(ASaltPlayerCharacter* Player)
{
	USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	if (Stage != 1 || !Satchel)
	{
		return false;
	}
	ASaltPlayerController* PC = ControllerOf(Player);
	bool bAny = false;
	for (FSaltItemStack& S : Loot)
	{
		if (S.IsEmpty())
		{
			continue;
		}
		const int32 Added = Satchel->Add(S.Id, S.Count);
		if (Added > 0)
		{
			bAny = true;
			if (PC)
			{
				if (const FSaltItemDef* Def = SaltItems::Find(S.Id))
				{
					PC->ShowNotice(FText::Format(NSLOCTEXT("Saltbound", "PickedUp", "+{0}  {1}"), FText::AsNumber(Added), Def->Name));
				}
			}
		}
		S.Count -= Added;
	}
	Loot.RemoveAll([](const FSaltItemStack& S) { return S.IsEmpty(); });
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (Salt > 0 && Saves)
	{
		Saves->SetSalt(Saves->GetSalt() + Salt);
		if (PC)
		{
			PC->ShowNotice(FText::Format(NSLOCTEXT("SaltDig", "Salt", "+{0}  salt"), FText::AsNumber(Salt)));
		}
		Salt = 0;
		bAny = true;
	}
	if (Loot.Num() > 0)
	{
		// Some did not fit: it stays in the hole until there is room.
		if (PC)
		{
			PC->ShowNotice(NSLOCTEXT("Saltbound", "SatchelFull", "Your satchel is full"));
		}
		return bAny;
	}
	if (!Note.IsEmpty() && PC)
	{
		PC->ShowNotice(Note);
	}
	Stage = 2;
	if (Saves)
	{
		Saves->SetDigStage(SpotId, Stage);
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_DIG spot=%s emptied"), *SpotId.ToString());
	ApplyLook();
	return true;
}

void ASaltDigSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// While the player holds E here with the shovel the hole deepens with the strokes (and stays as deep as it got).
	float WantDug = Stage > 0 ? 1.f : Dug;
	if (Stage == 0)
	{
		const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		const ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
		if (Player && Player->GetHoldTarget() == Use && HasShovelInHand(Player))
		{
			WantDug = FMath::Max(Dug, Player->GetHoldProgress() * 0.9f);
		}
		// The use volume becomes a hold only with the shovel in hand (else a press takes it, or says what is missing).
		Use->HoldTime = HasShovelInHand(Player) ? SaltDig::HoldSeconds : 0.f;
	}
	const float NewDug = FMath::FInterpTo(Dug, WantDug, DeltaSeconds, 10.f);
	const float NewRise = Stage == 1 ? FMath::Min(1.f, Rise + DeltaSeconds / 0.8f) : Rise;
	if (!FMath::IsNearlyEqual(NewDug, Dug, 0.0005f) || !FMath::IsNearlyEqual(NewRise, Rise, 0.0005f))
	{
		Dug = NewDug;
		Rise = NewRise;
		ApplyLook();
	}
}

// --- Loose ground ----------------------------------------------------------------------------------------------------

ASaltGroundDig::ASaltGroundDig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Use = CreateDefaultSubobject<USaltDigUseComponent>(TEXT("Use"));
	Use->SetupAttachment(Root);
	Use->InitBoxExtent(FVector(25.f, 25.f, 18.f));
	Use->SetUsingAbsoluteRotation(true);
	Use->HoldTime = SaltDig::HoldSeconds;
	Use->PromptOffset = FVector(0.f, 0.f, 12.f);
	// Off until it lies on ground the player looks at.
	Use->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FIntPoint ASaltGroundDig::CellOf(const FVector& Point) const
{
	const double Cell = SaltDig::GroundCellM * 100.0;
	return FIntPoint(FMath::FloorToInt32(Point.X / Cell), FMath::FloorToInt32(Point.Y / Cell));
}

bool ASaltGroundDig::IsCellDug(const FVector& Point) const
{
	return DugCells.Contains(CellOf(Point));
}

bool ASaltGroundDig::CanUse(const ASaltPlayerCharacter* Player) const
{
	return bOnGround && HasShovelInHand(Player) && !IsCellDug(Use->GetComponentLocation());
}

FText ASaltGroundDig::GetPrompt(const ASaltPlayerCharacter* Player) const
{
	return NSLOCTEXT("SaltDig", "DigGround", "Dig");
}

void ASaltGroundDig::Use_(ASaltPlayerCharacter* Player)
{
	DigAt(Player, Use->GetComponentLocation() - FVector(0.f, 0.f, Use->GetUnscaledBoxExtent().Z));
}

FName ASaltGroundDig::DigAt(ASaltPlayerCharacter* Player, const FVector& Point)
{
	if (IsCellDug(Point))
	{
		return NAME_None;
	}
	DugCells.Add(CellOf(Point));
	// The hole and the heap stay to be seen (a ring of them; the oldest fills in).
	if (Holes.Num() < SaltDig::GroundHolesKept)
	{
		UStaticMeshComponent* Hole = NewObject<UStaticMeshComponent>(this);
		UStaticMeshComponent* Heap = NewObject<UStaticMeshComponent>(this);
		for (UStaticMeshComponent* M : { Hole, Heap })
		{
			M->SetupAttachment(Root);
			M->SetUsingAbsoluteLocation(true);
			M->SetUsingAbsoluteRotation(true);
			M->SetUsingAbsoluteScale(true);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetCanEverAffectNavigation(false);
			M->RegisterComponent();
		}
		Hole->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath(TEXT("SM_Dig_Hole"))));
		Heap->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath(TEXT("SM_Dig_Heap"))));
		Holes.Add(Hole);
		Heaps.Add(Heap);
	}
	const int32 Slot = NextHole % Holes.Num();
	NextHole++;
	const float Yaw = FMath::FRandRange(0.f, 360.f);
	// Laid with the ground's slope and sunk a little, as the spots are (SaltDig::SinkCm).
	FVector Normal = FVector::UpVector;
	{
		FHitResult G;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(SaltGroundDigSlope), true, Player);
		if (GetWorld()->LineTraceSingleByChannel(G, Point + FVector(0.0, 0.0, 40.0), Point - FVector(0.0, 0.0, 60.0), ECC_Visibility, Q)
			&& SaltDig::IsDiggable(G))
		{
			Normal = G.ImpactNormal;
		}
	}
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Normal);
	const FVector At = Point - Normal * (SaltDig::SinkCm * 0.7f);
	Holes[Slot]->SetWorldLocationAndRotation(At, Tilt * FQuat(FRotator(0.f, Yaw, 0.f)));
	Holes[Slot]->SetWorldScale3D(FVector(0.7f, 0.7f, 0.7f));
	Heaps[Slot]->SetWorldLocationAndRotation(At + Tilt.RotateVector(FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-48.f, 0.f, 0.f))),
		Tilt * FQuat(FRotator(0.f, Yaw + 30.f, 0.f)));
	Heaps[Slot]->SetWorldScale3D(FVector(0.65f));
	// Something small, never nothing: mostly shells, now and then a nail, a bit of driftwood, a coil of rope.
	struct FFind { const TCHAR* Id; int32 Weight; int32 Count; };
	static const FFind Finds[] = { { TEXT("Seashell"), 45, 1 }, { TEXT("IronNails"), 20, 2 }, { TEXT("Driftwood"), 25, 1 }, { TEXT("RopeCoil"), 10, 1 } };
	int32 Total = 0;
	for (const FFind& F : Finds)
	{
		Total += F.Weight;
	}
	int32 Roll = FMath::RandRange(0, Total - 1);
	const FFind* Pick = &Finds[0];
	for (const FFind& F : Finds)
	{
		if (Roll < F.Weight)
		{
			Pick = &F;
			break;
		}
		Roll -= F.Weight;
	}
	USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	const int32 Added = Satchel ? Satchel->Add(Pick->Id, Pick->Count) : 0;
	if (ASaltPlayerController* PC = ControllerOf(Player))
	{
		const FSaltItemDef* Def = SaltItems::Find(Pick->Id);
		PC->ShowNotice(Added > 0 && Def ? FText::Format(NSLOCTEXT("Saltbound", "PickedUp", "+{0}  {1}"), FText::AsNumber(Added), Def->Name)
			: NSLOCTEXT("Saltbound", "SatchelFull", "Your satchel is full"));
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_DIG ground at=(%.0f, %.0f) found=%s added=%d"), Point.X, Point.Y, Pick->Id, Added);
	return Pick->Id;
}

void ASaltGroundDig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return;
	}
	// While the hold runs, the spot stays where it is.
	if (Player->GetHoldTarget() == Use && Player->GetHoldProgress() > 0.f)
	{
		return;
	}
	bool bFound = false;
	if (HasShovelInHand(Player))
	{
		FVector Eye;
		FRotator View;
		PC->GetPlayerViewPoint(Eye, View);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltGroundDig), true, Player);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 260.f, ECC_Visibility, Params) && SaltDig::IsDiggable(Hit)
			&& !IsCellDug(Hit.ImpactPoint))
		{
			// The volume sits on the ground where the eye looks (the player's own probe then finds it there).
			Use->SetWorldLocation(Hit.ImpactPoint + FVector(0.f, 0.f, Use->GetUnscaledBoxExtent().Z));
			bFound = true;
		}
	}
	if (bFound != bOnGround)
	{
		bOnGround = bFound;
		Use->SetCollisionEnabled(bFound ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

// --- The subsystem ---------------------------------------------------------------------------------------------------

bool USaltDigSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USaltDigSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltDigSubsystem, STATGROUP_Tickables);
}

void USaltDigSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The table's places are on the home island and Ruin Isle: only the game's map has them.
	if (!InWorld.GetMapName().EndsWith(TEXT("L_VisualTest")))
	{
		return;
	}
	PlaceSpots();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GroundDig = InWorld.SpawnActor<ASaltGroundDig>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(&InWorld))
	{
		LoadedHandle = Saves->OnGameLoaded.AddUObject(this, &USaltDigSubsystem::ApplySavedStages);
	}
	ApplySavedStages();
	TryBindKeeper();
}

void USaltDigSubsystem::PlaceSpots()
{
	UWorld* World = GetWorld();
	for (const FSaltDigSpotDef& Def : SaltDig::Spots())
	{
		const FVector XY(Def.LocationM.X * 100.0, Def.LocationM.Y * 100.0, 0.0);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltDigPlace), true);
		// Down onto the ground (the islands' hidden collision meshes): the first diggable hit from above.
		bool bGround = false;
		FVector Start = XY + FVector(0.0, 0.0, 6000.0);
		for (int32 Tries = 0; Tries < 6 && World->LineTraceSingleByChannel(Hit, Start, XY - FVector(0.0, 0.0, 2000.0), ECC_Visibility, Params); ++Tries)
		{
			if (SaltDig::IsDiggable(Hit))
			{
				bGround = true;
				break;
			}
			Params.AddIgnoredComponent(Hit.GetComponent());
		}
		if (!bGround)
		{
			UE_LOG(LogSaltbound, Warning, TEXT("SB_DIG spot=%s at (%.1f, %.1f) m has no diggable ground under it; not placed"), *Def.Id.ToString(),
				Def.LocationM.X, Def.LocationM.Y);
			continue;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Laid with the ground's slope and sunk a little: set level on the collision's point, the mound's rim stood off the
		// sculpted sand (its ripples, D37) with a shadow under it (seen in play).
		const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Hit.ImpactNormal);
		const FRotator Rot = (Tilt * FQuat(FRotator(0.f, Def.YawDeg, 0.f))).Rotator();
		ASaltDigSpot* Spot = World->SpawnActor<ASaltDigSpot>(Hit.ImpactPoint - Hit.ImpactNormal * SaltDig::SinkCm, Rot, P);
		if (Spot)
		{
			Spot->Setup(Def);
			Spots.Add(Spot);
		}
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_DIG placed=%d of %d"), Spots.Num(), SaltDig::Spots().Num());
}

void USaltDigSubsystem::ApplySavedStages()
{
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
	for (ASaltDigSpot* Spot : Spots)
	{
		if (Spot)
		{
			Spot->SetStage(Saves ? Saves->GetDigStage(Spot->GetSpotId()) : 0, true);
		}
	}
}

ASaltDigSpot* USaltDigSubsystem::FindSpot(FName Id) const
{
	for (ASaltDigSpot* Spot : Spots)
	{
		if (Spot && Spot->GetSpotId() == Id)
		{
			return Spot;
		}
	}
	return nullptr;
}

void USaltDigSubsystem::TryBindKeeper()
{
	if (bKeeperBound)
	{
		return;
	}
	for (TActorIterator<ASaltHubPoint> It(GetWorld()); It; ++It)
	{
		if (It->PointId == TEXT("keeper") && It->GetUse())
		{
			It->GetUse()->OnInteract.AddDynamic(this, &USaltDigSubsystem::HandleKeeper);
			bKeeperBound = true;
			UE_LOG(LogSaltbound, Display, TEXT("SB_DIG keeper=%s (the shovel is his to give)"), *It->GetName());
			return;
		}
	}
}

void USaltDigSubsystem::Tick(float DeltaTime)
{
	// The keeper's place is the island's (D71); look for it a while after the start (levels may still be coming in).
	if (!bKeeperBound && KeeperTries < 30 && GetWorld() && GetWorld()->GetMapName().EndsWith(TEXT("L_VisualTest")))
	{
		KeeperRetry += DeltaTime;
		if (KeeperRetry > 2.f)
		{
			KeeperRetry = 0.f;
			++KeeperTries;
			TryBindKeeper();
		}
	}
}

void USaltDigSubsystem::HandleKeeper(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	ASaltPlayerController* PC = ControllerOf(Player);
	if (!Player)
	{
		return;
	}
	// FH2 (D81): the Keeper speaks (World/SaltTalk.h, Content/Saltbound/Story/Lines.json); the shovel and the lantern are his first
	// talk's gift. Without his lines (a broken file) he hands them over as before.
	if (USaltTalkSubsystem* Talk = GetWorld()->GetSubsystem<USaltTalkSubsystem>(); Talk && Talk->Begin(TEXT("Keeper"), Player))
	{
		return;
	}
	if (Player->IsToolOwned(SaltDig::ShovelTool))
	{
		if (PC)
		{
			PC->ShowNotice(NSLOCTEXT("SaltDig", "KeeperAgain", "\"You have my shovel already. Bring it back with something in it.\""));
		}
		return;
	}
	Player->SetToolOwned(SaltDig::ShovelTool, true);
	// FH1 (D77): and a lantern: the castaway has nothing, and the nights are long.
	const bool bLantern = !Player->IsToolOwned(TEXT("Lantern"));
	Player->SetToolOwned(TEXT("Lantern"), true);
	if (USaltPrologueSubsystem* Prologue = GetWorld()->GetSubsystem<USaltPrologueSubsystem>())
	{
		Prologue->Reach(SaltStory::Keeper);
	}
	if (PC)
	{
		PC->ShowNotice(bLantern
			? NSLOCTEXT("SaltDig", "KeeperShovelLantern", "\"Take my shovel, and a lantern; the nights are long here. The Cartographer never trusted a store: half of them is buried.\"")
			: NSLOCTEXT("SaltDig", "KeeperShovel", "\"Take my shovel. The Cartographer never trusted a store; half of them is buried.\""));
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_DIG shovel given by the keeper"));
}

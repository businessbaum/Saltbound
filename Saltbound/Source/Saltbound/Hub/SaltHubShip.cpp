// Saltbound - the hub ship in the world (D62; see SaltHubShip.h).

#include "Hub/SaltHubShip.h"

#include "Boat/SaltBoat.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Hub/SaltHubBerth.h"
#include "Hub/SaltHubDoors.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "Ocean/SaltOcean.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"

ASaltHubShip::ASaltHubShip()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ASaltHubShip* ASaltHubShip::Find(const UWorld* World)
{
	for (TActorIterator<ASaltHubShip> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

TArray<ASaltHubBerth*> ASaltHubShip::GetBerths() const
{
	TArray<ASaltHubBerth*> Out;
	for (TActorIterator<ASaltHubBerth> It(GetWorld()); It; ++It)
	{
		Out.Add(*It);
	}
	return Out;
}

ASaltHubBerth* ASaltHubShip::FindBerth(FName Id) const
{
	if (Id.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<ASaltHubBerth> It(GetWorld()); It; ++It)
	{
		if (It->BerthId == Id)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ASaltHubShip::IsLoaded() const
{
	return Instance && Instance->IsLevelLoaded() && Instance->IsLevelVisible();
}

void ASaltHubShip::BeginPlay()
{
	Super::BeginPlay();
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		LoadedHandle = Saves->OnGameLoaded.AddUObject(this, &ASaltHubShip::HandleGameLoaded);
	}
	// First look on the next tick: the player and a loaded game are there by then.
	Accumulated = CheckInterval;
}

void ASaltHubShip::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		Saves->OnGameLoaded.Remove(LoadedHandle);
	}
	Unload();
	Super::EndPlay(EndPlayReason);
}

void ASaltHubShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Accumulated += DeltaSeconds;
	if (Accumulated >= CheckInterval)
	{
		Accumulated = 0.f;
		Evaluate();
	}
	// D70: her inside follows the player's distance more often than she follows the player.
	InsideClock += DeltaSeconds;
	if (InsideClock >= 0.25f)
	{
		InsideClock = 0.f;
		UpdateInside(false);
	}
}

// --- her inside (D70) ---------------------------------------------------------------------------------------------------------
namespace SaltHubInsideLocal
{
	/** Her hull's footprint about her pivot (cm): x 0..160 m of the ship's frame, 20 m either side, keel to poop. */
	constexpr double HalfLength = 8000.0;
	constexpr double HalfBeam = 2000.0;
	constexpr double Bottom = -700.0;
	constexpr double Top = 3600.0;
	/** Shown within InsideShowM, hidden again beyond this much more (no flicker at the edge). */
	constexpr double ShowHysteresisM = 40.0;
}

double ASaltHubShip::DistanceToHullM(const FVector& World) const
{
	const ASaltHubBerth* Berth = GetBerth();
	if (!Berth)
	{
		return 1.0e9;
	}
	const FVector L = Berth->GetActorTransform().InverseTransformPosition(World);
	const double Dx = FMath::Max(FMath::Abs(L.X) - SaltHubInsideLocal::HalfLength, 0.0);
	const double Dy = FMath::Max(FMath::Abs(L.Y) - SaltHubInsideLocal::HalfBeam, 0.0);
	return FMath::Sqrt(Dx * Dx + Dy * Dy) * 0.01;
}

bool ASaltHubShip::IsInHull(const FVector& World) const
{
	const ASaltHubBerth* Berth = GetBerth();
	if (!Berth)
	{
		return false;
	}
	const FVector L = Berth->GetActorTransform().InverseTransformPosition(World);
	return FMath::Abs(L.X) < SaltHubInsideLocal::HalfLength && FMath::Abs(L.Y) < SaltHubInsideLocal::HalfBeam && L.Z > SaltHubInsideLocal::Bottom
		&& L.Z < SaltHubInsideLocal::Top;
}

bool ASaltHubShip::IsInsideVisible() const
{
	if (InsideLevel.IsNull())
	{
		return IsLoaded();
	}
	return IsLoaded() && Inside && Inside->IsLevelLoaded() && Inside->IsLevelVisible();
}

bool ASaltHubShip::IsInsideReady(const UWorld* World)
{
	const ASaltHubShip* Ship = World ? Find(World) : nullptr;
	return !Ship || Ship->IsInsideVisible();
}

void ASaltHubShip::RequireInside(bool bBlock)
{
	if (!bShown)
	{
		Evaluate();
	}
	bInsideForced = true;
	UpdateInside(bBlock);
}

void ASaltHubShip::SetInsideHeld(bool bHeld)
{
	bInsideHeld = bHeld;
	UpdateInside(false);
}

void ASaltHubShip::UpdateInside(bool bBlock)
{
	ASaltHubBerth* Berth = GetBerth();
	if (InsideLevel.IsNull() || !bShown || !Berth)
	{
		UnloadInside();
		ApplyShelters();
		return;
	}
	// The player and every boat: how near her, and whether in her.
	double Nearest = 1.0e9;
	bool bIn = false;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			Nearest = FMath::Min(Nearest, DistanceToHullM(Pawn->GetActorLocation()));
			bIn |= IsInHull(Pawn->GetActorLocation());
		}
	}
	for (TActorIterator<ASaltBoat> It(GetWorld()); It; ++It)
	{
		Nearest = FMath::Min(Nearest, DistanceToHullM(It->GetActorLocation()));
		bIn |= IsInHull(It->GetActorLocation());
	}
	// Required (a loaded game, a test, a goto): kept until the player has come near her and gone away again.
	bForcedNear |= bInsideForced && (bIn || Nearest < InsideShowM);
	if (bInsideForced && bForcedNear && !bIn && Nearest > InsideShowM + SaltHubInsideLocal::ShowHysteresisM)
	{
		bInsideForced = bForcedNear = false;
	}
	const bool bKeep = bInsideHeld || bInsideForced || bIn;
	const bool bShownNow = Inside && Inside->GetShouldBeVisibleFlag();
	const bool bShow = bKeep || Nearest < InsideShowM + (bShownNow ? SaltHubInsideLocal::ShowHysteresisM : 0.0);
	const bool bLoad = bShow || Nearest < (Inside ? InsideUnloadM : InsideLoadM);
	if (!bLoad)
	{
		UnloadInside();
		ApplyShelters();
		return;
	}
	if (!Inside)
	{
		bool bOk = false;
		const FString Name = FString::Printf(TEXT("L_HubInside_%s_%d"), *BerthId.ToString(), ++InstanceCount);
		Inside = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, InsideLevel, Berth->GetActorLocation(), Berth->GetActorRotation(), bOk, Name);
		if (!bOk)
		{
			UE_LOG(LogSaltbound, Warning, TEXT("SB_HUB inside could not be loaded (%s)"), *InsideLevel.ToString());
			Inside = nullptr;
			return;
		}
		UE_LOG(LogSaltbound, Display, TEXT("SB_HUB inside loading (%.0f m from her)"), Nearest);
	}
	if (Inside->GetShouldBeVisibleFlag() != bShow)
	{
		Inside->SetShouldBeVisible(bShow);
		UE_LOG(LogSaltbound, Display, TEXT("SB_HUB inside %s (%.0f m from her)"), bShow ? TEXT("shown") : TEXT("hidden"), Nearest);
	}
	if (bBlock)
	{
		GetWorld()->BlockTillLevelStreamingCompleted();
	}
	ApplyShelters();
}

void ASaltHubShip::UnloadInside()
{
	if (Inside)
	{
		Inside->SetShouldBeLoaded(false);
		Inside->SetShouldBeVisible(false);
		Inside->SetIsRequestingUnloadAndRemoval(true);
		Inside = nullptr;
		UE_LOG(LogSaltbound, Display, TEXT("SB_HUB inside unloaded"));
	}
}

void ASaltHubShip::ApplyShelters()
{
	// Her harbour's water is sheltered while her inside is shown (with it hidden the doors are shut and nothing shows it).
	const bool bOn = IsInsideVisible();
	if (bOn == bShelterOn)
	{
		return;
	}
	bShelterOn = bOn;
	for (TActorIterator<ASaltOcean> It(GetWorld()); It; ++It)
	{
		const ASaltHubBerth* Berth = GetBerth();
		It->Shelters = bOn && Berth ? Berth->GetWorldShelters() : TArray<FSaltShelterBox>();
		break;
	}
}

void ASaltHubShip::HandleGameLoaded()
{
	// A loaded game puts her where it was saved: at once, the world has just been set up again.
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	ASaltHubBerth* Saved = Saves ? FindBerth(Saves->HubBerth) : nullptr;
	if (Saved && Saved->BerthId != BerthId)
	{
		TargetBerthId = Saved->BerthId;
		if (bShown)
		{
			PlaceAt(Saved);
		}
		else
		{
			BerthId = Saved->BerthId;
		}
	}
	Evaluate();
	// D70: loaded in her (moored in her harbour, or standing in her): her inside is there before the first frame.
	bool bInHer = false;
	for (TActorIterator<ASaltBoat> It(GetWorld()); It; ++It)
	{
		bInHer |= It->GetHold() == ESaltBoatHold::Moored && It->GetMooring().Id.ToString().StartsWith(TEXT("Hub"));
	}
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		bInHer |= PC->GetPawn() && IsInHull(PC->GetPawn()->GetActorLocation());
	}
	if (bInHer && bShown)
	{
		RequireInside(true);
	}
}

bool ASaltHubShip::PlayerView(FVector& OutLocation, FVector& OutForward) const
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	FRotator Rot;
	PC->GetPlayerViewPoint(OutLocation, Rot);
	OutForward = Rot.Vector();
	return true;
}

bool ASaltHubShip::BoatMooredToHer() const
{
	for (TActorIterator<ASaltBoat> It(GetWorld()); It; ++It)
	{
		if (It->GetHold() == ESaltBoatHold::Moored && It->GetMooring().Id.ToString().StartsWith(TEXT("Hub")))
		{
			return true;
		}
	}
	return false;
}

void ASaltHubShip::SetShowOverride(int32 Override)
{
	ShowOverride = FMath::Clamp(Override, -1, 1);
	Evaluate();
}

bool ASaltHubShip::MoveToBerth(FName Id)
{
	ASaltHubBerth* Berth = FindBerth(Id);
	if (!Berth)
	{
		return false;
	}
	TargetBerthId = Id;
	PlaceAt(Berth);
	return true;
}

void ASaltHubShip::Evaluate()
{
	TArray<ASaltHubBerth*> Berths = GetBerths();
	if (Berths.Num() == 0 || HubLevel.IsNull())
	{
		return;
	}
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	// Her berth: the one she lies at, the saved one, or the default.
	if (!FindBerth(BerthId))
	{
		ASaltHubBerth* B = Saves ? FindBerth(Saves->HubBerth) : nullptr;
		for (ASaltHubBerth* Candidate : Berths)
		{
			if (!B && Candidate->bDefault)
			{
				B = Candidate;
			}
		}
		BerthId = (B ? B : Berths[0])->BerthId;
	}
	if (!FindBerth(TargetBerthId))
	{
		TargetBerthId = BerthId;
	}
	FVector Eye, Forward;
	const bool bView = PlayerView(Eye, Forward);
	// The harbour the player is in (sailed to, or moored at) becomes the one she will follow to.
	if (bView)
	{
		for (ASaltHubBerth* B : Berths)
		{
			if (FVector::Dist2D(Eye, B->GetActorLocation()) < B->HarbourRadiusM * 100.0)
			{
				TargetBerthId = B->BerthId;
				break;
			}
		}
	}
	const bool bAllowed = ShowOverride == 1 || (ShowOverride != 0 && (!bHiddenUntilLandfall || (Saves && Saves->AnyFound())));
	ASaltHubBerth* Current = FindBerth(BerthId);
	if (!bAllowed)
	{
		if (bShown)
		{
			Unload();
		}
		return;
	}
	if (!bShown)
	{
		// First sight: never popping into view.
		bool bOk = ShowOverride == 1 || !bView;
		if (!bOk)
		{
			const FVector To = Current->GetActorLocation() - Eye;
			const bool bFar = To.Size2D() > FollowDistanceM * 100.0;
			const bool bOutOfView = FVector::DotProduct(To.GetSafeNormal(), Forward) < FMath::Cos(FMath::DegreesToRadians(ViewHalfAngle));
			bOk = bFar || bOutOfView;
		}
		if (bOk)
		{
			PlaceAt(Current);
		}
		return;
	}
	if (TargetBerthId != BerthId && bView && !BoatMooredToHer())
	{
		ASaltHubBerth* Target = FindBerth(TargetBerthId);
		const double Far = FollowDistanceM * 100.0;
		if (Target && FVector::Dist2D(Eye, Current->GetActorLocation()) > Far && FVector::Dist2D(Eye, Target->GetActorLocation()) > Far)
		{
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUB follows the player: %s -> %s"), *BerthId.ToString(), *TargetBerthId.ToString());
			PlaceAt(Target);
		}
	}
}

void ASaltHubShip::PlaceAt(ASaltHubBerth* Berth)
{
	if (!Berth || HubLevel.IsNull())
	{
		return;
	}
	Unload();
	bool bOk = false;
	const FString Name = FString::Printf(TEXT("L_Hub_%s_%d"), *Berth->BerthId.ToString(), ++InstanceCount);
	Instance = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, HubLevel, Berth->GetActorLocation(), Berth->GetActorRotation(), bOk, Name);
	BerthId = Berth->BerthId;
	bShown = bOk && Instance != nullptr;
	if (bShown)
	{
		SetCalmZones(Berth);
	}
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
	{
		Saves->HubBerth = BerthId;
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_HUB placed at %s (%.0f, %.0f) yaw %.1f ok=%d"), *BerthId.ToString(), Berth->GetActorLocation().X,
		Berth->GetActorLocation().Y, Berth->GetActorRotation().Yaw, bShown ? 1 : 0);
}

void ASaltHubShip::Unload()
{
	UnloadInside();
	bInsideForced = bForcedNear = false;
	if (Instance)
	{
		Instance->SetShouldBeLoaded(false);
		Instance->SetShouldBeVisible(false);
		Instance->SetIsRequestingUnloadAndRemoval(true);
		Instance = nullptr;
	}
	SetCalmZones(nullptr);
	bShown = false;
}

void ASaltHubShip::SetCalmZones(ASaltHubBerth* Berth)
{
	ASaltOcean* Ocean = nullptr;
	for (TActorIterator<ASaltOcean> It(GetWorld()); It; ++It)
	{
		Ocean = *It;
		break;
	}
	if (!Ocean)
	{
		return;
	}
	// Take hers away, then add the berth's.
	Ocean->CalmZones.RemoveAll([this](const FSaltCalmZone& Z)
	{
		return AddedZones.ContainsByPredicate([&Z](const FSaltCalmZone& A)
		{
			return A.Center.Equals(Z.Center, 1.0) && FMath::IsNearlyEqual(A.InnerRadius, Z.InnerRadius) && FMath::IsNearlyEqual(A.OuterRadius, Z.OuterRadius);
		});
	});
	AddedZones.Reset();
	if (Berth)
	{
		for (const FSaltCalmZone& Z : Berth->GetWorldCalmZones())
		{
			if (Ocean->CalmZones.Num() >= FSaltWaveField::MaxCalmZones)
			{
				UE_LOG(LogSaltbound, Warning, TEXT("SB_HUB no room for a calm zone (%d in use)"), Ocean->CalmZones.Num());
				break;
			}
			Ocean->CalmZones.Add(Z);
			AddedZones.Add(Z);
		}
	}
	// D69, D70: her harbour's water is sheltered while her inside is shown (ApplyShelters).
	bShelterOn = !IsInsideVisible();
	ApplyShelters();
	Ocean->RebuildWaveField();
}

// --- dev commands ----------------------------------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
namespace SaltHubCommands
{
	struct FGoto
	{
		const TCHAR* Name;
		FVector ShipM;
		float Face;
	};
	// Where each space is entered (ship metres, the data's frame; face: 0 bow, 90 starboard). D70: the harbour on the starboard quay
	// at the aft docking station, the fore docking station, the Slip from the quay, the Tally House where the dock stair arrives,
	// the Well's lower gallery and its foot on the quay, the main deck's waist, the Chandlery's door (D69: in the forecastle's
	// front), the quarterdeck (D69: up the grand stairs).
	const FGoto Places[] = {
		{ TEXT("dock"), FVector(46.0, 7.75, 1.5), 0.f },
		{ TEXT("foredock"), FVector(119.0, 7.75, 1.5), 0.f },
		{ TEXT("slip"), FVector(53.0, 8.0, 1.5), 80.f },
		{ TEXT("tally"), FVector(54.5, 17.6, 4.5), 0.f },
		{ TEXT("well"), FVector(74.5, 7.0, 4.5), 0.f },
		{ TEXT("wellfoot"), FVector(86.5, 7.6, 1.5), 180.f },
		{ TEXT("main"), FVector(60.0, 0.0, 16.5), 0.f },
		{ TEXT("chandlery"), FVector(131.0, 5.75, 16.5), 0.f },
		{ TEXT("quarterdeck"), FVector(40.0, 6.0, 22.5), 180.f },
		{ TEXT("poop"), FVector(22.6, 0.0, 28.5), 180.f },
	};

	FAutoConsoleCommandWithWorldAndArgs CmdShow(
		TEXT("sb.Hub.Show"), TEXT("sb.Hub.Show 1|0|-1: show the hub ship now, hide her, or back to the rule (hidden until the first landfall)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASaltHubShip* Ship = ASaltHubShip::Find(World))
			{
				Ship->SetShowOverride(Args.Num() ? FCString::Atoi(*Args[0]) : 1);
				UE_LOG(LogSaltbound, Display, TEXT("SB_HUB show=%d berth=%s"), Ship->IsShown() ? 1 : 0, *Ship->GetBerthId().ToString());
			}
		}));

	// D76 (owner: "Create a command to let the hub appear instantly to make testing easier").
	FAutoConsoleCommandWithWorldAndArgs CmdAppear(
		TEXT("sb.Hub.Appear"),
		TEXT("sb.Hub.Appear [door|dock|show]: the hub ship at once, for testing: shown now where she lies (no landfall needed), her ")
		TEXT("inside loaded. door (the default): your boat with you aboard 60 m off her aft starboard doors, bow on, at rest: set ")
		TEXT("sail and steer in for the arrival. dock: moored at the fitting berth in her harbour, you on the quay beside the boat. ")
		TEXT("show: only shown."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltHubShip* Ship = ASaltHubShip::Find(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			if (!Ship)
			{
				UE_LOG(LogSaltbound, Warning, TEXT("SB_HUB appear: no hub ship in this world"));
				return;
			}
			Ship->SetShowOverride(1);
			World->BlockTillLevelStreamingCompleted();
			Ship->RequireInside(true);
			const FString How = Args.Num() ? Args[0].ToLower() : TEXT("door");
			ASaltHubBerth* Berth = Ship->GetBerth();
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); It; ++It)
			{
				Boat = *It;
				break;
			}
			if (How == TEXT("show") || !Berth || !Boat || !Player)
			{
				UE_LOG(LogSaltbound, Display, TEXT("SB_HUB appear %s shown=%d inside=%d"), *How, Ship->IsShown() ? 1 : 0, Ship->IsInsideVisible() ? 1 : 0);
				return;
			}
			if (How == TEXT("dock"))
			{
				// Moored at the fitting berth (the aft docking station); the player on the quay beside her, facing her.
				for (TObjectIterator<USaltMooringPointComponent> It; It; ++It)
				{
					if (It->GetWorld() == World && It->MooringId == TEXT("HubFitting"))
					{
						Boat->MoorAt(It->GetMooring());
						// 3 m forward along the quay from her gangway, looking back at her.
						const FVector Quay = It->GetComponentLocation() + It->GetComponentRotation().RotateVector(FVector(300.0, It->Reach + 150.0 * FMath::Sign(It->Reach), It->PierHeight + 5.0));
						const float Face = (float)(Boat->GetActorLocation() - Quay).Rotation().Yaw;
						Player->TeleportFeetTo(Quay, Face);
						PC->SetControlRotation(FRotator(-10.f, Face, 0.f));
						break;
					}
				}
			}
			else
			{
				// Off the aft station's starboard doors, bow on, at rest; the player on her deck looking at the doors.
				ASaltHubDoors* Doors = nullptr;
				for (TActorIterator<ASaltHubDoors> It(World); It; ++It)
				{
					if (!Doors || It->Station == 0)
					{
						Doors = *It;
					}
				}
				if (Doors)
				{
					const FTransform F(FRotator(0.f, Doors->GetActorRotation().Yaw, 0.f), Doors->GetActorLocation());
					const FVector At = F.TransformPosition(FVector(0.0, Doors->HullHalfCm + 6000.0, 0.0));
					const float Heading = (float)F.Rotator().Yaw - 90.f;
					Boat->SetHold(ESaltBoatHold::Free);
					Boat->PlaceAt(At, Heading, 0.f);
					Boat->SetSailSet(false);
					Player->TeleportFeetTo(Boat->GetActorLocation() + FVector(0.0, 0.0, 110.0), Heading);
					PC->SetControlRotation(FRotator(-4.f, Heading, 0.f));
				}
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUB appear %s shown=%d inside=%d boat=(%.0f, %.0f)"), *How, Ship->IsShown() ? 1 : 0,
				Ship->IsInsideVisible() ? 1 : 0, Boat->GetActorLocation().X, Boat->GetActorLocation().Y);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBerth(
		TEXT("sb.Hub.Berth"), TEXT("sb.Hub.Berth <id>: move the hub ship to a berth now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltHubShip* Ship = ASaltHubShip::Find(World);
			const bool bOk = Ship && Args.Num() && Ship->MoveToBerth(FName(*Args[0]));
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUB berth %s ok=%d"), Args.Num() ? *Args[0] : TEXT("-"), bOk ? 1 : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdUnlock(
		TEXT("sb.Hub.Unlock"), TEXT("sb.Hub.Unlock <L#>|all|none: open a locked zone of the hub (its gates stop blocking), all, or none."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
			if (!Saves || Args.Num() == 0)
			{
				return;
			}
			const FString A = Args[0];
			if (A.Equals(TEXT("none"), ESearchCase::IgnoreCase))
			{
				Saves->ClearUnlocked();
			}
			else if (A.Equals(TEXT("all"), ESearchCase::IgnoreCase))
			{
				for (int32 I = 1; I <= 17; ++I)
				{
					Saves->SetUnlocked(FName(*FString::Printf(TEXT("L%d"), I)), true);
				}
			}
			else
			{
				Saves->SetUnlocked(FName(*A.ToUpper()), true);
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUB unlocked=%d"), Saves->GetUnlocked().Num());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdInfo(
		TEXT("sb.Hub.Info"), TEXT("The hub ship's state: berth, target, shown, loaded, the salt purse, zones unlocked."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const ASaltHubShip* Ship = ASaltHubShip::Find(World);
			const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUB info berth=%s target=%s shown=%d loaded=%d inside=%d salt=%d unlocked=%d"),
				Ship ? *Ship->GetBerthId().ToString() : TEXT("-"), Ship ? *Ship->GetTargetBerthId().ToString() : TEXT("-"),
				Ship && Ship->IsShown() ? 1 : 0, Ship && Ship->IsLoaded() ? 1 : 0, Ship && Ship->IsInsideVisible() ? 1 : 0,
				Saves ? Saves->GetSalt() : 0, Saves ? Saves->GetUnlocked().Num() : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSalt(
		TEXT("sb.Dev.Salt"), TEXT("sb.Dev.Salt <n>: set the salt purse."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World))
			{
				if (Args.Num())
				{
					Saves->SetSalt(FCString::Atoi(*Args[0]));
				}
				UE_LOG(LogSaltbound, Display, TEXT("SB_SALT %d"), Saves->GetSalt());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGoto(
		TEXT("sb.Dev.HubGoto"), TEXT("sb.Dev.HubGoto <dock|foredock|slip|tally|well|wellfoot|main|chandlery|quarterdeck|poop>: to a space of the hub (shows her and her inside)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltHubShip* Ship = ASaltHubShip::Find(World);
			const ASaltHubBerth* Berth = Ship ? Ship->GetBerth() : nullptr;
			if (!Berth && Ship)
			{
				Ship->EvaluateNow();
				Berth = Ship->GetBerth();
			}
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			if (!Ship || !Berth || !Player || Args.Num() == 0)
			{
				UE_LOG(LogSaltbound, Warning, TEXT("SB_HUB goto: no hub, berth or player"));
				return;
			}
			if (!Ship->IsShown())
			{
				Ship->SetShowOverride(1);
			}
			World->BlockTillLevelStreamingCompleted();
			Ship->RequireInside(true);                                        // her decks are there to stand on
			for (const FGoto& G : Places)
			{
				if (Args[0].Equals(G.Name, ESearchCase::IgnoreCase))
				{
					const float Yaw = Berth->ShipYawToWorld(G.Face);
					Player->TeleportFeetTo(Berth->ShipToWorld(G.ShipM) + FVector(0.0, 0.0, 2.0), Yaw);
					PC->SetControlRotation(FRotator(-5.f, Yaw, 0.f));
					UE_LOG(LogSaltbound, Display, TEXT("SB_HUB goto %s"), G.Name);
					return;
				}
			}
		}));
}
#endif

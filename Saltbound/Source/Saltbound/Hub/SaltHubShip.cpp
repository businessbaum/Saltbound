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
#include "Kismet/GameplayStatics.h"
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
	// Where each space is entered (ship metres, the data's frame; face: 0 bow, 90 starboard). D64: the Water Hall from its aft quay,
	// the Slip from the forward quay, the Tally House where the dock stair arrives, the Well's lower gallery, the Keelgate chamber,
	// the main deck's waist, the Chandlery's counter.
	const FGoto Places[] = {
		{ TEXT("dock"), FVector(27.5, 0.0, 1.5), 0.f },
		{ TEXT("slip"), FVector(49.0, 9.5, 1.5), 30.f },
		{ TEXT("tally"), FVector(57.2, 15.5, 4.5), 0.f },
		{ TEXT("well"), FVector(74.5, 7.0, 4.5), 0.f },
		{ TEXT("keelgate"), FVector(76.0, 2.0, 1.5), -20.f },
		{ TEXT("main"), FVector(60.0, 0.0, 16.5), 0.f },
		{ TEXT("chandlery"), FVector(105.0, -4.0, 16.5), -90.f },
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
			UE_LOG(LogSaltbound, Display, TEXT("SB_HUB info berth=%s target=%s shown=%d loaded=%d salt=%d unlocked=%d"),
				Ship ? *Ship->GetBerthId().ToString() : TEXT("-"), Ship ? *Ship->GetTargetBerthId().ToString() : TEXT("-"),
				Ship && Ship->IsShown() ? 1 : 0, Ship && Ship->IsLoaded() ? 1 : 0, Saves ? Saves->GetSalt() : 0, Saves ? Saves->GetUnlocked().Num() : 0);
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
		TEXT("sb.Dev.HubGoto"), TEXT("sb.Dev.HubGoto <dock|slip|tally|well|keelgate|main|chandlery>: to a space of the hub (shows her)."),
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
			World->BlockTillLevelStreamingCompleted();                         // her decks are there to stand on
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

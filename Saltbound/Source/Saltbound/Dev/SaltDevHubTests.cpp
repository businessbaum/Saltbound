// Saltbound - development: the hub ship's tests (sb.Dev.RunHubTests, D63; Docs/Design/HubShip/HubShip_Agent_Prompt.md
// section 9). Every place and limit comes from the data through L_Hub's ASaltHubTestData (build_hub_level.py); each part
// runs alone (sb.Dev.RunHubTests hoist), because the whole suite takes minutes.

#include "Dev/SaltDevTestSubsystem.h"

#include "Algo/Reverse.h"
#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Boat/SaltMooringPoint.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Hub/SaltHubBerth.h"
#include "Hub/SaltHubDoors.h"
#include "Hub/SaltHubGate.h"
#include "Hub/SaltHubHoist.h"
#include "Hub/SaltHubLadder.h"
#include "Hub/SaltHubPoint.h"
#include "Hub/SaltHubShip.h"
#include "Hub/SaltHubTestData.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Interaction/SaltStationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Ocean/SaltOceanSubsystem.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "UObject/UObjectIterator.h"
#include "World/SaltLightingRig.h"
#include "World/SaltWindSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdRunHubTests(
		TEXT("sb.Dev.RunHubTests"),
		TEXT("The hub ship's tests (needs her level: L_VisualTest with her berth, or L_HubReview): sb.Dev.RunHubTests [part] with part one of ")
		TEXT("placed, sealow, nolowfloor, headroom, route, gates, boatdock, slipway, hoist, follow, save, ladder."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = World ? World->GetSubsystem<USaltDevTestSubsystem>() : nullptr)
			{
				T->RunHubSuite(Args.Num() > 0 ? Args[0] : FString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHubHoist(
		TEXT("sb.Hub.Hoist"),
		TEXT("Send the hub's hoist to a stop (0 quay, 1 lower deck, 2 main deck): sb.Hub.Hoist <stop> [now]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<ASaltHubHoist> It(World); World && It && Args.Num() > 0; ++It)
			{
				const int32 Stop = FCString::Atoi(*Args[0]);
				if (Args.Num() > 1 && Args[1] == TEXT("now"))
				{
					It->SetStopNow(Stop);
				}
				else
				{
					It->GoTo(Stop);
				}
			}
		}));

	/** What the parts share while they run. */
	struct FHubTestState
	{
		TWeakObjectPtr<ASaltHubShip> Ship;
		TWeakObjectPtr<ASaltHubTestData> Data;
		TWeakObjectPtr<ASaltBoat> Boat;
		TWeakObjectPtr<ASaltHubBerth> DevBerth;
		TWeakObjectPtr<ASaltHubHoist> Hoist;
		TWeakObjectPtr<ASaltHubDoors> Doors;
		TWeakObjectPtr<ASaltHubLadder> Ladder;
		TArray<TWeakObjectPtr<ASaltHubGate>> Gates;
		FName HomeBerth;
		FVector OwnFeet = FVector::ZeroVector;
		float OwnYaw = 0.f;
		FName OwnSea;
		FVector OwnWind = FVector::ZeroVector;
		// A part's scratch.
		float T = 0.f;
		float Best = 0.f;
		float BestT = 0.f;
		float Max1 = 0.f;
		float Max2 = 0.f;
		float Min1 = 0.f;
		int32 Index = 0;
		int32 Count = 0;
		int32 Bad = 0;
		bool bFlag = false;
		bool bDone = false;
		FString Worst;
		FVector Mark = FVector::ZeroVector;
	};

	/** What a hub trace looks at: only what the player's capsule meets (blocks pawns), only the hub's level; and for the
	 *  clear height over a floor (as D61's check) not the props, gates, masts, the hoist or the markers. */
	bool Counts(const FHitResult& Hit, const ULevel* Level, bool bCeiling)
	{
		const AActor* A = Hit.GetActor();
		const UPrimitiveComponent* C = Hit.GetComponent();
		if (!A || !C || A->GetLevel() != Level || C->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			return false;
		}
		if (bCeiling)
		{
			static const FName Skip[] = { TEXT("HubBlk_Props"), TEXT("HubBlk_Gates"), TEXT("HubBlk_Masts"), TEXT("HubBlk_Markers") };
			for (const FName& Tag : Skip)
			{
				if (A->ActorHasTag(Tag))
				{
					return false;
				}
			}
			if (A->IsA<ASaltHubGate>() || A->IsA<ASaltHubHoist>())
			{
				return false;
			}
		}
		return true;
	}

	bool HubTrace(UWorld* World, const ULevel* Level, const FVector& From, const FVector& To, bool bCeiling, FHitResult& Out)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltHubTest), false);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		for (int32 I = 0; I < 16; ++I)
		{
			if (!World->LineTraceSingleByObjectType(Out, From, To, Objects, Params))
			{
				return false;
			}
			if (Counts(Out, Level, bCeiling))
			{
				return true;
			}
			if (Out.GetComponent())
			{
				Params.AddIgnoredComponent(Out.GetComponent());
			}
			else
			{
				return false;
			}
		}
		return false;
	}

	/** Whether a player can stand at a floor spot: nothing of hers within a knee-high sphere (a wall's foot, the bulwark's
	 *  lean, a prop). The clear height is measured only where the player can stand. */
	bool Standable(UWorld* World, const ULevel* Level, const FVector& FeetWorld)
	{
		TArray<FOverlapResult> Hits;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		World->OverlapMultiByObjectType(Hits, FeetWorld + FVector(0.0, 0.0, 45.0), FQuat::Identity, Objects, FCollisionShape::MakeSphere(30.f),
			FCollisionQueryParams(SCENE_QUERY_STAT(SaltHubStand), false));
		for (const FOverlapResult& O : Hits)
		{
			const UPrimitiveComponent* C = O.GetComponent();
			if (C && O.GetActor() && O.GetActor()->GetLevel() == Level && C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				return false;
			}
		}
		return true;
	}

	/** An actor of her level by what it is: the blockout part's tag, else its class. */
	FString What(const AActor* A)
	{
		return !A ? FString(TEXT("-")) : A->Tags.Num() ? A->Tags[0].ToString() : A->GetClass()->GetName();
	}

	FVector FeetOf(const ASaltPlayerCharacter* P)
	{
		return P->GetActorLocation() - FVector(0.0, 0.0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
}
#endif

void USaltDevTestSubsystem::RunHubSuite(const FString& Only)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	ASaltPlayerCharacter* Player0 = GetPlayer();
	USaltSaveSubsystem* Saves0 = USaltSaveSubsystem::Get(World);
	if (!World || !Player0 || !Saves0)
	{
		UE_LOG(LogSaltbound, Error, TEXT("SB_TEST HubSuite FAIL reason=no_player_or_saves"));
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST HubSuite DONE passed=0 failed=1"));
		return;
	}
	TSharedRef<FHubTestState> S = MakeShared<FHubTestState>();
	Passed = Failed = 0;
	Steps.Reset();
	S->OwnFeet = FeetOf(Player0);
	S->OwnYaw = Player0->GetController() ? Player0->GetController()->GetControlRotation().Yaw : 0.f;
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		S->Boat = *It;
		break;
	}
	for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
	{
		S->OwnSea = It->CurrentSeaState;
		break;
	}
	if (const USaltWindSubsystem* Wind = World->GetSubsystem<USaltWindSubsystem>())
	{
		S->OwnWind = FVector(Wind->GetBaseFromDeg(), Wind->GetBaseSpeed(), Wind->GetGustiness());
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_TEST HubSuite START only='%s'"), *Only);

	// Helpers ----------------------------------------------------------------------------------------------------------
	auto EndStep = [this]() { StepTime = 1.0e9f; };
	auto Level = [S]() { return S->Data.IsValid() ? S->Data->GetLevel() : nullptr; };
	auto Rig = [this]() -> ASaltLightingRig*
	{
		for (TActorIterator<ASaltLightingRig> It(GetWorld()); It; ++It)
		{
			return *It;
		}
		return nullptr;
	};
	auto Controller = [this]() { return GetPlayer() ? Cast<APlayerController>(GetPlayer()->GetController()) : nullptr; };
	auto Face = [this, Controller](float WorldYaw)
	{
		if (APlayerController* PC = Controller())
		{
			PC->SetControlRotation(FRotator(0.f, WorldYaw, 0.f));
		}
	};
	auto Input = [this](float Forward, float Right = 0.f)
	{
		if (ASaltPlayerCharacter* P = GetPlayer())
		{
			P->SetDebugMoveInput(FVector2D(Right, Forward));
		}
	};
	// The player's feet at a spot of the ship (the level frame, cm), facing a local yaw.
	auto PlaceLocal = [this, S, Face](const FVector& Local, float LocalYaw)
	{
		ASaltPlayerCharacter* P = GetPlayer();
		ASaltHubTestData* D = S->Data.Get();
		if (P && D)
		{
			if (P->IsAtStation())
			{
				P->LeaveStation();
			}
			const float Yaw = D->LocalYawToWorld(LocalYaw);
			P->TeleportFeetTo(D->ToWorld(Local) + FVector(0.0, 0.0, 3.0), Yaw);
			Face(Yaw);
		}
	};
	auto BoatLocal = [S]()
	{
		const ASaltBoat* B = S->Boat.Get();
		const ASaltHubTestData* D = S->Data.Get();
		return B && D ? D->ToLocal(B->GetActorLocation()) : FVector::ZeroVector;
	};
	auto BoatLocalYaw = [S]()
	{
		const ASaltBoat* B = S->Boat.Get();
		const ASaltHubTestData* D = S->Data.Get();
		return B && D ? (float)FRotator::NormalizeAxis(B->GetYawDeg() - D->GetActorRotation().Yaw) : 0.f;
	};
	auto PlaceBoat = [S](const FVector& Local, float LocalYaw, float SpeedMs, bool bSail)
	{
		ASaltBoat* B = S->Boat.Get();
		const ASaltHubTestData* D = S->Data.Get();
		if (B && D)
		{
			B->SetHold(ESaltBoatHold::Free);
			B->PlaceAt(D->ToWorld(FVector(Local.X, Local.Y, 0.0)), D->LocalYawToWorld(LocalYaw), SpeedMs * 100.f);
			B->SetSailSet(bSail);
			B->SetControlOverride(true, 0.f);
			B->SetAutopilot(false);
		}
	};
	auto FindMooring = [this](FName Id) -> USaltMooringPointComponent*
	{
		for (TObjectIterator<USaltMooringPointComponent> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->MooringId == Id)
			{
				return *It;
			}
		}
		return nullptr;
	};

	// Setup: her level at her berth, the test data in it --------------------------------------------------------------
	Steps.Add({TEXT("HubSetup"), 0.f, [this, S]()
	{
		UWorld* W = GetWorld();
		S->Ship = ASaltHubShip::Find(W);
		if (ASaltHubShip* Ship = S->Ship.Get())
		{
			Ship->SetShowOverride(1);
			W->BlockTillLevelStreamingCompleted();
			S->HomeBerth = Ship->GetBerthId();
		}
		for (TActorIterator<ASaltHubTestData> It(W); It; ++It)
		{
			S->Data = *It;
			break;
		}
		for (TActorIterator<ASaltHubHoist> It(W); It; ++It)
		{
			S->Hoist = *It;
			break;
		}
		for (TActorIterator<ASaltHubDoors> It(W); It; ++It)
		{
			S->Doors = *It;
			break;
		}
		for (TActorIterator<ASaltHubGate> It(W); It; ++It)
		{
			S->Gates.Add(*It);
		}
		S->Gates.Sort([](const TWeakObjectPtr<ASaltHubGate>& A, const TWeakObjectPtr<ASaltHubGate>& B) { return A->GetName() < B->GetName(); });
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST HubSetup ship=%d berth=%s data=%d gates=%d hoist=%d doors=%d boat=%d"), S->Ship.IsValid() ? 1 : 0,
			*S->HomeBerth.ToString(), S->Data.IsValid() ? 1 : 0, S->Gates.Num(), S->Hoist.IsValid() ? 1 : 0, S->Doors.IsValid() ? 1 : 0,
			S->Boat.IsValid() ? 1 : 0);
		if (!S->Data.IsValid())
		{
			Report(TEXT("HubSetup"), false, TEXT("no ASaltHubTestData: her level is not in this world (sb.Hub.Show 1, or L_HubReview)"));
		}
	}, nullptr, nullptr});
	Steps.Add({TEXT("HubSetupSettle"), 1.f, nullptr, nullptr, nullptr});

	// HubPlaced: the ship at her berth (1 cm, 0.1 deg), the sea floor 3 m or more under the keel ---------------------------
	Steps.Add({TEXT("HubPlaced"), 0.f, [this, S, Level]()
	{
		ASaltHubTestData* D = S->Data.Get();
		const ASaltHubShip* Ship = S->Ship.Get();
		const ASaltHubBerth* Berth = Ship ? Ship->GetBerth() : nullptr;
		if (!D || !Berth)
		{
			Report(TEXT("HubPlaced"), false, TEXT("no ship, berth or data"));
			return;
		}
		const float Off = (float)FVector::Dist(D->GetActorLocation(), Berth->GetActorLocation());
		const float YawOff = FMath::Abs((float)FRotator::NormalizeAxis(D->GetActorRotation().Yaw - Berth->GetActorRotation().Yaw));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltHubSeabed), false);
		for (AActor* A : Level()->Actors)
		{
			Params.AddIgnoredActor(A);
		}
		Params.AddIgnoredActor(GetPlayer());
		Params.AddIgnoredActor(S->Boat.Get());
		int32 Low = 0;
		int32 Deep = 0;
		float MinClear = 1.0e9f;
		for (const FVector& P : D->KeelPoints)
		{
			const FVector From = D->ToWorld(FVector(P.X, P.Y, D->KeelZ));
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From - FVector(0.0, 0.0, 10000.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				const float Clear = (float)(From.Z - Hit.ImpactPoint.Z);
				MinClear = FMath::Min(MinClear, Clear);
				Low += Clear < D->SeabedClear ? 1 : 0;
			}
			else
			{
				++Deep;
			}
		}
		Report(TEXT("HubPlaced"), Off < 1.f && YawOff < 0.1f && Low == 0 && D->KeelPoints.Num() >= 40, FString::Printf(
			TEXT("berth=%s offset=%.2fcm yawOff=%.3fdeg keelPoints=%d underClear=%d deeper100m=%d minClear=%.0fcm (need %.0f)"), *Berth->BerthId.ToString(),
			Off, YawOff, D->KeelPoints.Num(), Low, Deep, MinClear < 1.0e8f ? MinClear : -1.f, D->SeabedClear));
	}, nullptr, nullptr});

	// HubSeaLow: Storm for 60 s, the sea under the hull and in the basin --------------------------------------------------
	Steps.Add({TEXT("HubSeaLow"), 60.f, [S, Rig]()
	{
		S->Max1 = S->Max2 = -1.0e9f;
		if (ASaltLightingRig* R = Rig())
		{
			R->SetSeaState(TEXT("Storm"), 0.f);
		}
	}, [this, S](float)
	{
		const ASaltHubTestData* D = S->Data.Get();
		const USaltOceanSubsystem* Ocean = GetWorld()->GetSubsystem<USaltOceanSubsystem>();
		if (!D || !Ocean)
		{
			return;
		}
		auto Sample = [D, Ocean](const FVector& P)
		{
			const FVector W = D->ToWorld(FVector(P.X, P.Y, 0.0));
			return (float)(Ocean->SampleWater(FVector2D(W.X, W.Y)).Location.Z - D->GetActorLocation().Z);
		};
		for (const FVector& P : D->HullWater)
		{
			S->Max1 = FMath::Max(S->Max1, Sample(P));
		}
		for (const FVector& P : D->BasinWater)
		{
			S->Max2 = FMath::Max(S->Max2, Sample(P));
		}
	}, [this, S, Rig]()
	{
		const ASaltHubTestData* D = S->Data.Get();
		if (ASaltLightingRig* R = Rig())
		{
			R->SetSeaState(S->OwnSea.IsNone() ? FName(TEXT("Moderate")) : S->OwnSea, 0.f);
		}
		Report(TEXT("HubSeaLow"), D && S->Max1 < D->HullWaterMax && S->Max2 < D->BasinWaterMax && D->HullWater.Num() >= 30, FString::Printf(
			TEXT("storm60s hullMax=%.0fcm (<%.0f, %d points) basinMax=%.0fcm (<%.0f, %d points)"), S->Max1, D ? D->HullWaterMax : 0.f,
			D ? D->HullWater.Num() : 0, S->Max2, D ? D->BasinWaterMax : 0.f, D ? D->BasinWater.Num() : 0));
	}});

	// HubNoLowFloor: no open floor under +1.5 m (traces down over every open rectangle, 1 m apart; the slipway runs into the sea)
	Steps.Add({TEXT("HubNoLowFloor"), 0.f, [this, S, Level]()
	{
		const ASaltHubTestData* D = S->Data.Get();
		if (!D)
		{
			Report(TEXT("HubNoLowFloor"), false, TEXT("no data"));
			return;
		}
		int32 Samples = 0;
		int32 Low = 0;
		FString Worst;
		float WorstZ = 1.0e9f;
		for (const FSaltHubArea& A : D->OpenFloors)
		{
			for (double X = A.Min.X + 50.0; X < A.Max.X; X += 100.0)
			{
				for (double Y = A.Min.Y + 50.0; Y < A.Max.Y; Y += 100.0)
				{
					const bool bExempt = D->LowFloorExempt.ContainsByPredicate([X, Y](const FSaltHubArea& E)
					{
						return X >= E.Min.X && X < E.Max.X && Y >= E.Min.Y && Y < E.Max.Y;
					});
					FHitResult Hit;
					if (bExempt || !HubTrace(GetWorld(), Level(), D->ToWorld(FVector(X, Y, A.Floor + 100.0)), D->ToWorld(FVector(X, Y, A.Floor - 1000.0)), false, Hit))
					{
						continue;
					}
					++Samples;
					const float Z = (float)D->ToLocal(Hit.ImpactPoint).Z;
					if (Z < D->MinFloorZ - 2.f)
					{
						++Low;
						if (Z < WorstZ)
						{
							WorstZ = Z;
							Worst = FString::Printf(TEXT("%s (%.1f, %.1f) z=%.0fcm on %s"), *A.Id.ToString(), X / 100.0 + 80.0, Y / 100.0, Z, *What(Hit.GetActor()));
						}
					}
				}
			}
		}
		Report(TEXT("HubNoLowFloor"), Low == 0 && Samples > 0, FString::Printf(TEXT("samples=%d below%.0fcm=%d %s"), Samples, D->MinFloorZ, Low,
			Low ? *FString::Printf(TEXT("lowest: %s"), *Worst) : TEXT("")));
	}, nullptr, nullptr});

	// HubHeadroom: 2.2 m or more clear over every open floor sample (0.5 m apart) --------------------------------------------
	Steps.Add({TEXT("HubHeadroom"), 0.f, [this, S, Level]()
	{
		const ASaltHubTestData* D = S->Data.Get();
		if (!D)
		{
			Report(TEXT("HubHeadroom"), false, TEXT("no data"));
			return;
		}
		int32 Samples = 0;
		int32 Low = 0;
		int32 Cramped = 0;
		TArray<FString> Lows;
		float WorstClear = 1.0e9f;
		for (const FSaltHubArea& A : D->OpenFloors)
		{
			for (double X = A.Min.X + 25.0; X < A.Max.X; X += 50.0)
			{
				for (double Y = A.Min.Y + 25.0; Y < A.Max.Y; Y += 50.0)
				{
					FHitResult Down;
					if (!HubTrace(GetWorld(), Level(), D->ToWorld(FVector(X, Y, A.Floor + 100.0)), D->ToWorld(FVector(X, Y, A.Floor - 100.0)), true, Down))
					{
						continue;
					}
					const FVector Floor = D->ToLocal(Down.ImpactPoint);
					if (FMath::Abs(Floor.Z - A.Floor) > 30.0)
					{
						continue;                                   // a step or a prop's top, not the floor itself
					}
					if (!Standable(GetWorld(), Level(), Down.ImpactPoint))
					{
						++Cramped;                                  // against a wall's foot: nobody stands there
						continue;
					}
					++Samples;
					FHitResult Up;
					const bool bRoof = HubTrace(GetWorld(), Level(), D->ToWorld(Floor + FVector(0.0, 0.0, 5.0)), D->ToWorld(Floor + FVector(0.0, 0.0, 1500.0)), true, Up);
					const float Clear = bRoof ? (float)(D->ToLocal(Up.ImpactPoint).Z - Floor.Z) : 1500.f;
					WorstClear = FMath::Min(WorstClear, Clear);
					if (Clear < D->MinHeadroom)
					{
						++Low;
						if (Lows.Num() < 8)
						{
							Lows.Add(FString::Printf(TEXT("%s(%.1f,%.1f)=%.0fcm:%s"), *A.Id.ToString(), X / 100.0 + 80.0, Y / 100.0, Clear, *What(Up.GetActor())));
						}
					}
				}
			}
		}
		Report(TEXT("HubHeadroom"), Low == 0 && Samples > 0, FString::Printf(TEXT("samples=%d against_walls=%d under%.0fcm=%d least=%.0fcm %s"), Samples,
			Cramped, D->MinHeadroom, Low, WorstClear, *FString::Join(Lows, TEXT(" "))));
	}, nullptr, nullptr});

	// HubRoute: walk (simulated input) from the fitting berth to each NPC spot and back, within the time target ----------------
	const int32 RouteCount = 4;
	for (int32 R = 0; R < RouteCount; ++R)
	{
		for (int32 Back = 0; Back < 2; ++Back)
		{
			Steps.Add({FString::Printf(TEXT("HubRoute%d%s"), R, Back ? TEXT("Back") : TEXT("Go")), 160.f, [S, R, Back, PlaceLocal]()
			{
				const ASaltHubTestData* D = S->Data.Get();
				S->T = 0.f;
				S->Index = 1;
				S->Best = 1.0e9f;
				S->BestT = 0.f;
				S->bDone = false;
				S->Worst.Reset();
				if (!D || !D->Routes.IsValidIndex(R) || D->Routes[R].Points.Num() < 2)
				{
					S->bDone = true;
					return;
				}
				TArray<FVector> Pts = D->Routes[R].Points;
				if (Back)
				{
					Algo::Reverse(Pts);
				}
				// Back starts where Go ended; if Go did not get there, from the route's end.
				if (!Back || !S->bFlag)
				{
					const FVector Dir = Pts[1] - Pts[0];
					PlaceLocal(Pts[0], FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
				}
			}, [this, S, R, Back, EndStep, Face, Input](float Dt)
			{
				ASaltHubTestData* D = S->Data.Get();
				ASaltPlayerCharacter* P = GetPlayer();
				if (S->bDone || !D || !P || !D->Routes.IsValidIndex(R))
				{
					EndStep();
					return;
				}
				const FSaltHubRoute& Route = D->Routes[R];
				const TArray<FVector>& Raw = Route.Points;
				auto Pt = [&Raw, Back](int32 I) { return Back ? Raw[Raw.Num() - 1 - I] : Raw[I]; };
				S->T += Dt;
				if (S->T < 0.8f)
				{
					return;                                         // land first
				}
				const float Walked = S->T - 0.8f;
				const FVector Feet = D->ToLocal(FeetOf(P));
				FVector Target = Pt(S->Index);
				float Dist = (float)FVector::Dist2D(Feet, Target);
				if (Dist < 55.f && FMath::Abs(Feet.Z - Target.Z) < 120.f)
				{
					++S->Index;
					S->Best = 1.0e9f;
					S->BestT = Walked;
					if (S->Index >= Raw.Num())
					{
						Input(0.f);
						S->bDone = true;
						S->bFlag = !Back;                           // Go arrived: Back starts there
						const FString Name = FString::Printf(TEXT("HubRoute_%s_%s"), *Route.Name.ToString(), Back ? TEXT("back") : TEXT("go"));
						Report(Name, Walked <= Route.TargetSeconds, FString::Printf(TEXT("time=%.1fs (target %.0f) points=%d"), Walked, Route.TargetSeconds, Raw.Num()));
						EndStep();
						return;
					}
					Target = Pt(S->Index);
					Dist = (float)FVector::Dist2D(Feet, Target);
				}
				if (Dist < S->Best - 25.f)
				{
					S->Best = Dist;
					S->BestT = Walked;
				}
				else if (Walked - S->BestT > 3.f || Walked > 150.f)
				{
					Input(0.f);
					S->bDone = true;
					S->bFlag = false;
					Report(FString::Printf(TEXT("HubRoute_%s_%s"), *Route.Name.ToString(), Back ? TEXT("back") : TEXT("go")), false, FString::Printf(
						TEXT("STUCK going to %d (%.1f, %.1f, %.1f) at (%.1f, %.1f, %.1f) after %.1fs"), S->Index, Target.X / 100.0 + 80.0, Target.Y / 100.0,
						Target.Z / 100.0, Feet.X / 100.0 + 80.0, Feet.Y / 100.0, Feet.Z / 100.0, Walked));
					EndStep();
					return;
				}
				const FVector To = Target - Feet;
				Face(D->LocalYawToWorld(FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X))));
				Input(1.f);
			}, [Input]() { Input(0.f); }});
		}
	}

	// HubGatesBlock: walking into each gate stops the player; E shows its line; unlocked, it lets them by ----------------------
	Steps.Add({TEXT("HubGatesBlockStart"), 0.f, [S]() { S->Index = -1; S->Count = 0; S->Bad = 0; S->Worst.Reset(); }, nullptr, nullptr});
	for (int32 G = 0; G < 40; ++G)
	{
		Steps.Add({FString::Printf(TEXT("HubGatesBlock%02d"), G), 2.6f, [this, S, G, Face]()
		{
			S->T = 0.f;
			ASaltHubGate* Gate = S->Gates.IsValidIndex(G) ? S->Gates[G].Get() : nullptr;
			ASaltPlayerCharacter* P = GetPlayer();
			if (!Gate || !P)
			{
				return;
			}
			const FVector From = Gate->GetActorTransform().TransformPosition(Gate->Approach);
			const FVector To = Gate->GetActorLocation() - From;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
			if (P->IsAtStation())
			{
				P->LeaveStation();
			}
			P->TeleportFeetTo(From + FVector(0.0, 0.0, 3.0), Yaw);
			Face(Yaw);
		}, [this, S, G, Input, EndStep](float Dt)
		{
			ASaltHubGate* Gate = S->Gates.IsValidIndex(G) ? S->Gates[G].Get() : nullptr;
			if (!Gate)
			{
				EndStep();
				return;
			}
			S->T += Dt;
			Input(S->T > 0.4f && Gate->bTryWalk ? 1.f : 0.f);
		}, [this, S, G, Input]()
		{
			Input(0.f);
			ASaltHubGate* Gate = S->Gates.IsValidIndex(G) ? S->Gates[G].Get() : nullptr;
			ASaltPlayerCharacter* P = GetPlayer();
			ASaltPlayerController* PC = P ? Cast<ASaltPlayerController>(P->GetController()) : nullptr;
			if (!Gate || !P)
			{
				return;
			}
			// Across the blocker's middle the player must not have come: measured along the way from the approach spot.
			const FVector From = Gate->GetActorTransform().TransformPosition(Gate->Approach);
			const FVector N = (From - Gate->GetActorLocation()).GetSafeNormal2D();
			const float Ahead = (float)FVector::DotProduct(FeetOf(P) - Gate->GetActorLocation(), N);
			const bool bBlocks = Gate->GetBlocker()->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
			const bool bHeld = Gate->bTryWalk ? Ahead > 0.f : bBlocks;
			Gate->GetUse()->Interact(P);
			const FString Shown = PC ? PC->GetNoticeText().ToString() : FString();
			const bool bLine = !Gate->Line.IsEmpty() && Shown == Gate->Line.ToString();
			Report(FString::Printf(TEXT("HubGatesBlock_%s_%s"), *Gate->GateType.ToString(), *Gate->ZoneId.ToString()), bHeld && bLine, FString::Printf(
				TEXT("gate=%s walk=%d stoppedAt=%.0fcm blocks=%d line='%s' shown='%s'"), *Gate->GetName(), Gate->bTryWalk ? 1 : 0, Ahead, bBlocks ? 1 : 0,
				*Gate->Line.ToString(), *Shown));
		}});
	}
	Steps.Add({TEXT("HubGatesBlockUnlock"), 0.f, [this, S]()
	{
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
		if (!Saves || S->Gates.Num() == 0)
		{
			Report(TEXT("HubGatesBlockUnlock"), false, TEXT("no saves or gates"));
			return;
		}
		auto Open = [S]()
		{
			int32 N = 0;
			for (const TWeakObjectPtr<ASaltHubGate>& G : S->Gates)
			{
				N += G.IsValid() && G->GetBlocker()->GetCollisionEnabled() == ECollisionEnabled::NoCollision ? 1 : 0;
			}
			return N;
		};
		const int32 Before = Open();
		for (const TWeakObjectPtr<ASaltHubGate>& G : S->Gates)                // every zone a gate closes (D64: L1-L18)
		{
			if (G.IsValid())
			{
				Saves->SetUnlocked(G->ZoneId, true);
			}
		}
		const int32 Unlocked = Open();
		Saves->ClearUnlocked();
		const int32 After = Open();
		Report(TEXT("HubGatesBlockUnlock"), Before == 0 && Unlocked == S->Gates.Num() && After == 0, FString::Printf(
			TEXT("gates=%d openLocked=%d openUnlocked=%d openRelocked=%d"), S->Gates.Num(), Before, Unlocked, After));
	}, nullptr, nullptr});

	// HubBoatDock: in from the side through the hidden doors with her mast up (the dock watch opens them in time), without
	// touching a leaf or the doorway; moored at the fitting berth and a fleet berth (the save keeps it); out again (D64)
	auto DockRun = [this, S, BoatLocal, BoatLocalYaw, EndStep](bool bIn)
	{
		return [this, S, bIn, BoatLocal, BoatLocalYaw, EndStep](float Dt)
		{
			ASaltBoat* B = S->Boat.Get();
			const ASaltHubTestData* D = S->Data.Get();
			ASaltHubDoors* Doors = S->Doors.Get();
			if (!B || !D || !Doors)
			{
				Report(bIn ? TEXT("HubBoatDockIn") : TEXT("HubBoatDockOut"), false, TEXT("no boat, data or doors"));
				EndStep();
				return;
			}
			S->T += Dt;
			const FVector L = BoatLocal();
			const float Yaw = BoatLocalYaw();
			const float Mid = 0.5f * (float)(D->DoorsX.X + D->DoorsX.Y);
			const float Half = D->DoorsHullHalf;
			// Going out, she waits (held, sail down) until the doors stand open (E at a capstan), then sails.
			if (!bIn && !S->bFlag)
			{
				B->SetSailSet(false);
				B->SetControlOverride(true, 0.f);
				if (Doors->IsOpen())
				{
					S->bFlag = true;
					B->SetSailSet(true);
				}
				if (S->T > 15.f)
				{
					Report(TEXT("HubBoatDockOut"), false, TEXT("the doors did not open"));
					EndStep();
				}
				return;
			}
			// Steer along the doors' middle: the heading wanted leans 20 deg per metre off it (30 at most); + oar = to starboard.
			const float Base = bIn ? -90.f : 90.f;
			const float Lean = FMath::Clamp((bIn ? -20.f : 20.f) * ((float)L.X - Mid) / 100.f, -30.f, 30.f);
			const float Err = (float)FRotator::NormalizeAxis(Base + Lean - Yaw);
			B->SetControlOverride(true, FMath::Clamp(Err / 25.f, -1.f, 1.f));
			const float Out = FMath::Abs((float)L.Y);
			// Coming in she takes in her sail 6 m short of the hull and glides through (her sail set is about as wide as the doors'
			// narrowest clear way, D65); going out she sets it once past the leaves.
			if (bIn && Out < Half + 600.f)
			{
				B->SetSailSet(false);
			}
			// Through the doorway (from 3 m outside the hull to past the open leaves): no contact, clear of the leaves.
			if (Out > Half - 900.f && Out < Half + 300.f)
			{
				S->Max1 = FMath::Max(S->Max1, B->GetContactDepth());
				S->Count += B->GetContactCount() > 0 ? 1 : 0;
				const float Side = FMath::Min((float)L.X - (float)D->DoorsX.X, (float)D->DoorsX.Y - (float)L.X);
				S->Min1 = FMath::Min(S->Min1, Side - 170.f);                                  // the boat's half-beam (D36)
				S->Best = FMath::Min(S->Best, Side - SaltHull::SailHalfFoot * 100.f);         // her sail's foot (D65), at the narrowest
			}
			if (bIn && S->Max2 < 0.f && Out < Half + 600.f)
			{
				S->Max2 = Doors->GetOpenAmount();                    // how far open when her bow reaches the hull
			}
			const bool bArrived = bIn ? Out < Half - 900.f : Out > Half + 1500.f;      // past the open leaves, or 15 m out
			if (bArrived || S->T > 60.f)
			{
				const bool bThrough = bArrived;
				const bool bOpenInTime = !bIn || S->Max2 >= 0.99f;
				if (bIn)
				{
					B->SetControlOverride(true, 0.f);
				}
				// The hull's contact decides (the leaves stand furthest in at 6 m up, over her hull); the gaps at that narrowest
				// height are reported for her rig.
				Report(bIn ? TEXT("HubBoatDockIn") : TEXT("HubBoatDockOut"), bThrough && bOpenInTime && S->Max1 < 1.f && S->Count == 0,
					FString::Printf(TEXT("t=%.1fs end=(%.1f, %.1f) doorsAtHull=%.2f contactFrames=%d depth=%.1fcm beamGapAtNarrowest=%.2fm sailGapAtNarrowest=%.2fm"), S->T,
					L.X / 100.0 + 80.0, L.Y / 100.0, S->Max2, S->Count, S->Max1, S->Min1 / 100.f, S->Best / 100.f));
				EndStep();
			}
		};
	};
	Steps.Add({TEXT("HubBoatDockIn"), 70.f, [this, S, PlaceBoat]()
	{
		S->T = 0.f;
		S->Max1 = 0.f;
		S->Max2 = -1.f;
		S->Min1 = S->Best = 1.0e9f;
		S->Count = 0;
		if (USaltWindSubsystem* W = GetWorld()->GetSubsystem<USaltWindSubsystem>())
		{
			W->SetBaseWind(200.f, 700.f, 0.f);
		}
		if (ASaltHubDoors* Doors = S->Doors.Get())
		{
			Doors->SetOpenAmountNow(0.f);                             // shut: the watch must open them for her
			Doors->bWatch = true;
		}
		if (const ASaltHubTestData* D = S->Data.Get())
		{
			PlaceBoat(D->BoatInStart, -90.f, 2.5f, true);
		}
	}, DockRun(true), nullptr});
	auto Moor = [this, S, FindMooring](FName Id)
	{
		return [this, S, Id, FindMooring]()
		{
			ASaltBoat* B = S->Boat.Get();
			const USaltMooringPointComponent* M = FindMooring(Id);
			if (!B || !M)
			{
				return;
			}
			// At rest 2 m out from the berth into the basin, then the windlass (E): it moors to the nearest berth.
			const FSaltMooring Spot = M->GetMooring();
			const FVector Out = Spot.Pose.GetRotation().RotateVector(FVector(0.0, -Spot.Side() * 200.0, 0.0));
			B->SetHold(ESaltBoatHold::Free);
			B->PlaceAt(Spot.Pose.GetLocation() + Out, Spot.Pose.Rotator().Yaw, 0.f);
			B->SetSailSet(false);
			B->SetControlOverride(true, 0.f);
			B->ToggleHold();
		};
	};
	auto Moored = [this, S, FindMooring](FName Id, const TCHAR* Name)
	{
		return [this, S, Id, Name, FindMooring]()
		{
			const ASaltBoat* B = S->Boat.Get();
			const USaltMooringPointComponent* M = FindMooring(Id);
			const float Off = B && M ? (float)FVector::Dist2D(B->GetActorLocation(), M->GetComponentLocation()) : -1.f;
			Report(Name, B && M && B->GetHold() == ESaltBoatHold::Moored && B->GetMooring().Id == Id && Off < 80.f, FString::Printf(
				TEXT("hold=%d mooring=%s offset=%.0fcm heel=%.1fdeg"), B ? (int32)B->GetHold() : -1, B ? *B->GetMooring().Id.ToString() : TEXT("-"), Off,
				B ? B->GetHeelDeg() : 0.f));
		};
	};
	auto Keep = [this, S, FindMooring](FName Id, const TCHAR* Name)
	{
		return [this, S, Id, Name, FindMooring]()
		{
			ASaltBoat* B = S->Boat.Get();
			USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
			const USaltMooringPointComponent* M = FindMooring(Id);
			if (!B || !Saves || !M)
			{
				Report(Name, false, TEXT("no boat, saves or mooring"));
				return;
			}
			const FString Own = Saves->Slot;
			Saves->Slot = TEXT("SaltDevTest");
			const bool bSaved = Saves->SaveGame(GetWorld(), TEXT("test"));
			B->ResetToStart();
			const bool bLoaded = Saves->LoadGame(GetWorld());
			UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
			Saves->Slot = Own;
			const float Off = (float)FVector::Dist2D(B->GetActorLocation(), M->GetComponentLocation());
			Report(Name, bSaved && bLoaded && B->GetHold() == ESaltBoatHold::Moored && B->GetMooring().Id == Id && Off < 100.f, FString::Printf(
				TEXT("saved=%d loaded=%d hold=%d mooring=%s offset=%.0fcm"), bSaved ? 1 : 0, bLoaded ? 1 : 0, (int32)B->GetHold(),
				*B->GetMooring().Id.ToString(), Off));
		};
	};
	Steps.Add({TEXT("HubBoatDockMoorFitting"), 10.f, Moor(TEXT("HubFitting")), nullptr, Moored(TEXT("HubFitting"), TEXT("HubBoatDockMoorFitting"))});
	Steps.Add({TEXT("HubBoatDockKeepFitting"), 0.f, Keep(TEXT("HubFitting"), TEXT("HubBoatDockKeepFitting")), nullptr, nullptr});
	Steps.Add({TEXT("HubBoatDockMoorFleet"), 10.f, Moor(TEXT("HubFleet1")), nullptr, Moored(TEXT("HubFleet1"), TEXT("HubBoatDockMoorFleet"))});
	Steps.Add({TEXT("HubBoatDockKeepFleet"), 0.f, Keep(TEXT("HubFleet1"), TEXT("HubBoatDockKeepFleet")), nullptr, nullptr});
	Steps.Add({TEXT("HubBoatDockOut"), 80.f, [S, PlaceBoat]()
	{
		S->T = 0.f;
		S->Max1 = S->Max2 = 0.f;
		S->Min1 = S->Best = 1.0e9f;
		S->Count = 0;
		S->bFlag = false;
		if (const ASaltHubTestData* D = S->Data.Get())
		{
			PlaceBoat(D->HallMiddle, 90.f, 0.f, false);
		}
		if (ASaltHubDoors* Doors = S->Doors.Get())
		{
			Doors->Open();                                            // as E at a capstan
		}
	}, DockRun(false), nullptr});

	// HubSlipway: a boat driven up the slipway runs aground softly and lies still ------------------------------------------
	Steps.Add({TEXT("HubSlipway"), 20.f, [S, PlaceBoat]()
	{
		S->Max1 = 0.f;
		if (const ASaltHubTestData* D = S->Data.Get())
		{
			PlaceBoat(D->SlipStart, 0.f, 2.5f, false);
		}
	}, [S](float)
	{
		if (const ASaltBoat* B = S->Boat.Get())
		{
			S->Max1 = FMath::Max(S->Max1, B->GetContactDepth());
		}
	}, [this, S, BoatLocal]()
	{
		const ASaltBoat* B = S->Boat.Get();
		const ASaltHubTestData* D = S->Data.Get();
		if (!B || !D)
		{
			Report(TEXT("HubSlipway"), false, TEXT("no boat or data"));
			return;
		}
		const FVector L = BoatLocal();
		const float Speed = (float)B->GetVelocity().Size2D() / 100.f;
		const bool bOn = L.X > D->Slipway.Min.X - 800.0 && L.X < D->Slipway.Max.X && FMath::Abs(L.Y) < D->Slipway.Max.Y + 200.0;
		Report(TEXT("HubSlipway"), Speed < 0.3f && S->Max1 < 12.f && B->GetContactCount() > 0 && FMath::Abs(B->GetHeelDeg()) < 10.f && bOn, FString::Printf(
			TEXT("finalSpeed=%.2fm/s maxPenetration=%.1fcm contacts=%d heel=%.1fdeg at=(%.1f, %.1f) slipway x %.0f..%.0f"), Speed, S->Max1,
			B->GetContactCount(), B->GetHeelDeg(), L.X / 100.0 + 80.0, L.Y / 100.0, D->Slipway.Min.X / 100.0 + 80.0, D->Slipway.Max.X / 100.0 + 80.0));
	}});

	// HubHoist: E rides it to each stop in time; the player stays inside the cage; no stepping out between stops -------------
	auto InCage = [this, S]()
	{
		const ASaltHubHoist* H = S->Hoist.Get();
		const ASaltPlayerCharacter* P = GetPlayer();
		if (!H || !P)
		{
			return false;
		}
		const FVector L = H->GetActorTransform().InverseTransformPosition(FeetOf(P)) - FVector(0.0, 0.0, H->GetHeight());
		return FMath::Abs(L.X) < 130.0 && FMath::Abs(L.Y) < 130.0 && FMath::Abs(L.Z - 12.0) < 40.0 && P->IsAtStation();
	};
	Steps.Add({TEXT("HubHoistBoard"), 1.f, [this, S]()
	{
		ASaltHubHoist* H = S->Hoist.Get();
		ASaltPlayerCharacter* P = GetPlayer();
		if (H && P)
		{
			if (P->IsAtStation())
			{
				P->LeaveStation();
			}
			H->SetStopNow(0);
			P->TeleportFeetTo(H->GetActorTransform().TransformPosition(FVector(-200.0, 0.0, 3.0)), H->GetActorRotation().Yaw);
			H->GetStation()->Interact(P);                         // E in the cage
		}
	}, nullptr, nullptr});
	auto Ride = [this, S, InCage, Input, EndStep](int32 Stop, float Limit, bool bTryLeave)
	{
		const FString Name = FString::Printf(TEXT("HubHoistTo%d"), Stop);
		Steps.Add({Name, Limit + 3.f, [S]()
		{
			S->T = 0.f;
			S->bFlag = true;
			S->bDone = false;
			S->Count = 0;
			const ASaltHubHoist* H = S->Hoist.Get();
			S->Index = H ? H->GetStop() : -1;
		}, [this, S, Stop, InCage, Input, EndStep, bTryLeave](float Dt)
		{
			ASaltHubHoist* H = S->Hoist.Get();
			ASaltPlayerCharacter* P = GetPlayer();
			if (!H || !P)
			{
				EndStep();
				return;
			}
			S->T += Dt;
			Input(S->T < 0.3f ? (Stop > S->Index ? 1.f : -1.f) : 0.f);    // W or S, once
			S->bFlag &= InCage();
			if (bTryLeave && S->T > 2.f && S->Count == 0)
			{
				S->Count = 1;
				P->DebugInteract();                                // E while moving: held inside
			}
			if (S->T > 0.5f && H->GetStop() == Stop)
			{
				S->bDone = true;
				EndStep();
			}
		}, [this, S, Stop, Limit, Name, Input]()
		{
			Input(0.f);
			const ASaltHubHoist* H = S->Hoist.Get();
			Report(Name, S->bDone && S->T <= Limit && S->bFlag, FString::Printf(TEXT("from=%d to=%d time=%.1fs (limit %.1f) stayedInside=%d stop=%d"),
				S->Index, Stop, S->T, Limit, S->bFlag ? 1 : 0, H ? H->GetStop() : -1));
		}});
	};
	// The times: 4 m and 12 m at 1.6 m/s with 0.6 m/s2 to start and stop take 5.2 s and 10.2 s; a second over that.
	Ride(1, 6.5f, false);
	Ride(2, 11.5f, true);
	Steps.Add({TEXT("HubHoistStepOut"), 1.f, [this]()
	{
		if (ASaltPlayerCharacter* P = GetPlayer())
		{
			P->DebugInteract();                                    // E at the stop: out on to the deck
		}
	}, nullptr, [this, S]()
	{
		const ASaltHubHoist* H = S->Hoist.Get();
		const ASaltPlayerCharacter* P = GetPlayer();
		if (!H || !P)
		{
			Report(TEXT("HubHoistStepOut"), false, TEXT("no hoist or player"));
			return;
		}
		const FVector L = H->GetActorTransform().InverseTransformPosition(FeetOf(P));
		const float Deck = H->StopHeights.Num() > 2 ? H->StopHeights[2] : 0.f;
		Report(TEXT("HubHoistStepOut"), !P->IsAtStation() && FMath::Abs(L.Z - Deck) < 40.f && L.X < H->GapCentre.X, FString::Printf(
			TEXT("atStation=%d feet=(%.0f, %.0f, %.0f) deck=%.0f"), P->IsAtStation() ? 1 : 0, L.X, L.Y, L.Z, Deck));
	}});
	Steps.Add({TEXT("HubHoistReboard"), 1.f, [this, S]()
	{
		ASaltHubHoist* H = S->Hoist.Get();
		if (ASaltPlayerCharacter* P = GetPlayer(); H && P)
		{
			H->GetStation()->Interact(P);
		}
	}, nullptr, nullptr});
	Ride(1, 11.5f, false);
	Ride(0, 6.5f, false);

	// HubFollow: a dev berth 6 km away; she stays while the player is within 2.5 km of a berth, moves when both are far ----------
	Steps.Add({TEXT("HubFollowSetup"), 1.f, [this, S, Face]()
	{
		ASaltHubShip* Ship = S->Ship.Get();
		const ASaltHubBerth* Home = Ship ? Ship->GetBerth() : nullptr;
		ASaltPlayerCharacter* P = GetPlayer();
		if (!Home || !P)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.Name = TEXT("DevBerth");
		Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		ASaltHubBerth* Dev = GetWorld()->SpawnActor<ASaltHubBerth>(Home->GetActorLocation() + FVector(600000.0, 0.0, 0.0), Home->GetActorRotation(), Params);
		if (Dev)
		{
			Dev->BerthId = TEXT("DevBerth");
			Dev->HarbourRadiusM = Home->HarbourRadiusM;
			Dev->bDefault = false;
		}
		S->DevBerth = Dev;
		// 100 m off her side (abeam: her +Y is about world -X at the berth's heading; any side clear of the hull will do).
		P->TeleportFeetTo(Home->GetActorLocation() + Home->GetActorRotation().RotateVector(FVector(0.0, -10000.0, 20.0)), 0.f);
		Ship->EvaluateNow();
	}, nullptr, nullptr});
	Steps.Add({TEXT("HubFollowStays"), 3.f, [this, S]()
	{
		ASaltHubShip* Ship = S->Ship.Get();
		const ASaltHubBerth* Dev = S->DevBerth.Get();
		if (ASaltPlayerCharacter* P = GetPlayer(); Ship && Dev && P)
		{
			P->TeleportFeetTo(Dev->GetActorLocation() + Dev->GetActorRotation().RotateVector(FVector(0.0, -10000.0, 20.0)), 0.f);   // in the other harbour
			Ship->EvaluateNow();
		}
	}, nullptr, [this, S]()
	{
		ASaltHubShip* Ship = S->Ship.Get();
		if (Ship)
		{
			Ship->EvaluateNow();
		}
		Report(TEXT("HubFollowStays"), Ship && S->DevBerth.IsValid() && Ship->GetBerthId() == S->HomeBerth && Ship->GetTargetBerthId() == TEXT("DevBerth"),
			FString::Printf(TEXT("berth=%s target=%s (the player in the dev berth's harbour, under 2.5 km from it)"),
			Ship ? *Ship->GetBerthId().ToString() : TEXT("-"), Ship ? *Ship->GetTargetBerthId().ToString() : TEXT("-")));
	}});
	Steps.Add({TEXT("HubFollowMoves"), 3.f, [this, S]()
	{
		ASaltHubShip* Ship = S->Ship.Get();
		const ASaltHubBerth* Home = Ship ? Ship->FindBerth(S->HomeBerth) : nullptr;
		const ASaltHubBerth* Dev = S->DevBerth.Get();
		if (ASaltPlayerCharacter* P = GetPlayer(); Ship && Home && Dev && P)
		{
			// Halfway, off to the side: 3 km or more from both.
			S->Mark = 0.5 * (Home->GetActorLocation() + Dev->GetActorLocation()) + FVector(0.0, 50000.0, 20.0);
			P->TeleportFeetTo(S->Mark, 0.f);
			Ship->EvaluateNow();
		}
	}, nullptr, [this, S]()
	{
		ASaltHubShip* Ship = S->Ship.Get();
		const ASaltHubBerth* Home = Ship ? Ship->FindBerth(S->HomeBerth) : nullptr;
		const ASaltHubBerth* Dev = S->DevBerth.Get();
		GetWorld()->BlockTillLevelStreamingCompleted();
		const float DHome = Home ? (float)FVector::Dist2D(S->Mark, Home->GetActorLocation()) / 100.f : 0.f;
		const float DDev = Dev ? (float)FVector::Dist2D(S->Mark, Dev->GetActorLocation()) / 100.f : 0.f;
		const bool bMoved = Ship && Ship->GetBerthId() == TEXT("DevBerth");
		Report(TEXT("HubFollowMoves"), bMoved && Ship->IsLoaded() && FMath::Min(DHome, DDev) > Ship->FollowDistanceM, FString::Printf(
			TEXT("berth=%s loaded=%d playerFromHome=%.0fm fromDev=%.0fm (moves only beyond %.0fm)"), Ship ? *Ship->GetBerthId().ToString() : TEXT("-"),
			Ship && Ship->IsLoaded() ? 1 : 0, DHome, DDev, Ship ? Ship->FollowDistanceM : 0.f));
	}});
	Steps.Add({TEXT("HubFollowReturn"), 0.5f, [this, S]()
	{
		ASaltHubShip* Ship = S->Ship.Get();
		if (ASaltPlayerCharacter* P = GetPlayer())
		{
			P->TeleportFeetTo(S->OwnFeet, S->OwnYaw);
		}
		if (Ship)
		{
			Ship->MoveToBerth(S->HomeBerth);
			GetWorld()->BlockTillLevelStreamingCompleted();
		}
		if (ASaltHubBerth* Dev = S->DevBerth.Get())
		{
			Dev->Destroy();
		}
		// Her level came back: the test data and the actors in it are new ones.
		for (TActorIterator<ASaltHubTestData> It(GetWorld()); It; ++It)
		{
			S->Data = *It;
		}
		S->Gates.Reset();
		for (TActorIterator<ASaltHubGate> It(GetWorld()); It; ++It)
		{
			S->Gates.Add(*It);
		}
		for (TActorIterator<ASaltHubHoist> It(GetWorld()); It; ++It)
		{
			S->Hoist = *It;
		}
	}, nullptr, nullptr});

	// HubSave: the berth, the unlocked zones and the salt survive a save and a load ----------------------------------------
	Steps.Add({TEXT("HubSave"), 0.f, [this, S]()
	{
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
		const ASaltHubShip* Ship = S->Ship.Get();
		if (!Saves)
		{
			Report(TEXT("HubSave"), false, TEXT("no saves"));
			return;
		}
		const FString Own = Saves->Slot;
		const int32 OwnSalt = Saves->GetSalt();
		const TArray<FName> OwnUnlocked = Saves->GetUnlocked();
		Saves->Slot = TEXT("SaltDevTest");
		Saves->SetSalt(4321);
		Saves->ClearUnlocked();
		Saves->SetUnlocked(TEXT("L5"), true);
		Saves->SetUnlocked(TEXT("L9"), true);
		const FName Berth = Ship ? Ship->GetBerthId() : FName(TEXT("SaltboundRoads"));
		Saves->HubBerth = Berth;
		const bool bSaved = Saves->SaveGame(GetWorld(), TEXT("test"));
		Saves->SetSalt(0);
		Saves->ClearUnlocked();
		Saves->HubBerth = TEXT("Nowhere");
		const bool bLoaded = Saves->LoadGame(GetWorld());
		const int32 Salt = Saves->GetSalt();
		const bool bZones = Saves->GetUnlocked().Num() == 2 && Saves->IsUnlocked(TEXT("L5")) && Saves->IsUnlocked(TEXT("L9"));
		const FName Loaded = Saves->HubBerth;
		UGameplayStatics::DeleteGameInSlot(Saves->Slot, 0);
		Saves->Slot = Own;
		Saves->SetSalt(OwnSalt);
		Saves->ClearUnlocked();
		for (const FName& Z : OwnUnlocked)
		{
			Saves->SetUnlocked(Z, true);
		}
		Report(TEXT("HubSave"), bSaved && bLoaded && Salt == 4321 && bZones && Loaded == Berth && (!Ship || Ship->GetBerthId() == Berth), FString::Printf(
			TEXT("saved=%d loaded=%d salt=%d zones=%d berth=%s shipAt=%s"), bSaved ? 1 : 0, bLoaded ? 1 : 0, Salt, bZones ? 1 : 0, *Loaded.ToString(),
			Ship ? *Ship->GetBerthId().ToString() : TEXT("-")));
	}, nullptr, nullptr});

	// HubLadder: from the water, take hold of a quay ladder and climb on to the quay ---------------------------------------
	Steps.Add({TEXT("HubLadderSwim"), 1.5f, [this, S, Face, Input]()
	{
		ASaltHubLadder* Best = nullptr;
		double BestX = -1.0e9;
		const ASaltHubTestData* D = S->Data.Get();
		for (TActorIterator<ASaltHubLadder> It(GetWorld()); It && D; ++It)
		{
			const FVector L = D->ToLocal(It->GetActorLocation());
			if (L.Y > 0.0 && L.X > BestX)                                // the starboard quay's, nearest the fitting berth
			{
				BestX = L.X;
				Best = *It;
			}
		}
		S->Ladder = Best;
		S->bFlag = false;
		S->bDone = false;
		S->T = 0.f;
		ASaltPlayerCharacter* P = GetPlayer();
		if (Best && P)
		{
			if (P->IsAtStation())
			{
				P->LeaveStation();
			}
			const float Yaw = (float)FRotator::NormalizeAxis(Best->GetActorRotation().Yaw - 90.0);   // facing the wall (its -Y)
			P->TeleportFeetTo(Best->GetActorTransform().TransformPosition(FVector(0.0, 110.0, -200.0)), Yaw);
			Face(Yaw);
			Input(0.f);
		}
	}, nullptr, nullptr});
	Steps.Add({TEXT("HubLadderClimb"), 15.f, nullptr, [this, S, Input, EndStep](float Dt)
	{
		ASaltPlayerCharacter* P = GetPlayer();
		const ASaltHubLadder* L = S->Ladder.Get();
		if (!P || !L)
		{
			EndStep();
			return;
		}
		S->T += Dt;
		S->bFlag |= P->IsOnLadder();
		const FVector Local = L->GetActorTransform().InverseTransformPosition(FeetOf(P));
		if (S->bFlag && !P->IsOnLadder() && !P->IsSwimming() && FMath::Abs(Local.Z) < 30.0 && Local.Y < -20.0)
		{
			S->bDone = true;
			Input(0.f);
			EndStep();
			return;
		}
		Input(1.f);
	}, [this, S, Input]()
	{
		Input(0.f);
		const ASaltPlayerCharacter* P = GetPlayer();
		const ASaltHubLadder* L = S->Ladder.Get();
		const FVector Local = P && L ? L->GetActorTransform().InverseTransformPosition(FeetOf(P)) : FVector::ZeroVector;
		Report(TEXT("HubLadder"), S->bFlag && S->bDone, FString::Printf(TEXT("ladder=%s tookHold=%d onQuay=%d time=%.1fs feet=(%.0f, %.0f, %.0f)"),
			*GetNameSafe(L), S->bFlag ? 1 : 0, S->bDone ? 1 : 0, S->T, Local.X, Local.Y, Local.Z));
	}});

	// Finish: everything back as it was -------------------------------------------------------------------------------------
	Steps.Add({TEXT("HubFinish"), 0.f, [this, S, Input, Rig]()
	{
		Input(0.f);
		ASaltPlayerCharacter* P = GetPlayer();
		if (P && P->IsAtStation())
		{
			P->LeaveStation();
		}
		if (ASaltBoat* B = S->Boat.Get())
		{
			B->SetControlOverride(false);
			B->SetAutopilot(false);
			B->ResetToStart();
		}
		if (USaltWindSubsystem* W = GetWorld()->GetSubsystem<USaltWindSubsystem>(); W && S->OwnWind.Y > 0.f)
		{
			W->SetBaseWind(S->OwnWind.X, S->OwnWind.Y, S->OwnWind.Z);
		}
		if (ASaltLightingRig* R = Rig(); R && !S->OwnSea.IsNone())
		{
			R->SetSeaState(S->OwnSea, 0.f);
		}
		if (ASaltHubHoist* H = S->Hoist.Get())
		{
			H->SetStopNow(0);
		}
		if (P)
		{
			P->TeleportFeetTo(S->OwnFeet, S->OwnYaw);
		}
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST HubSuite DONE passed=%d failed=%d"), Passed, Failed);
	}, nullptr, nullptr});

	// A part on its own: its steps, the set-up and the finish.
	if (!Only.IsEmpty())
	{
		const FString Part = TEXT("Hub") + Only;
		Steps.RemoveAll([&Part](const FStep& Step)
		{
			return !Step.Name.StartsWith(Part, ESearchCase::IgnoreCase) && !Step.Name.StartsWith(TEXT("HubSetup")) && Step.Name != TEXT("HubFinish");
		});
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST HubSuite only '%s': %d steps"), *Only, Steps.Num());
	}
	StepIndex = 0;
	StepTime = 0.f;
	bStepStarted = false;
#endif
}

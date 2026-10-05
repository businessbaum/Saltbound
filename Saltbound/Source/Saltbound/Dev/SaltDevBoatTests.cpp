// Saltbound - development: automated sailing tests (sb.Dev.RunBoatTests, D36).

#include "Dev/SaltDevTestSubsystem.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "CableComponent.h"
#include "Containers/Ticker.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/SaltContent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Interaction/SaltStationComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Player/SaltPlayerCharacter.h"
#include "Props/SaltSwingComponent.h"
#include "Saltbound.h"
#include "World/SaltLightingRig.h"
#include "World/SaltWindSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdRunBoatTests(
		TEXT("sb.Dev.RunBoatTests"),
		TEXT("Run the sailing tests (needs a SaltBoat in the map): buoyancy, roll, points of sail, turning, collision, grounding, player aboard, helm, mooring. sb.Dev.RunBoatTests ladder: the ladders alone."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = World ? World->GetSubsystem<USaltDevTestSubsystem>() : nullptr)
			{
				T->RunBoatSuite(Args.Num() > 0 ? Args[0] : FString());
			}
		}));

	/** D56: the camera's height in the boat's frame every frame. Standing still on deck it should stay put however the
	 *  boat heaves; jitter there is the camera "bugging up and down" in big waves. sb.Dev.CameraTrace [seconds=10] [deck]
	 *  (deck: stand the player on the middle of the deck first) logs SB_CAM lines. */
	FAutoConsoleCommandWithWorldAndArgs CmdCameraTrace(
		TEXT("sb.Dev.CameraTrace"),
		TEXT("Log the camera's height in the boat's frame every frame: sb.Dev.CameraTrace [seconds=10] [deck]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); World && It; ++It) { Boat = *It; break; }
			if (!P || !Boat)
			{
				return;
			}
			if (Args.Num() > 1 && Args[1] == TEXT("deck"))
			{
				const FVector Local(0.f, 0.f, (SaltHull::SheerZ(0.f) + 0.05f) * 100.f);
				P->TeleportFeetTo(Boat->GetActorTransform().TransformPosition(Local), Boat->GetYawDeg());
			}
			const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.f;
			TWeakObjectPtr<ASaltPlayerCharacter> WeakP = P;
			TWeakObjectPtr<ASaltBoat> WeakBoat = Boat;
			TSharedRef<float> Clock = MakeShared<float>(0.f);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakP, WeakBoat, Clock, Seconds](float Dt) -> bool
			{
				ASaltPlayerCharacter* Pl = WeakP.Get();
				ASaltBoat* B = WeakBoat.Get();
				if (!Pl || !B)
				{
					return false;
				}
				*Clock += Dt;
				const FTransform Xf = B->GetActorTransform();
				// The view as rendered (after CalcCamera), else the camera component.
				const APlayerController* Ctl = Cast<APlayerController>(Pl->GetController());
				const FVector Cam = Ctl && Ctl->PlayerCameraManager ? Ctl->PlayerCameraManager->GetCameraLocation() : Pl->GetCamera()->GetComponentLocation();
				const UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(Pl->GetMovementBaseObject());
				const FVector Wd = B->GetKinematicAngularVelocity();
				UE_LOG(LogSaltbound, Display, TEXT("SB_CAM t=%.4f dt=%.4f camZ=%.2f camBoatZ=%.3f capBoatZ=%.3f boatZ=%.2f onBoat=%d vz=%.3f w=(%.2f,%.2f,%.2f) heel=%.2f pitch=%.2f contacts=%.1f"),
					*Clock, Dt, Cam.Z, Xf.InverseTransformPosition(Cam).Z, Xf.InverseTransformPosition(Pl->GetActorLocation()).Z, Xf.GetLocation().Z,
					Base && Base->GetOwner() == B ? 1 : 0, B->GetVelocity().Z, Wd.X, Wd.Y, Wd.Z, B->GetHeelDeg(), B->GetPitchDeg(), B->GetContactDepth());
				return *Clock < Seconds;
			}));
		}));

	/** D58: take the steering oar (the helm) for captures of its controls row: sb.Dev.Helm [off]. */
	FAutoConsoleCommandWithWorldAndArgs CmdHelm(
		TEXT("sb.Dev.Helm"),
		TEXT("Take the boat's helm (steering oar): sb.Dev.Helm [off]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); World && It; ++It) { Boat = *It; break; }
			if (!P || !Boat)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("off"))
			{
				P->LeaveStation();
				return;
			}
			P->EnterStation(Boat->GetHelm());
		}));

	/** D58 (owner: "if my character is on an edge of a physical object on the ship and the ship moves up and down because
	 *  of the waves, my camera sometimes starts bugging"): finds the low steps on the deck (3-25 cm: the chest's and the
	 *  rails' feet), stands the player beside them one spot after another while the boat lies at anchor in the open sea,
	 *  and measures the view against the boat: its largest jump from one frame to the next, its jerk (the second difference,
	 *  rms), and how often the body lost and found the floor. sb.Dev.EdgeTest [seconds per spot = 4] logs SB_TEST
	 *  DeckEdgeSteady lines and SB_TEST EdgeSuite DONE; the boat goes back to its mooring after. */
	FAutoConsoleCommandWithWorldAndArgs CmdEdgeTest(
		TEXT("sb.Dev.EdgeTest"),
		TEXT("Stand beside the low steps on the deck in open water and measure the view's jumps: sb.Dev.EdgeTest [seconds per spot=4]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); World && It; ++It) { Boat = *It; break; }
			if (!P || !Boat || !Boat->Hull)
			{
				UE_LOG(LogSaltbound, Display, TEXT("SB_TEST EdgeSuite DONE passed=0 failed=1 (no player or boat)"));
				return;
			}
			struct FSpot { FString Name; FVector Feet; float Top = 0.f; };
			struct FState
			{
				TWeakObjectPtr<ASaltPlayerCharacter> Player;
				TWeakObjectPtr<ASaltBoat> Boat;
				TArray<FSpot> Spots;
				int32 Spot = -2;
				float T = 0.f;
				float Seconds = 4.f;
				TArray<float> Cam;
				TArray<float> Cap;
				int32 Flips = 0;
				int32 OnTop = 0;
				bool bLastGround = true;
				float MaxStepOffset = 0.f;
				int32 Passed = 0;
				int32 Failed = 0;
			};
			TSharedRef<FState> S = MakeShared<FState>();
			S->Player = P;
			S->Boat = Boat;
			S->Seconds = Args.Num() > 0 ? FMath::Max(1.f, FCString::Atof(*Args[0])) : 4.f;
			// Open water, at anchor (it holds place and heading), in the rig's sea (Rough by default).
			Boat->PlaceAt(FVector(250000.f, -80000.f, 0.f), 0.f);
			Boat->SetHold(ESaltBoatHold::Anchored);
			TWeakObjectPtr<UWorld> WeakWorld = World;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S, WeakWorld](float Dt) -> bool
			{
				ASaltPlayerCharacter* Pl = S->Player.Get();
				ASaltBoat* B = S->Boat.Get();
				UWorld* W = WeakWorld.Get();
				if (!Pl || !B || !W)
				{
					UE_LOG(LogSaltbound, Display, TEXT("SB_TEST EdgeSuite DONE passed=0 failed=1 (aborted)"));
					return false;
				}
				S->T += Dt;
				const FTransform Xf = B->Hull->GetComponentTransform();
				if (S->Spot == -2)
				{
					// Let the boat settle at anchor, then find the spots: the deck's floor on a 5 cm grid, and the places beside a
					// low step (3-25 cm, within the player's step height, inside the capsule's reach) with the highest steps.
					if (S->T < 4.f)
					{
						return true;
					}
					FCollisionQueryParams Q(SCENE_QUERY_STAT(SaltEdgeTest), false, Pl);
					constexpr int32 NX = 183;
					constexpr int32 NY = 69;
					constexpr float Cell = 5.f;
					TArray<float> Floor;
					Floor.Init(TNumericLimits<float>::Lowest(), NX * NY);
					auto CellAt = [&](int32 IX, int32 IY) { return FVector2D(-455.f + IX * Cell, -170.f + IY * Cell); };
					for (int32 IX = 0; IX < NX; ++IX)
					{
						for (int32 IY = 0; IY < NY; ++IY)
						{
							const FVector2D C = CellAt(IX, IY);
							const float Deck = SaltHull::SheerZ(C.X * 0.01f) * 100.f;
							FHitResult Hit;
							if (W->LineTraceSingleByChannel(Hit, Xf.TransformPosition(FVector(C.X, C.Y, Deck + 250.f)), Xf.TransformPosition(FVector(C.X, C.Y, Deck - 40.f)),
								ECC_Pawn, Q) && Hit.GetActor() == B)
							{
								Floor[IX * NY + IY] = (float)Xf.InverseTransformPosition(Hit.ImpactPoint).Z;
							}
						}
					}
					struct FStep { FVector2D At; float Z = 0.f; float Height = 0.f; };
					TArray<FStep> Steps;
					constexpr int32 Reach = 5;
					for (int32 IX = Reach; IX < NX - Reach; ++IX)
					{
						for (int32 IY = Reach; IY < NY - Reach; ++IY)
						{
							const float Z = Floor[IX * NY + IY];
							if (Z == TNumericLimits<float>::Lowest())
							{
								continue;
							}
							float Lo = Z, Hi = Z;
							int32 N = 0;
							for (int32 DX = -Reach; DX <= Reach; ++DX)
							{
								for (int32 DY = -Reach; DY <= Reach; ++DY)
								{
									const float V = Floor[(IX + DX) * NY + IY + DY];
									if (DX * DX + DY * DY <= Reach * Reach && V != TNumericLimits<float>::Lowest())
									{
										Lo = FMath::Min(Lo, V);
										Hi = FMath::Max(Hi, V);
										++N;
									}
								}
							}
							if (N >= 60 && Hi - Lo >= 3.f && Hi - Lo <= 25.f && Z - Lo < 1.f)
							{
								Steps.Add({ CellAt(IX, IY), Z, Hi - Lo });
							}
						}
					}
					Steps.Sort([](const FStep& A, const FStep& Bs) { return A.Height > Bs.Height; });
					TArray<FStep> Picked;
					for (const FStep& St : Steps)
					{
						if (Picked.Num() < 6 && !Picked.ContainsByPredicate([&St](const FStep& Pk) { return FVector2D::Distance(Pk.At, St.At) < 30.f; }))
						{
							Picked.Add(St);
						}
					}
					// Each spot and four around it (the step's edge lies somewhere under the capsule's rim).
					for (const FStep& St : Picked)
					{
						for (const FVector2D Off : { FVector2D(0.f, 0.f), FVector2D(8.f, 0.f), FVector2D(-8.f, 0.f), FVector2D(0.f, 8.f), FVector2D(0.f, -8.f) })
						{
							S->Spots.Add({ FString::Printf(TEXT("(%.0f, %.0f) step %.0f cm"), St.At.X + Off.X, St.At.Y + Off.Y, St.Height),
								FVector(St.At.X + Off.X, St.At.Y + Off.Y, St.Z + 2.f), St.Z });
						}
					}
					UE_LOG(LogSaltbound, Display, TEXT("SB_EDGE START low steps=%d spots=%d %s"), Steps.Num(), S->Spots.Num(), *B->Describe());
					S->Spot = -1;
					S->T = 100.f;
				}
				const float Settle = 1.f;
				if (S->Spot >= 0 && S->T >= Settle)
				{
					// Recording: the view as rendered and the body, in the boat's frame.
					const APlayerController* Ctl = Cast<APlayerController>(Pl->GetController());
					const FVector View = Ctl && Ctl->PlayerCameraManager ? Ctl->PlayerCameraManager->GetCameraLocation() : Pl->GetCamera()->GetComponentLocation();
					S->Cam.Add((float)Xf.InverseTransformPosition(View).Z);
					const float CapZ = (float)Xf.InverseTransformPosition(Pl->GetActorLocation()).Z;
					S->Cap.Add(CapZ);
					const bool bGround = Pl->GetCharacterMovement()->IsMovingOnGround();
					S->Flips += bGround != S->bLastGround ? 1 : 0;
					S->bLastGround = bGround;
					S->OnTop += CapZ - Pl->GetCapsuleHalfHeightForClimb() > S->Spots[S->Spot].Top - 15.f ? 1 : 0;
					S->MaxStepOffset = FMath::Max(S->MaxStepOffset, FMath::Abs(Pl->GetEyeStepOffset()));
				}
				if (S->T < Settle + S->Seconds)
				{
					return true;
				}
				// The spot is done: its numbers, then the next spot.
				if (S->Spot >= 0 && S->Cam.Num() > 3)
				{
					float MaxStep = 0.f, MaxCapStep = 0.f, Jerk = 0.f, MaxJerk = 0.f;
					for (int32 I = 1; I < S->Cam.Num(); ++I)
					{
						MaxStep = FMath::Max(MaxStep, FMath::Abs(S->Cam[I] - S->Cam[I - 1]));
						MaxCapStep = FMath::Max(MaxCapStep, FMath::Abs(S->Cap[I] - S->Cap[I - 1]));
						if (I + 1 < S->Cam.Num())
						{
							const float J = S->Cam[I + 1] - 2.f * S->Cam[I] + S->Cam[I - 1];
							Jerk += J * J;
							MaxJerk = FMath::Max(MaxJerk, FMath::Abs(J));
						}
					}
					Jerk = FMath::Sqrt(Jerk / FMath::Max(S->Cam.Num() - 2, 1));
					// Steady: the view never jumps more than 4 cm against the deck from one frame to the next (it was 15-25 cm
					// before D58; the deck's own motion is about 1 cm a frame). A fall off the boat (into the sea) is not a jump.
					const bool bAboard = S->Cap.Num() > 0 && S->Cap.Last() > 0.f;
					const bool bPass = !bAboard || (MaxStep < 4.f && Jerk < 0.6f);
					(bPass ? S->Passed : S->Failed)++;
					UE_LOG(LogSaltbound, Display, TEXT("SB_TEST DeckEdgeSteady %s spot=\"%s\" frames=%d camMaxStep=%.2f camJerkRms=%.3f camMaxJerk=%.2f capMaxStep=%.2f floorFlips=%d onTop=%.2f maxStepOffset=%.2f aboard=%d"),
						bPass ? TEXT("PASS") : TEXT("FAIL"), *S->Spots[S->Spot].Name, S->Cam.Num(), MaxStep, Jerk, MaxJerk, MaxCapStep, S->Flips, S->OnTop / (float)S->Cam.Num(),
						S->MaxStepOffset, bAboard ? 1 : 0);
				}
				++S->Spot;
				if (!S->Spots.IsValidIndex(S->Spot))
				{
					UE_LOG(LogSaltbound, Display, TEXT("SB_TEST EdgeSuite DONE passed=%d failed=%d spots=%d"), S->Passed, S->Failed, S->Spots.Num());
					B->ResetToStart();
					return false;
				}
				S->T = 0.f;
				S->Cam.Reset();
				S->Cap.Reset();
				S->Flips = 0;
				S->OnTop = 0;
				S->MaxStepOffset = 0.f;
				S->bLastGround = true;
				Pl->TeleportFeetTo(Xf.TransformPosition(S->Spots[S->Spot].Feet), B->GetYawDeg());
				return true;
			}));
		}));

	/** Open water south-east of the test cove (about 15 m deep), clear of the pier, rocks and headlands. */
	const FVector TestWater(7000.f, -2500.f, 0.f);

	float MeanOf(const TArray<FVector>& S, int32 Axis, int32 From = 0)
	{
		double Sum = 0.0;
		int32 N = 0;
		for (int32 I = From; I < S.Num(); ++I)
		{
			Sum += S[I][Axis];
			++N;
		}
		return N > 0 ? (float)(Sum / N) : 0.f;
	}

	float MaxAbsOf(const TArray<FVector>& S, int32 Axis)
	{
		float M = 0.f;
		for (const FVector& V : S)
		{
			M = FMath::Max(M, (float)FMath::Abs(V[Axis]));
		}
		return M;
	}
}
#endif

void USaltDevTestSubsystem::RunBoatSuite(const FString& Only)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	ASaltBoat* Boat = nullptr;
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		Boat = *It;
		break;
	}
	ASaltLightingRig* Rig = nullptr;
	for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
	{
		Rig = *It;
		break;
	}
	USaltWindSubsystem* Wind = World->GetSubsystem<USaltWindSubsystem>();
	if (!Boat || !Wind || !GetPlayer())
	{
		UE_LOG(LogSaltbound, Error, TEXT("SB_TEST BoatSuite FAIL reason=no_boat_wind_or_player"));
		return;
	}
	TestBoat = Boat;
	TWeakObjectPtr<ASaltLightingRig> WeakRig(Rig);
	Passed = Failed = 0;
	Steps.Reset();
	SavedWind = FVector(Wind->GetBaseFromDeg(), Wind->GetBaseSpeed(), Wind->GetGustiness());
	SavedSeaState = Rig ? Rig->CurrentSeaState : NAME_None;
	UE_LOG(LogSaltbound, Display, TEXT("SB_TEST BoatSuite START mass=%.0fkg"), Boat->GetMass());

	auto SetWind = [this](float FromDeg, float SpeedMs)
	{
		if (USaltWindSubsystem* W = GetWorld()->GetSubsystem<USaltWindSubsystem>())
		{
			W->SetBaseWind(FromDeg, SpeedMs * 100.f, 0.f);
		}
	};
	auto Place = [this](const FVector& At, float Yaw, float SpeedMs, bool bSail, bool bAutopilot)
	{
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetHold(ESaltBoatHold::Free);
			Bt->PlaceAt(At, Yaw, SpeedMs * 100.f);
			Bt->SetSailSet(bSail);
			Bt->SetControlOverride(true, 0.f);
			Bt->SetAutopilot(bAutopilot, Yaw);
		}
	};
	auto Wait = [this](const FString& Name, float Seconds)
	{
		Steps.Add({Name, Seconds, nullptr, nullptr, nullptr});
	};
	auto FeetInBoat = [this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		const ASaltBoat* Bt = TestBoat.Get();
		if (!P || !Bt)
		{
			return FVector::ZeroVector;
		}
		return Bt->GetActorTransform().InverseTransformPosition(P->GetActorLocation() - FVector(0.f, 0.f, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	};
	auto EyeInBoat = [this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		const ASaltBoat* Bt = TestBoat.Get();
		return (P && Bt) ? Bt->GetActorTransform().InverseTransformPosition(P->GetCamera()->GetComponentLocation()) : FVector::ZeroVector;
	};
	auto OnBoat = [this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		const UPrimitiveComponent* Base = P ? Cast<UPrimitiveComponent>(P->GetCharacterMovement()->GetMovementBaseObject()) : nullptr;
		return Base && Base->GetOwner() == TestBoat.Get() && P->GetCharacterMovement()->IsMovingOnGround();
	};
	auto PlacePlayerAboard = [this](float X, float Y, float RelYaw)
	{
		ASaltPlayerCharacter* P = GetPlayer();
		ASaltBoat* Bt = TestBoat.Get();
		if (P && Bt)
		{
			const FVector Local(X * 100.f, Y * 100.f, (SaltHull::SheerZ(X) + 0.05f) * 100.f);
			P->TeleportFeetTo(Bt->GetActorTransform().TransformPosition(Local), Bt->GetYawDeg() + RelYaw);
		}
	};

	Steps.Add({TEXT("BoatSetup"), 0.f, [WeakRig]()
	{
		if (ASaltLightingRig* R = WeakRig.Get())
		{
			R->SetSeaState(TEXT("Flat"), 0.f);
		}
	}, nullptr, nullptr});

	// 1) Floating at rest in calm water: at the design waterline, level, no drift.
	Steps.Add({TEXT("FloatSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(TestWater, 90.f, 0.f, false, false); }, nullptr, nullptr});
	Wait(TEXT("FloatSettle"), 6.f);
	Steps.Add({TEXT("Float"), 3.f, [this]() { Samples.Reset(); },
		[this](float)
		{
			if (const ASaltBoat* Bt = TestBoat.Get())
			{
				Samples.Add(FVector(Bt->GetWaterlineOffset(), Bt->GetHeelDeg(), Bt->GetPitchDeg()));
			}
		},
		[this]()
		{
			const ASaltBoat* Bt = TestBoat.Get();
			const float Wl = MaxAbsOf(Samples, 0), Heel = MaxAbsOf(Samples, 1), Pitch = MaxAbsOf(Samples, 2);
			const float Drift = Bt ? Bt->GetSpeedKnots() : 99.f;
			Report(TEXT("BoatFloats"), Wl < 4.f && Heel < 1.f && Pitch < 1.f && Drift < 0.2f, FString::Printf(
				TEXT("waterline=%.2fcm heel=%.2fdeg pitch=%.2fdeg drift=%.2fkn mass=%.0fkg"), Wl, Heel, Pitch, Drift, Bt ? Bt->GetMass() : 0.f));
		}});

	// 2) Roll period and damping after a sideways kick.
	Steps.Add({TEXT("RollKick"), 10.f,
		[this]()
		{
			Samples.Reset();
			if (ASaltBoat* Bt = TestBoat.Get())
			{
				Bt->DebugKick(FVector::ZeroVector, Bt->GetActorForwardVector() * 0.25);
			}
		},
		[this](float)
		{
			if (const ASaltBoat* Bt = TestBoat.Get())
			{
				Samples.Add(FVector(StepTime, Bt->GetHeelDeg(), 0.f));
			}
		},
		[this]()
		{
			TArray<float> Crossings;
			TArray<float> Peaks;
			float Peak = 0.f;
			for (int32 I = 1; I < Samples.Num(); ++I)
			{
				const FVector& A = Samples[I - 1];
				const FVector& B = Samples[I];
				Peak = FMath::Max(Peak, (float)FMath::Abs(B.Y));
				if ((A.Y < 0.f) != (B.Y < 0.f) && A.X > 0.2f)
				{
					Crossings.Add(FMath::Lerp(A.X, B.X, A.Y / (A.Y - B.Y)));
					Peaks.Add(Peak);
					Peak = 0.f;
				}
			}
			const float Period = Crossings.Num() >= 3 ? 2.f * (Crossings.Last() - Crossings[0]) / (Crossings.Num() - 1) : 0.f;
			const float Decay = Peaks.Num() >= 3 && Peaks[1] > 0.f ? Peaks[2] / Peaks[1] : 0.f;
			Report(TEXT("BoatRollPeriod"), Period > 2.f && Period < 5.5f, FString::Printf(TEXT("period=%.2fs firstSwing=%.2fdeg decayPerHalfSwing=%.2f crossings=%d"),
				Period, Peaks.Num() > 1 ? Peaks[1] : 0.f, Decay, Crossings.Num()));
		}});

	// 3) Simple sailing (D41) in a steady 7 m/s (13.6 kn) wind, the autopilot holding a heading of 90 deg (+Y): the set
	// sail drives the boat whatever the wind's angle (the wind only shades the speed a little).
	struct FPoint { const TCHAR* Name; float WindFrom; float MinKn; float MaxKn; };
	const FPoint Points[] = {
		{ TEXT("WindAbeam"), 0.f, 6.f, 9.5f },        // 90 deg on the port side
		{ TEXT("WindAhead"), 80.f, 4.5f, 9.f },       // 10 deg off the bow
		{ TEXT("WindAstern"), 275.f, 5.5f, 9.5f },    // 175 deg
	};
	for (const FPoint& Pt : Points)
	{
		const FString Name(Pt.Name);
		Steps.Add({Name + TEXT("Setup"), 0.f, [=, this]() { SetWind(Pt.WindFrom, 7.f); Place(TestWater, 90.f, 0.f, true, true); }, nullptr, nullptr});
		Wait(Name + TEXT("Build"), 25.f);
		Steps.Add({Name, 6.f, [this]() { Samples.Reset(); MaxValue = 0.f; },
			[this](float)
			{
				if (const ASaltBoat* Bt = TestBoat.Get())
				{
					const float Leeway = Bt->GetSpeedKnots() > 1.f ? FRotator::NormalizeAxis(Bt->GetCourseDeg() - Bt->GetYawDeg()) : 0.f;
					Samples.Add(FVector(Bt->GetSpeedKnots(), FMath::Abs(Bt->GetHeelDeg()), FMath::Abs(Leeway)));
					MaxValue = FMath::Max(MaxValue, FMath::Abs(FRotator::NormalizeAxis(Bt->GetYawDeg() - 90.f)));
				}
			},
			[this, Name, Pt]()
			{
				const float Speed = MeanOf(Samples, 0), Heel = MeanOf(Samples, 1), Leeway = MeanOf(Samples, 2);
				Report(TEXT("Sail") + Name, Speed >= Pt.MinKn && Speed <= Pt.MaxKn && Heel < 20.f && Leeway < 10.f, FString::Printf(
					TEXT("speed=%.2fkn heel=%.1fdeg leeway=%.1fdeg headingError=%.1fdeg (from rest in 25 s)"), Speed, Heel, Leeway, MaxValue));
			}});
	}
	// The sea draws the boat's wake (D44, D45): it gets the speed and a trail of stern positions a second apart.
	Steps.Add({TEXT("WakeTrail"), 0.f, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		UMaterialParameterCollection* Mpc = LoadObject<UMaterialParameterCollection>(nullptr, SaltContent::OceanParameterCollection);
		if (!Bt || !Mpc)
		{
			Report(TEXT("BoatWakeTrail"), false, TEXT("no boat or MPC_Ocean"));
			return;
		}
		auto Get = [this, Mpc](const TCHAR* Name) { return UKismetMaterialLibrary::GetVectorParameterValue(GetWorld(), Mpc, FName(Name)); };
		const FLinearColor Wake = Get(TEXT("BoatWake"));
		const FLinearColor P0 = Get(TEXT("BoatTrailP0"));
		const FLinearColor P1 = Get(TEXT("BoatTrailP1"));
		const FLinearColor S0 = Get(TEXT("BoatTrailS0"));
		const float Speed = Bt->GetSpeedKnots() / 1.943844f;
		// Trail points 1 and 2 are consecutive samples, one second apart.
		const float Spacing = (float)FVector2D::Distance(FVector2D(P0.B, P0.A), FVector2D(P1.R, P1.G)) * 0.01f;
		const float Extent = Get(TEXT("BoatWakeExtent")).R;
		Report(TEXT("BoatWakeTrail"), Wake.A > 0.5f && FMath::Abs(Wake.R - Speed) < 0.5f && S0.R > 0.9f && S0.G > 0.9f
			&& FMath::Abs(Spacing - Speed) < Speed * 0.25f && Extent > 35.f && Extent <= 40.f, FString::Printf(
			TEXT("wakeSpeed=%.2fm/s boatSpeed=%.2fm/s on=%.0f strengthNow=%.2f strength1=%.2f sampleSpacing=%.2fm wakeExtent=%.1fm"),
			Wake.R, Speed, Wake.A, S0.R, S0.G, Spacing, Extent));
	}, nullptr, nullptr});
	// Taking the sail in slows the boat down.
	Steps.Add({TEXT("TakeInSail"), 20.f, [this]()
	{
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			MaxValue = Bt->GetSpeedKnots();
			Bt->SetSailSet(false);
		}
	}, nullptr, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		const float Speed = Bt ? Bt->GetSpeedKnots() : 99.f;
		Report(TEXT("SailTakenInSlows"), Speed < 2.5f && Speed < MaxValue * 0.4f, FString::Printf(TEXT("before=%.2fkn after20s=%.2fkn"), MaxValue, Speed));
	}});

	// 3b) Dropping the anchor under full sail and full rudder (D43): the boat comes to a smooth stop and stops turning.
	Steps.Add({TEXT("AnchorSetup"), 0.f, [=, this]() { SetWind(0.f, 7.f); Place(TestWater, 90.f, 3.f, true, true); }, nullptr, nullptr});
	Wait(TEXT("AnchorBuild"), 15.f);
	Steps.Add({TEXT("AnchorStop"), 14.f, [this]()
	{
		Samples.Reset();
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetAutopilot(false);
			Bt->SetControlOverride(true, 1.f);
			Bt->SetHold(ESaltBoatHold::Anchored);
		}
	},
	[this](float)
	{
		if (const ASaltBoat* Bt = TestBoat.Get())
		{
			Samples.Add(FVector(StepTime, Bt->GetSpeedKnots() / 1.943844f, Bt->GetYawRateDeg()));
		}
	},
	[this]()
	{
		// Smooth: the speed never falls faster than 2.5 m/s2 (over a quarter of a second); at the end it lies still.
		float MaxDecel = 0.f;
		int32 J = 0;
		for (int32 I = 1; I < Samples.Num(); ++I)
		{
			while (J + 1 < I && Samples[I].X - Samples[J + 1].X >= 0.25f)
			{
				++J;
			}
			const float Dt = Samples[I].X - Samples[J].X;
			if (Dt >= 0.2f)
			{
				MaxDecel = FMath::Max(MaxDecel, (float)(Samples[J].Y - Samples[I].Y) / Dt);
			}
		}
		float EndRate = 0.f;
		for (int32 I = Samples.Num() * 4 / 5; I < Samples.Num(); ++I)
		{
			EndRate = FMath::Max(EndRate, (float)FMath::Abs(Samples[I].Z));
		}
		const float Before = Samples.Num() > 0 ? Samples[0].Y : 0.f;
		const float EndSpeed = MeanOf(Samples, 1, Samples.Num() * 4 / 5);
		const ASaltBoat* Bt = TestBoat.Get();
		const bool bSail = Bt && Bt->IsSailSet();
		Report(TEXT("AnchorStopsBoat"), Before > 2.f && EndSpeed < 0.15f && EndRate < 0.5f && MaxDecel < 2.5f && bSail, FString::Printf(
			TEXT("before=%.2fm/s end=%.3fm/s endYawRate=%.2fdeg/s maxDecel=%.2fm/s2 sailSet=%d hold=%.2f"),
			Before, EndSpeed, EndRate, MaxDecel, bSail ? 1 : 0, Bt ? Bt->GetAnchorHold() : 0.f));
	}});

	// 4) Turning circle: beam reach at speed, then full starboard rudder.
	Steps.Add({TEXT("TurnSetup"), 0.f, [=, this]() { SetWind(0.f, 7.f); Place(TestWater, 90.f, 3.f, true, true); }, nullptr, nullptr});
	// A boat that has just set off has no wake behind the point it started from (owner, D48).
	Steps.Add({TEXT("WakeFresh"), 0.5f, nullptr, nullptr, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		const float Extent = Bt ? Bt->GetWakeExtent() : 99.f;
		Report(TEXT("BoatWakeStartsAtBoat"), Extent < 3.f, FString::Printf(TEXT("wakeExtent=%.2fm after 0.5 s at 3 m/s"), Extent));
	}});
	Wait(TEXT("TurnBuild"), 15.f);
	Steps.Add({TEXT("TurnRudder"), 3.f, [this]()
	{
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetAutopilot(false);
			Bt->SetControlOverride(true, 1.f);
		}
	}, nullptr, nullptr});
	Steps.Add({TEXT("Turn"), 6.f, [this]() { Samples.Reset(); },
		[this](float)
		{
			if (const ASaltBoat* Bt = TestBoat.Get())
			{
				Samples.Add(FVector(Bt->GetSpeedKnots() / 1.943844f, Bt->GetYawRateDeg(), 0.f));
			}
		},
		[this]()
		{
			const float Speed = MeanOf(Samples, 0), Rate = MeanOf(Samples, 1);
			const float Radius = Rate > 0.1f ? Speed / FMath::DegreesToRadians(Rate) : 999.f;
			Report(TEXT("BoatTurns"), Rate > 5.f && Radius < 50.f, FString::Printf(TEXT("yawRate=%.1fdeg/s speed=%.2fm/s radius=%.1fm"), Rate, Speed, Radius));
		}});

	// 5) Some steerage at rest (sculling): no wind, sail down, full rudder.
	Steps.Add({TEXT("RestSetup"), 0.f, [=, this]()
	{
		SetWind(0.f, 0.f);
		Place(TestWater, 90.f, 0.f, false, false);
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetControlOverride(true, 1.f);
		}
	}, nullptr, nullptr});
	Wait(TEXT("RestBuild"), 4.f);
	Steps.Add({TEXT("RestTurn"), 3.f, [this]() { Samples.Reset(); },
		[this](float) { if (const ASaltBoat* Bt = TestBoat.Get()) { Samples.Add(FVector(Bt->GetYawRateDeg(), 0.f, 0.f)); } },
		[this]()
		{
			const float Rate = MeanOf(Samples, 0);
			Report(TEXT("BoatTurnsAtRest"), Rate > 1.f && Rate < 12.f, FString::Printf(TEXT("yawRate=%.2fdeg/s"), Rate));
		}});

	// 6) Running into the pier end at 2.5 m/s (sail down): stops at the posts, no tunnelling.
	Steps.Add({TEXT("PierSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(FVector(2700.f, 0.f, 0.f), 180.f, 2.5f, false, false); }, nullptr, nullptr});
	Steps.Add({TEXT("PierImpact"), 8.f, [this]() { MinValue = 1e9f; MaxValue = 0.f; },
		[this](float)
		{
			if (const ASaltBoat* Bt = TestBoat.Get())
			{
				MinValue = FMath::Min(MinValue, (float)Bt->GetActorLocation().X);
				MaxValue = FMath::Max(MaxValue, Bt->GetContactDepth());
			}
		},
		[this]()
		{
			const ASaltBoat* Bt = TestBoat.Get();
			const float Speed = Bt ? Bt->GetSpeedKnots() / 1.943844f : 99.f;
			// The pier ends at X = 1400 cm; the stem is 5 m ahead of the origin.
			Report(TEXT("BoatStopsAtPier"), MinValue > 1780.f && MaxValue < 12.f && Speed < 0.5f, FString::Printf(
				TEXT("closestOriginX=%.0fcm stemGap=%.0fcm maxPenetration=%.1fcm finalSpeed=%.2fm/s"), MinValue, MinValue - 500.f - 1400.f, MaxValue, Speed));
		}});

	// 7) Running aground on the beach slope (sail down, 2 m/s): stops, stays on top of the sand.
	Steps.Add({TEXT("GroundSetup"), 0.f, [=, this]() { Place(FVector(300.f, -2200.f, 0.f), 180.f, 2.f, false, false); }, nullptr, nullptr});
	Steps.Add({TEXT("Grounding"), 12.f, [this]() { MaxValue = 0.f; },
		[this](float) { if (const ASaltBoat* Bt = TestBoat.Get()) { MaxValue = FMath::Max(MaxValue, Bt->GetContactDepth()); } },
		[this]()
		{
			const ASaltBoat* Bt = TestBoat.Get();
			const float Speed = Bt ? Bt->GetSpeedKnots() / 1.943844f : 99.f;
			const int32 Contacts = Bt ? Bt->GetContactCount() : 0;
			const float X = Bt ? (float)Bt->GetActorLocation().X : 0.f;
			Report(TEXT("BoatRunsAground"), Speed < 0.3f && MaxValue < 12.f && Contacts > 0, FString::Printf(
				TEXT("finalSpeed=%.2fm/s maxPenetration=%.1fcm contacts=%d originX=%.0fcm waterline=%.1fcm heel=%.1fdeg"),
				Speed, MaxValue, Contacts, X, Bt ? Bt->GetWaterlineOffset() : 0.f, Bt ? Bt->GetHeelDeg() : 0.f));
		}});

	// 8) Standing on deck while sailing a beam reach: no sliding, no jitter.
	Steps.Add({TEXT("AboardSetup"), 0.f, [=, this]() { SetWind(0.f, 7.f); Place(TestWater, 90.f, 3.f, true, true); }, nullptr, nullptr});
	Wait(TEXT("AboardBuild"), 15.f);
	Steps.Add({TEXT("AboardPlace"), 0.f, [=, this]() { PlacePlayerAboard(-1.0f, 0.45f, 0.f); }, nullptr, nullptr});
	Wait(TEXT("AboardSettle"), 1.5f);
	Steps.Add({TEXT("Aboard"), 8.f,
		[this]() { Feet.Reset(); Eye.Reset(); OffDeckFrames = 0; },
		[=, this](float Dt) { Feet.Add(FeetInBoat(), Dt); Eye.Add(EyeInBoat(), Dt); OffDeckFrames += OnBoat() ? 0 : 1; },
		[this]()
		{
			const ASaltBoat* Bt = TestBoat.Get();
			Report(TEXT("StandOnSailingBoat"), Feet.MaxDrift < 3.f && Eye.MaxAccelStep < 0.6f && OffDeckFrames == 0, FString::Printf(
				TEXT("feetDrift=%.2fcm eyeJitterMax=%.3fcm offDeckFrames=%d boatSpeed=%.1fkn heel=%.1fdeg"),
				Feet.MaxDrift, Eye.MaxAccelStep, OffDeckFrames, Bt ? Bt->GetSpeedKnots() : 0.f, Bt ? Bt->GetHeelDeg() : 0.f));
		}});

	// 9) Take the helm at sea, steer to starboard with the keys, leave the helm.
	Steps.Add({TEXT("HelmApproach"), 0.f, [=, this]() { PlacePlayerAboard(SaltHull::OarHelmsmanX - 0.1f, SaltHull::OarHelmsmanY, 0.f); }, nullptr, nullptr});
	Wait(TEXT("HelmSettle"), 1.f);
	Steps.Add({TEXT("HelmAim"), 0.4f, nullptr, [this](float)
	{
		ASaltPlayerCharacter* P = GetPlayer();
		const ASaltBoat* Bt = TestBoat.Get();
		if (P && Bt && P->GetController())
		{
			P->GetController()->SetControlRotation((Bt->GetHelm()->GetComponentLocation() - P->GetCamera()->GetComponentLocation()).Rotation());
		}
	}, nullptr});
	Steps.Add({TEXT("HelmEnter"), 1.f, [this]()
	{
		ASaltPlayerCharacter* P = GetPlayer();
		const ASaltBoat* Bt = TestBoat.Get();
		const bool bFocused = P && Bt && P->GetFocusedInteractable() == Bt->GetHelm();
		Report(TEXT("BoatHelmFocus"), bFocused, FString::Printf(TEXT("focused=%s"), *GetNameSafe(P ? P->GetFocusedInteractable() : nullptr)));
		if (P) { P->DebugInteract(); }
	}, nullptr, nullptr});
	Steps.Add({TEXT("HelmSteer"), 4.f,
		[this]()
		{
			if (ASaltBoat* Bt = TestBoat.Get())
			{
				Bt->SetAutopilot(false);
				Bt->SetControlOverride(false);
			}
			Samples.Reset();
			if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(FVector2D(1.f, 0.f)); }
		},
		[this](float) { if (const ASaltBoat* Bt = TestBoat.Get()) { Samples.Add(FVector(Bt->GetYawRateDeg(), Bt->GetRudderAngleDeg(), 0.f)); } },
		[this]()
		{
			ASaltPlayerCharacter* P = GetPlayer();
			if (P) { P->SetDebugMoveInput(FVector2D::ZeroVector); }
			const float Rate = MeanOf(Samples, 0, Samples.Num() / 2);
			const float Rudder = Samples.Num() > 0 ? Samples.Last().Y : 0.f;
			Report(TEXT("BoatHelmSteers"), P && P->IsAtStation() && Rate > 3.f && Rudder > 20.f, FString::Printf(
				TEXT("atHelm=%d yawRate=%.1fdeg/s rudder=%.1fdeg"), P && P->IsAtStation() ? 1 : 0, Rate, Rudder));
		}});
	Steps.Add({TEXT("HelmJump"), 0.5f, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		MaxValue = Bt && Bt->IsSailSet() ? 1.f : 0.f;
		if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugJump(true); P->DebugJump(false); P->DebugDive(true); P->DebugDive(false); }
	}, nullptr, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		const bool bSame = Bt && (Bt->IsSailSet() ? 1.f : 0.f) == MaxValue && Bt->GetHold() == ESaltBoatHold::Free;
		Report(TEXT("BoatHelmOnlySteers"), bSame, FString::Printf(TEXT("sailUnchanged=%d holdFree=%d"), bSame ? 1 : 0, Bt && Bt->GetHold() == ESaltBoatHold::Free ? 1 : 0));
	}});
	Steps.Add({TEXT("HelmLeave"), 1.5f, [this]() { if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugInteract(); } }, nullptr, [=, this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		Report(TEXT("BoatHelmLeave"), P && !P->IsAtStation() && OnBoat(), FString::Printf(TEXT("atHelm=%d onDeck=%d"), P && P->IsAtStation() ? 1 : 0, OnBoat() ? 1 : 0));
	}});

	// 9b) The sail rope by the starboard shrouds sets / takes in the sail; the windlass drops the anchor.
	auto UsePoint = [=, this](const FString& Name, TFunction<USaltInteractableComponent*()> Target, float StandX, float StandY,
		TFunction<bool()> Check, TFunction<FString()> State)
	{
		Steps.Add({Name + TEXT("Approach"), 0.f, [=, this]() { PlacePlayerAboard(StandX, StandY, 0.f); }, nullptr, nullptr});
		Wait(Name + TEXT("Settle"), 1.f);
		Steps.Add({Name + TEXT("Aim"), 0.4f, nullptr, [=, this](float)
		{
			ASaltPlayerCharacter* P = GetPlayer();
			USaltInteractableComponent* T = Target();
			if (P && T && P->GetController())
			{
				P->GetController()->SetControlRotation((T->GetComponentLocation() - P->GetCamera()->GetComponentLocation()).Rotation());
			}
		}, nullptr});
		Steps.Add({Name, 1.f, [=, this]()
		{
			ASaltPlayerCharacter* P = GetPlayer();
			const bool bFocused = P && P->GetFocusedInteractable() == Target();
			Report(Name + TEXT("Focus"), bFocused, FString::Printf(TEXT("focused=%s"), *GetNameSafe(P ? P->GetFocusedInteractable() : nullptr)));
			if (P) { P->DebugInteract(); }
		}, nullptr, [=, this]()
		{
			Report(Name, Check(), State());
		}});
	};
	Steps.Add({TEXT("PointsSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(TestWater, 90.f, 0.f, false, false); }, nullptr, nullptr});
	Wait(TEXT("PointsSettle"), 1.f);
	UsePoint(TEXT("BoatSailRope"), [this]() { return TestBoat.IsValid() ? TestBoat->GetSailRope(true) : nullptr; }, SaltHull::MastX + 0.35f, 0.8f,
		[this]() { return TestBoat.IsValid() && TestBoat->IsSailSet(); },
		[this]() { return FString::Printf(TEXT("sailSet=%d"), TestBoat.IsValid() && TestBoat->IsSailSet() ? 1 : 0); });
	UsePoint(TEXT("BoatWindlass"), [this]() { return TestBoat.IsValid() ? TestBoat->GetWindlass() : nullptr; }, 2.95f, 0.55f,
		[this]() { return TestBoat.IsValid() && TestBoat->GetHold() == ESaltBoatHold::Anchored; },
		[this]() { return FString::Printf(TEXT("hold=%d (2 = anchored)"), TestBoat.IsValid() ? (int32)TestBoat->GetHold() : -1); });
	Steps.Add({TEXT("PointsTidy"), 0.f, [this]() { if (ASaltBoat* Bt = TestBoat.Get()) { Bt->SetSailSet(false); Bt->SetHold(ESaltBoatHold::Free); } }, nullptr, nullptr});

	// 9d) D53: the gaff hangs from its halyards; the bell, its clapper and the lantern swing with the boat and settle;
	// the bell rope rings the bell, the player bumps it aside, the lantern lights and douses.
	Steps.Add({TEXT("RigSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(TestWater, 90.f, 0.f, false, false); }, nullptr, nullptr});
	Wait(TEXT("RigSettle"), 4.f);
	Steps.Add({TEXT("Halyards"), 0.f, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		bool bOk = Bt != nullptr;
		FString State;
		for (const bool bPeak : { false, true })
		{
			const UCableComponent* C = Bt ? Bt->GetHalyard(bPeak) : nullptr;
			const USceneComponent* End = Bt ? Bt->GetGaffHalyardPoint(bPeak) : nullptr;
			const float Dist = C && End ? (float)FVector::Dist(C->GetComponentLocation(), End->GetComponentLocation()) : 0.f;
			const bool bAttached = C && End && C->GetAttachedComponent() == End && C->IsVisible();
			const bool bTaut = C && C->CableLength >= Dist && C->CableLength < Dist + 5.f;
			const bool bReach = bPeak ? (Dist > 150.f && Dist < 260.f) : (Dist > 10.f && Dist < 45.f);
			bOk = bOk && bAttached && bTaut && bReach;
			State += FString::Printf(TEXT("%s: attached=%d dist=%.0fcm length=%.0fcm  "), bPeak ? TEXT("peak") : TEXT("throat"), bAttached ? 1 : 0, Dist,
				C ? C->CableLength : 0.f);
		}
		Report(TEXT("BoatHalyardsHoldGaff"), bOk, State);
	}, nullptr, nullptr});
	Steps.Add({TEXT("SwingRoll"), 2.5f, [this]()
	{
		MaxValue = MinValue = 0.f;
		if (ASaltBoat* Bt = TestBoat.Get()) { Bt->DebugKick(FVector::ZeroVector, Bt->GetActorQuat().RotateVector(FVector(0.35, 0.0, 0.0))); }
	}, [this](float)
	{
		if (const ASaltBoat* Bt = TestBoat.Get())
		{
			MaxValue = FMath::Max(MaxValue, Bt->GetBellSwing()->GetSwingAngle());
			MinValue = FMath::Max(MinValue, Bt->GetLanternSwing()->GetSwingAngle());
		}
	}, [this]()
	{
		Report(TEXT("BoatBellAndLanternSwing"), MaxValue > 2.f && MinValue > 2.f && MaxValue <= 30.5f && MinValue <= 35.5f,
			FString::Printf(TEXT("after a roll kick: bell=%.1fdeg lantern=%.1fdeg"), MaxValue, MinValue));
	}});
	Wait(TEXT("SwingDie"), 12.f);
	Steps.Add({TEXT("SwingSettled"), 0.f, [this]()
	{
		const ASaltBoat* Bt = TestBoat.Get();
		const float BellA = Bt ? Bt->GetBellSwing()->GetSwingAngle() : 99.f;
		const float LanternA = Bt ? Bt->GetLanternSwing()->GetSwingAngle() : 99.f;
		const float ClapperA = Bt ? Bt->GetClapperSwing()->GetSwingAngle() : 99.f;
		Report(TEXT("BoatSwingsSettle"), BellA < 1.f && LanternA < 1.f && ClapperA < 1.5f,
			FString::Printf(TEXT("bell=%.2fdeg lantern=%.2fdeg clapper=%.2fdeg"), BellA, LanternA, ClapperA));
	}, nullptr, nullptr});
	Steps.Add({TEXT("BellBaseline"), 0.f, [this]() { MaxValue = TestBoat.IsValid() ? (float)TestBoat->GetClapperSwing()->GetStrikeCount() : 0.f; },
		nullptr, nullptr});
	UsePoint(TEXT("BoatBellRope"), [this]() { return TestBoat.IsValid() ? TestBoat->GetBellRope() : nullptr; }, 2.15f, 0.5f,
		[this]() { return TestBoat.IsValid() && TestBoat->GetClapperSwing()->GetStrikeCount() > (int32)MaxValue; },
		[this]() { return FString::Printf(TEXT("strikes %d -> %d"), (int32)MaxValue, TestBoat.IsValid() ? TestBoat->GetClapperSwing()->GetStrikeCount() : -1); });
	UsePoint(TEXT("BoatLantern"), [this]() { return TestBoat.IsValid() ? TestBoat->GetLanternUse() : nullptr; }, 0.4f, 0.85f,
		[this]() { return TestBoat.IsValid() && TestBoat->IsLanternLit() && TestBoat->GetLanternLight()->IsVisible(); },
		[this]() { return FString::Printf(TEXT("lit=%d"), TestBoat.IsValid() && TestBoat->IsLanternLit() ? 1 : 0); });
	Steps.Add({TEXT("LanternDouse"), 0.f, [this]() { if (ASaltBoat* Bt = TestBoat.Get()) { Bt->SetLanternLit(false); } }, nullptr, nullptr});
	// Walk into the bell from the starboard side: it swings away and stops the walker.
	Steps.Add({TEXT("BumpApproach"), 0.f, [=, this]() { PlacePlayerAboard(SaltHull::BellPivot().X, 0.85f, -90.f); }, nullptr, nullptr});
	Wait(TEXT("BumpSettle"), 1.f);
	Steps.Add({TEXT("BumpBell"), 1.5f, [this]()
	{
		MaxValue = 0.f;
		if (ASaltPlayerCharacter* P = GetPlayer())
		{
			if (P->GetController() && TestBoat.IsValid()) { P->GetController()->SetControlRotation(FRotator(0.f, TestBoat->GetYawDeg() - 90.f, 0.f)); }
			P->SetDebugMoveInput(FVector2D(0.f, 1.f));
		}
	}, [this](float)
	{
		if (const ASaltBoat* Bt = TestBoat.Get()) { MaxValue = FMath::Max(MaxValue, Bt->GetBellSwing()->GetSwingAngle()); }
	}, [=, this]()
	{
		ASaltPlayerCharacter* P = GetPlayer();
		if (P) { P->SetDebugMoveInput(FVector2D::ZeroVector); }
		const FVector Feet = FeetInBoat();
		Report(TEXT("BoatBellBumped"), MaxValue > 4.f && Feet.Y > 15.f, FString::Printf(TEXT("bellMax=%.1fdeg walkerY=%.0fcm (stopped beside it)"), MaxValue, Feet.Y));
	}});

	// 9c) The starboard ladder (D54): E takes hold, W climbs rung by rung and at the top you step on board; from the deck E
	// takes hold at the top, S climbs down and at the bottom you are in the sea; E lets go anywhere; no keys, no slipping.
	auto AimAtLadder = [=, this](const FString& Name)
	{
		Steps.Add({Name + TEXT("Aim"), 0.4f, nullptr, [this](float)
		{
			ASaltPlayerCharacter* P = GetPlayer();
			const ASaltBoat* Bt = TestBoat.Get();
			if (P && Bt && P->GetController())
			{
				const FVector Target = Bt->GetLadder(true)->GetComponentLocation() + FVector(0.f, 0.f, 30.f);
				P->GetController()->SetControlRotation((Target - P->GetCamera()->GetComponentLocation()).Rotation());
			}
		}, nullptr});
	};
	auto TakeHold = [=, this](const FString& Name, float ExpectAlong)
	{
		AimAtLadder(Name);
		Steps.Add({Name, 1.f, [=, this]()
		{
			ASaltPlayerCharacter* P = GetPlayer();
			const bool bFocused = P && TestBoat.IsValid() && P->GetFocusedInteractable() == TestBoat->GetLadder(true);
			Report(Name + TEXT("Focus"), bFocused, FString::Printf(TEXT("focused=%s swimming=%d"),
				*GetNameSafe(P ? P->GetFocusedInteractable() : nullptr), P && P->IsSwimming() ? 1 : 0));
			if (P) { P->DebugInteract(); }
		}, nullptr, [=, this]()
		{
			const ASaltPlayerCharacter* P = GetPlayer();
			const bool bOn = P && P->IsOnLadder() && !P->IsSwimming();
			const float Along = P ? P->GetLadderAlong() : -1.f;
			Report(Name, bOn && FMath::Abs(Along - ExpectAlong) < 0.35f, FString::Printf(TEXT("onLadder=%d along=%.2f (expected about %.2f)"),
				P && P->IsOnLadder() ? 1 : 0, Along, ExpectAlong));
		}});
	};
	// Climb with W (+1) or S (-1) for up to Seconds; samples the feet's height (rungs: the body slows at each grip).
	auto Climb = [=, this](const FString& Name, float Input, float Seconds, TFunction<bool()> Check, TFunction<FString()> State)
	{
		Steps.Add({Name, Seconds, [=, this]()
		{
			Samples.Reset();
			if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(FVector2D(0.f, Input)); }
		}, [=, this](float)
		{
			ASaltPlayerCharacter* P = GetPlayer();
			Samples.Add(FVector(StepTime, FeetInBoat().Z, P && P->IsOnLadder() ? 1.f : 0.f));
			// Off the ladder (on deck or in the sea): let go of the key, as a player would.
			if (P && StepTime > 0.3f && !P->IsOnLadder() && !P->IsClimbing()) { P->SetDebugMoveInput(FVector2D::ZeroVector); }
		}, [=, this]()
		{
			if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(FVector2D::ZeroVector); }
			if (Check)
			{
				Report(Name, Check(), State());
			}
		}});
	};
	// Time on the ladder and the number of pulls (the body slows between rungs).
	auto ClimbStats = [this](float& OutTime, int32& OutSteps)
	{
		OutTime = 0.f;
		OutSteps = 0;
		bool bMoving = false;
		for (int32 I = 1; I < Samples.Num(); ++I)
		{
			if (Samples[I].Z > 0.5f)
			{
				OutTime = Samples[I].X;
			}
			const float Dt = FMath::Max((float)(Samples[I].X - Samples[I - 1].X), 1e-3f);
			const float Speed = FMath::Abs((float)(Samples[I].Y - Samples[I - 1].Y)) / Dt;
			if (Samples[I].Z < 0.5f)
			{
				continue;
			}
			if (!bMoving && Speed > 60.f)
			{
				bMoving = true;
				++OutSteps;
			}
			else if (bMoving && Speed < 35.f)
			{
				bMoving = false;
			}
		}
	};
	auto SwimToLadder = [=, this](const FString& Name)
	{
		Steps.Add({Name + TEXT("Swim"), 0.f, [this]()
		{
			ASaltPlayerCharacter* P = GetPlayer();
			const ASaltBoat* Bt = TestBoat.Get();
			if (P && Bt)
			{
				const FVector Local(SaltHull::LadderX * 100.f, (SaltHull::HalfBeamAtSheer(SaltHull::LadderX) + 0.8f) * 100.f, -150.f);
				P->TeleportFeetTo(Bt->GetActorTransform().TransformPosition(Local), Bt->GetYawDeg() - 90.f);
			}
		}, nullptr, nullptr});
		Wait(Name + TEXT("Float"), 2.5f);
	};
	Steps.Add({TEXT("LadderSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(TestWater, 90.f, 0.f, false, false); }, nullptr, nullptr});
	Wait(TEXT("LadderSettle"), 1.f);
	SwimToLadder(TEXT("LadderUp"));
	TakeHold(TEXT("BoatLadderHoldFromWater"), 0.f);
	Steps.Add({TEXT("LadderNoKeys"), 1.5f, [this]() { MaxValue = GetPlayer() ? GetPlayer()->GetLadderAlong() : 0.f; }, nullptr, [this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		const float Along = P ? P->GetLadderAlong() : -1.f;
		Report(TEXT("BoatLadderHolds"), P && P->IsOnLadder() && FMath::Abs(Along - MaxValue) < 0.15f,
			FString::Printf(TEXT("along %.2f -> %.2f without keys"), MaxValue, Along));
	}});
	Climb(TEXT("BoatLadderUp"), 1.f, 4.f, [=, this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		float Time;
		int32 Rungs;
		ClimbStats(Time, Rungs);
		// D59: one smooth glide up (no rung-by-rung pulls), then over the top on to the deck.
		return P && !P->IsOnLadder() && !P->IsClimbing() && OnBoat() && FMath::Abs(FeetInBoat().Y) < SaltHull::HalfBeamAtSheer(0.f) * 100.f - 30.f
			&& Time > 0.6f && Rungs >= 1 && Rungs <= 2;
	}, [=, this]()
	{
		float Time;
		int32 Rungs;
		ClimbStats(Time, Rungs);
		const FVector F = FeetInBoat();
		return FString::Printf(TEXT("onDeck=%d feet=(%.0f, %.0f, %.0f)cm onLadder=%.2fs pulls=%d"), OnBoat() ? 1 : 0, F.X, F.Y, F.Z, Time, Rungs);
	});
	Steps.Add({TEXT("LadderDeck"), 0.f, [=, this]() { PlacePlayerAboard(SaltHull::LadderX, 0.85f, 90.f); }, nullptr, nullptr});
	Wait(TEXT("LadderDeckSettle"), 1.f);
	TakeHold(TEXT("BoatLadderHoldFromDeck"), 1.f);
	Climb(TEXT("BoatLadderDown"), -1.f, 5.f, [=, this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		float Time;
		int32 Rungs;
		ClimbStats(Time, Rungs);
		return P && !P->IsOnLadder() && P->IsSwimming() && FMath::Abs(FeetInBoat().Y) > SaltHull::HalfBeamAtSheer(0.f) * 100.f && Rungs >= 1 && Rungs <= 2;
	}, [=, this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		float Time;
		int32 Rungs;
		ClimbStats(Time, Rungs);
		const FVector F = FeetInBoat();
		return FString::Printf(TEXT("swimming=%d feet=(%.0f, %.0f, %.0f)cm onLadder=%.2fs pulls=%d"), P && P->IsSwimming() ? 1 : 0, F.X, F.Y, F.Z,
			Time, Rungs);
	});
	// Take hold again from the water, climb half way and let go with E: into the sea.
	SwimToLadder(TEXT("LadderLetGo"));
	TakeHold(TEXT("BoatLadderHoldAgain"), 0.f);
	Climb(TEXT("LadderHalfWay"), 1.f, 0.8f, nullptr, nullptr);
	Steps.Add({TEXT("BoatLadderLetGo"), 3.f, [this]()
	{
		MaxValue = GetPlayer() ? GetPlayer()->GetLadderAlong() : 0.f;
		if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugInteract(); }
	}, nullptr, [this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		Report(TEXT("BoatLadderLetGo"), P && !P->IsOnLadder() && P->IsSwimming() && MaxValue > 0.1f,
			FString::Printf(TEXT("let go at along %.2f, swimming=%d"), MaxValue, P && P->IsSwimming() ? 1 : 0));
	}});
	// D59 (owner: "just running in the ladder does nothing"): swimming at the ladder takes hold of it, no E needed.
	SwimToLadder(TEXT("LadderTouch"));
	Steps.Add({TEXT("BoatLadderTouch"), 2.f, [this]()
	{
		MaxValue = 0.f;
		if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(FVector2D(0.f, 1.f)); }
	}, [this](float)
	{
		// Holding W it takes hold and climbs straight on up (on deck within about a second): note when it held on.
		ASaltPlayerCharacter* P = GetPlayer();
		if (P && P->IsOnLadder() && MaxValue <= 0.f)
		{
			MaxValue = FMath::Max(StepTime, 0.001f);
		}
		if (P && MaxValue > 0.f && !P->IsOnLadder() && !P->IsClimbing())
		{
			P->SetDebugMoveInput(FVector2D::ZeroVector);
		}
	}, [this]()
	{
		ASaltPlayerCharacter* P = GetPlayer();
		if (P)
		{
			P->SetDebugMoveInput(FVector2D::ZeroVector);
			P->LetGoOfLadder();
		}
		Report(TEXT("BoatLadderTouch"), MaxValue > 0.f && MaxValue < 1.f, FString::Printf(TEXT("swam at the ladder: took hold after %.2f s"), MaxValue));
	}});

	// 10) Waves: a beam reach in the Rough sea, then bare poles in the Storm sea (open water beyond the cove's calm
	// zone). The boat must ride the waves without capsizing.
	struct FSea { const TCHAR* Name; float WindMs; bool bSail; float MaxHeel; float MinKn; };
	// The storm only has to be survived: rolls stay below the capsize threshold (70 degrees).
	const FSea Seas[] = { { TEXT("Rough"), 8.f, true, 40.f, 2.5f }, { TEXT("Storm"), 14.f, false, 70.f, 0.f } };
	for (const FSea& Sea : Seas)
	{
		const FString Name(Sea.Name);
		Steps.Add({Name + TEXT("SeaSetup"), 0.f, [=, this]()
		{
			if (ASaltLightingRig* R = WeakRig.Get())
			{
				R->SetSeaState(FName(Sea.Name), 0.f);
			}
			SetWind(0.f, Sea.WindMs);
			Place(FVector(15000.f, -9000.f, 0.f), 90.f, Sea.bSail ? 2.5f : 0.f, Sea.bSail, true);
		}, nullptr, nullptr});
		Wait(Name + TEXT("SeaBuild"), 20.f);
		Steps.Add({Name + TEXT("Sea"), 20.f, [this]() { Samples.Reset(); MinValue = 1e9f; MaxValue = -1e9f; },
			[this](float)
			{
				if (const ASaltBoat* Bt = TestBoat.Get())
				{
					Samples.Add(FVector(Bt->GetSpeedKnots(), Bt->GetHeelDeg(), Bt->GetPitchDeg()));
					MinValue = FMath::Min(MinValue, (float)Bt->GetActorLocation().Z);
					MaxValue = FMath::Max(MaxValue, (float)Bt->GetActorLocation().Z);
				}
			},
			[this, Name, Sea]()
			{
				const float Speed = MeanOf(Samples, 0), Heel = MaxAbsOf(Samples, 1), Pitch = MaxAbsOf(Samples, 2);
				Report(TEXT("Boat") + Name + TEXT("Sea"), Heel < Sea.MaxHeel && Speed >= Sea.MinKn, FString::Printf(
					TEXT("speed=%.2fkn maxHeel=%.1fdeg maxPitch=%.1fdeg heaveRange=%.0fcm sail=%d"),
					Speed, Heel, Pitch, MaxValue - MinValue, Sea.bSail ? 1 : 0));
			}});
	}
	Steps.Add({TEXT("FlatAgain"), 0.f, [WeakRig]() { if (ASaltLightingRig* R = WeakRig.Get()) { R->SetSeaState(TEXT("Flat"), 0.f); } }, nullptr, nullptr});

	// 10b) Sailing onto Ruin Isle (D47) from the west: the boat runs aground on the beach shelf (the coast is about
	// 47 m west of the island's centre at X = 1600 m), stays on top of the sand, and stops once the sail is in.
	Steps.Add({TEXT("IsleSetup"), 0.f, [=, this]() { SetWind(0.f, 7.f); Place(FVector(146000.f, 60000.f, 0.f), 0.f, 3.f, true, true); }, nullptr, nullptr});
	Steps.Add({TEXT("IsleApproach"), 45.f, [this]() { MaxValue = 0.f; },
		[this](float) { if (const ASaltBoat* Bt = TestBoat.Get()) { MaxValue = FMath::Max(MaxValue, Bt->GetContactDepth()); } }, nullptr});
	Steps.Add({TEXT("IsleLanding"), 12.f, [this]() { if (ASaltBoat* Bt = TestBoat.Get()) { Bt->SetSailSet(false); } },
		[this](float) { if (const ASaltBoat* Bt = TestBoat.Get()) { MaxValue = FMath::Max(MaxValue, Bt->GetContactDepth()); } },
		[this]()
		{
			const ASaltBoat* Bt = TestBoat.Get();
			const float Speed = Bt ? Bt->GetSpeedKnots() / 1.943844f : 99.f;
			const float X = Bt ? (float)Bt->GetActorLocation().X : 0.f;
			const int32 Contacts = Bt ? Bt->GetContactCount() : 0;
			Report(TEXT("BoatLandsOnRuinIsle"), Speed < 0.3f && Contacts > 0 && MaxValue < 15.f && X > 150000.f && X < 155800.f, FString::Printf(
				TEXT("finalSpeed=%.2fm/s contacts=%d maxPenetration=%.1fcm originX=%.0fcm heel=%.1fdeg pitch=%.1fdeg"),
				Speed, Contacts, MaxValue, X, Bt ? Bt->GetHeelDeg() : 0.f, Bt ? Bt->GetPitchDeg() : 0.f));
		}});

	// 11) Reset to the mooring; the lines hold the boat in a fresh breeze.
	Steps.Add({TEXT("MooringSetup"), 0.f, [=, this]()
	{
		if (ASaltPlayerCharacter* P = GetPlayer()) { P->TeleportFeetTo(TestWater + FVector(0.f, 0.f, 3000.f), 0.f); }
		SetWind(200.f, 7.f);
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetControlOverride(false);
			Bt->ResetToStart();
			MarkWorld = Bt->GetActorLocation();
		}
	}, nullptr, nullptr});
	Steps.Add({TEXT("Moored"), 10.f, [this]() { MaxValue = 0.f; },
		[this](float) { if (const ASaltBoat* Bt = TestBoat.Get()) { MaxValue = FMath::Max(MaxValue, (float)FVector::Dist2D(Bt->GetActorLocation(), MarkWorld)); } },
		[this]()
		{
			const ASaltBoat* Bt = TestBoat.Get();
			Report(TEXT("BoatMooredHolds"), Bt && Bt->GetHold() == ESaltBoatHold::Moored && MaxValue < 40.f,
				FString::Printf(TEXT("maxOffset=%.1fcm"), MaxValue));
		}});

	Steps.Add({TEXT("BoatFinish"), 0.f, [this, WeakRig]()
	{
		if (USaltWindSubsystem* W = GetWorld()->GetSubsystem<USaltWindSubsystem>())
		{
			W->SetBaseWind(SavedWind.X, SavedWind.Y, SavedWind.Z);
		}
		if (ASaltLightingRig* R = WeakRig.Get())
		{
			if (!SavedSeaState.IsNone())
			{
				R->SetSeaState(SavedSeaState, 0.f);
			}
		}
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetAutopilot(false);
			Bt->SetControlOverride(false);
			Bt->ResetToStart();
		}
		ResetPlayer();
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST BoatSuite DONE passed=%d failed=%d"), Passed, Failed);
	}, nullptr, nullptr});

	// D59: a part on its own (its steps, the set-up and the finish).
	if (!Only.IsEmpty())
	{
		const FString Part = Only;
		Steps.RemoveAll([&Part](const FStep& S)
		{
			return !S.Name.Contains(Part) && S.Name != TEXT("BoatSetup") && S.Name != TEXT("BoatFinish");
		});
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST BoatSuite only '%s': %d steps"), *Part, Steps.Num());
	}

	StepIndex = 0;
	StepTime = 0.f;
	bStepStarted = false;
#endif
}

// Saltbound - development: automated sailing tests (sb.Dev.RunBoatTests, D36).

#include "Dev/SaltDevTestSubsystem.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltStationComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"
#include "World/SaltLightingRig.h"
#include "World/SaltWindSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdRunBoatTests(
		TEXT("sb.Dev.RunBoatTests"),
		TEXT("Run the sailing tests (needs a SaltBoat in the map): buoyancy, roll, points of sail, turning, collision, grounding, player aboard, helm, mooring."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = World ? World->GetSubsystem<USaltDevTestSubsystem>() : nullptr)
			{
				T->RunBoatSuite();
			}
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

void USaltDevTestSubsystem::RunBoatSuite()
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
	auto Place = [this](const FVector& At, float Yaw, float SpeedMs, bool bSail, float Sheet, bool bAutopilot)
	{
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetHold(ESaltBoatHold::Free);
			Bt->PlaceAt(At, Yaw, SpeedMs * 100.f);
			Bt->SetSailRaised(bSail);
			Bt->SetControlOverride(true, 0.f, Sheet);
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
	Steps.Add({TEXT("FloatSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(TestWater, 90.f, 0.f, false, 0.5f, false); }, nullptr, nullptr});
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

	// 3) Points of sail in a steady 7 m/s (13.6 kn) wind, the autopilot holding a heading of 90 deg (+Y).
	// A heavy boat gathers way slowly (about half a minute to full speed), so each run starts under way.
	struct FPoint { const TCHAR* Name; float WindFrom; float Sheet; float StartMs; float MinKn; float MaxKn; };
	const FPoint Points[] = {
		{ TEXT("BeamReach"), 0.f, 0.45f, 2.5f, 4.5f, 8.5f },     // wind 90 deg on the port side
		{ TEXT("CloseHauled"), 40.f, 0.05f, 2.f, 3.f, 7.f },     // 50 deg off the wind
		{ TEXT("BroadReach"), 310.f, 0.75f, 2.5f, 4.f, 8.5f },   // 140 deg
		{ TEXT("Running"), 275.f, 1.f, 2.f, 3.f, 7.5f },         // 175 deg, almost dead downwind
		{ TEXT("NoGoZone"), 80.f, 0.f, 0.f, 0.f, 1.f },          // 10 deg: the sail luffs
	};
	for (const FPoint& Pt : Points)
	{
		const FString Name(Pt.Name);
		Steps.Add({Name + TEXT("Setup"), 0.f, [=, this]() { SetWind(Pt.WindFrom, 7.f); Place(TestWater, 90.f, Pt.StartMs, true, Pt.Sheet, true); }, nullptr, nullptr});
		Wait(Name + TEXT("Build"), 28.f);
		Steps.Add({Name, 8.f, [this]() { Samples.Reset(); MaxValue = 0.f; MinValue = 0.f; },
			[this](float)
			{
				if (const ASaltBoat* Bt = TestBoat.Get())
				{
					const float Leeway = Bt->GetSpeedKnots() > 1.f ? FRotator::NormalizeAxis(Bt->GetCourseDeg() - Bt->GetYawDeg()) : 0.f;
					Samples.Add(FVector(Bt->GetSpeedKnots(), FMath::Abs(Bt->GetHeelDeg()), FMath::Abs(Leeway)));
					MaxValue = FMath::Max(MaxValue, FMath::Abs(FRotator::NormalizeAxis(Bt->GetYawDeg() - 90.f)));
					MinValue += FVector::DotProduct(Bt->GetVelocity(), Bt->GetActorForwardVector()) * 0.01943844f;   // headway, kn
				}
			},
			[this, Name, Pt]()
			{
				const float Speed = MeanOf(Samples, 0), Heel = MeanOf(Samples, 1), Leeway = MeanOf(Samples, 2);
				const float Headway = Samples.Num() > 0 ? MinValue / Samples.Num() : 0.f;
				// In the no-go zone the boat makes no headway (it drifts back and sideways, so leeway means nothing).
				const bool bNoGo = Pt.MaxKn <= 1.f;
				const bool bPass = bNoGo ? Headway < 0.5f
					: (Speed >= Pt.MinKn && Speed <= Pt.MaxKn && Heel < 25.f && Leeway < 10.f);
				Report(TEXT("Sail") + Name, bPass, FString::Printf(TEXT("speed=%.2fkn headway=%.2fkn heel=%.1fdeg leeway=%.1fdeg headingError=%.1fdeg sheet=%.2f"),
					Speed, Headway, Heel, Leeway, MaxValue, Pt.Sheet));
			}});
	}

	// 4) Turning circle: beam reach at speed, then full starboard rudder.
	Steps.Add({TEXT("TurnSetup"), 0.f, [=, this]() { SetWind(0.f, 7.f); Place(TestWater, 90.f, 3.f, true, 0.45f, true); }, nullptr, nullptr});
	Wait(TEXT("TurnBuild"), 15.f);
	Steps.Add({TEXT("TurnRudder"), 3.f, [this]()
	{
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetAutopilot(false);
			Bt->SetControlOverride(true, 1.f, 0.45f);
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
		Place(TestWater, 90.f, 0.f, false, 0.5f, false);
		if (ASaltBoat* Bt = TestBoat.Get())
		{
			Bt->SetControlOverride(true, 1.f, 0.5f);
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
	Steps.Add({TEXT("PierSetup"), 0.f, [=, this]() { SetWind(0.f, 0.f); Place(FVector(2700.f, 0.f, 0.f), 180.f, 2.5f, false, 0.5f, false); }, nullptr, nullptr});
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
	Steps.Add({TEXT("GroundSetup"), 0.f, [=, this]() { Place(FVector(300.f, -2200.f, 0.f), 180.f, 2.f, false, 0.5f, false); }, nullptr, nullptr});
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
	Steps.Add({TEXT("AboardSetup"), 0.f, [=, this]() { SetWind(0.f, 7.f); Place(TestWater, 90.f, 3.f, true, 0.45f, true); }, nullptr, nullptr});
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
	Steps.Add({TEXT("HelmApproach"), 0.f, [=, this]() { PlacePlayerAboard(SaltHull::HelmsmanX - 0.15f, 0.f, 0.f); }, nullptr, nullptr});
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
	Steps.Add({TEXT("HelmLeave"), 1.5f, [this]() { if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugInteract(); } }, nullptr, [=, this]()
	{
		const ASaltPlayerCharacter* P = GetPlayer();
		Report(TEXT("BoatHelmLeave"), P && !P->IsAtStation() && OnBoat(), FString::Printf(TEXT("atHelm=%d onDeck=%d"), P && P->IsAtStation() ? 1 : 0, OnBoat() ? 1 : 0));
	}});

	// 10) Waves: a beam reach in the Rough sea, then bare poles in the Storm sea (open water beyond the cove's calm
	// zone). The boat must ride the waves without capsizing.
	struct FSea { const TCHAR* Name; float WindMs; bool bSail; float MaxHeel; float MinKn; };
	const FSea Seas[] = { { TEXT("Rough"), 8.f, true, 35.f, 2.5f }, { TEXT("Storm"), 14.f, false, 50.f, 0.f } };
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
			Place(FVector(15000.f, -9000.f, 0.f), 90.f, Sea.bSail ? 2.5f : 0.f, Sea.bSail, 0.45f, true);
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

	StepIndex = 0;
	StepTime = 0.f;
	bStepStarted = false;
#endif
}

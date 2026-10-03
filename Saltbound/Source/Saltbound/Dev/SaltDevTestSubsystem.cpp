// Saltbound - development: automated movement tests and dev console commands.

#include "Dev/SaltDevTestSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dev/SaltDevMovingPlatform.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltStationComponent.h"
#include "Ocean/SaltOceanSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltCharacterMovementComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "TimerManager.h"
#include "World/SaltLightingRig.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "RHI.h"
#include "RenderCore.h"
#include "UnrealClient.h"

#if !UE_BUILD_SHIPPING
namespace
{
	USaltDevTestSubsystem* GetTests(UWorld* World)
	{
		return World ? World->GetSubsystem<USaltDevTestSubsystem>() : nullptr;
	}

	float ArgOr(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunMovementTests(
		TEXT("sb.Dev.RunMovementTests"),
		TEXT("Run moving-platform movement tests. Args: [MotionScale=1] [Speed=300] [TurnRate=8]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->RunMovementSuite(ArgOr(Args, 0, 1.f), ArgOr(Args, 1, 300.f), ArgOr(Args, 2, 8.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPlatform(
		TEXT("sb.Dev.Platform"),
		TEXT("Configure test platforms. Args: <MotionScale> [Speed TurnRate] (with speed: forced drive, without: helm drive)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->ConfigurePlatform(ArgOr(Args, 0, 1.f), Args.Num() > 1, ArgOr(Args, 1, 0.f), ArgOr(Args, 2, 0.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPerf(
		TEXT("sb.Dev.Perf"),
		TEXT("Measure frame / GPU / thread times for N seconds (default 10) and log an SB_PERF line."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->StartPerf(ArgOr(Args, 0, 10.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBenchmark(
		TEXT("sb.Dev.Benchmark"),
		TEXT("Measure performance for each lighting preset (Day, Evening, Dusk). Args: [SecondsPerPreset=10]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->RunBenchmark(ArgOr(Args, 0, 10.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDelayed(
		TEXT("sb.Dev.Delayed"),
		TEXT("Run a console command after a delay: sb.Dev.Delayed <Seconds> <Command ...>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 2)
			{
				return;
			}
			const float Delay = FCString::Atof(*Args[0]);
			FString Cmd;
			for (int32 i = 1; i < Args.Num(); ++i)
			{
				Cmd += (i > 1 ? TEXT(" ") : TEXT("")) + Args[i];
			}
			FTimerHandle Handle;
			TWeakObjectPtr<UWorld> WeakWorld(World);
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Cmd]()
			{
				if (UWorld* W = WeakWorld.Get())
				{
					UE_LOG(LogSaltbound, Display, TEXT("SB_DELAYED %s"), *Cmd);
					GEngine->Exec(W, *Cmd);
				}
			}), FMath::Max(Delay, 0.01f), false);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCamera(
		TEXT("sb.Dev.Camera"),
		TEXT("View from a fixed camera: sb.Dev.Camera <X> <Y> <Z> <Pitch> <Yaw> [FOV=80] [Float=0]. With Float=1, Z is a height above ")
		TEXT("the water and the camera rides the waves (smoothed, like a small boat). sb.Dev.Camera off returns to the player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			if (!PC)
			{
				return;
			}
			ACameraActor* Cam = nullptr;
			for (TActorIterator<ACameraActor> It(World); It; ++It)
			{
				if (It->Tags.Contains(TEXT("SB_DevCamera"))) { Cam = *It; break; }
			}
			ASaltPlayerController* SaltPC = Cast<ASaltPlayerController>(PC);
			if (Args.Num() >= 1 && Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase))
			{
				PC->SetViewTarget(PC->GetPawn());
				if (SaltPC) { SaltPC->SetHUDVisible(true); }
				return;
			}
			if (SaltPC) { SaltPC->SetHUDVisible(false); }
			if (Args.Num() < 5)
			{
				return;
			}
			if (!Cam)
			{
				Cam = World->SpawnActor<ACameraActor>();
				Cam->Tags.Add(TEXT("SB_DevCamera"));
				Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			}
			Cam->SetActorLocationAndRotation(FVector(ArgOr(Args, 0, 0.f), ArgOr(Args, 1, 0.f), ArgOr(Args, 2, 0.f)),
				FRotator(ArgOr(Args, 3, 0.f), ArgOr(Args, 4, 0.f), 0.f));
			Cam->GetCameraComponent()->SetFieldOfView(ArgOr(Args, 5, 80.f));
			PC->SetViewTarget(Cam);
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->SetFloatingCamera(Cam, ArgOr(Args, 6, 0.f) != 0.f, ArgOr(Args, 2, 0.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDumpFrames(
		TEXT("sb.Dev.DumpFrames"),
		TEXT("Write the next N rendered frames to Saved/Screenshots (MovieFrameNNNNN) straight from the renderer, independent ")
		TEXT("of other windows. With -UseFixedTimeStep -FPS=<n> every frame advances the game by exactly 1/n s (reproducible clips)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			GIsDumpingMovie = Args.Num() > 0 ? FMath::Max(0, FCString::Atoi(*Args[0])) : 0;
			UE_LOG(LogSaltbound, Display, TEXT("SB_DUMP frames=%d"), GIsDumpingMovie);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTeleport(
		TEXT("sb.Dev.Teleport"),
		TEXT("Place the player's feet at a world location and view through the player again: sb.Dev.Teleport <X> <Y> <Z> [Yaw=0] [Pitch=0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			if (!P || Args.Num() < 3)
			{
				return;
			}
			P->TeleportFeetTo(FVector(ArgOr(Args, 0, 0.f), ArgOr(Args, 1, 0.f), ArgOr(Args, 2, 0.f)), ArgOr(Args, 3, 0.f));
			PC->SetControlRotation(FRotator(ArgOr(Args, 4, 0.f), ArgOr(Args, 3, 0.f), 0.f));
			PC->SetViewTarget(P);
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->SetFloatingCamera(nullptr, false, 0.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdInput(
		TEXT("sb.Dev.Input"),
		TEXT("Simulated player input for tests: sb.Dev.Input <Forward> <Right> [Up 0|1] [Dive 0|1] (all zero = release)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			if (ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr)
			{
				P->SetDebugMoveInput(FVector2D(ArgOr(Args, 1, 0.f), ArgOr(Args, 0, 0.f)));
				P->DebugJump(ArgOr(Args, 2, 0.f) != 0.f);
				P->DebugDive(ArgOr(Args, 3, 0.f) != 0.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWaterState(
		TEXT("sb.Dev.WaterState"),
		TEXT("Log the player's water state (movement mode, immersion, surface height) and the camera's height above the water."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
			ASaltPlayerCharacter* P = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
			const USaltCharacterMovementComponent* Move = P ? Cast<USaltCharacterMovementComponent>(P->GetCharacterMovement()) : nullptr;
			if (!Move)
			{
				return;
			}
			const FVector Feet = P->GetActorLocation() - FVector(0.f, 0.f, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector Cam = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
			const USaltOceanSubsystem* Ocean = World->GetSubsystem<USaltOceanSubsystem>();
			const float CamWater = Ocean ? (float)Ocean->SampleWater(FVector2D(Cam.X, Cam.Y)).Location.Z : 0.f;
			UE_LOG(LogSaltbound, Display, TEXT("SB_WATER mode=%s inWater=%d immersion=%.2f surface=%.1f feet=(%.0f, %.0f, %.1f) speed=%.0f velZ=%.0f cam=%.1f camAboveWater=%.1f"),
				*Move->GetMovementName(), Move->IsInWater() ? 1 : 0, Move->ImmersionDepth(), Move->GetWaterSurfaceZ(),
				Feet.X, Feet.Y, Feet.Z, Move->Velocity.Size2D(), Move->Velocity.Z, Cam.Z, Cam.Z - CamWater);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdResetPlayer(
		TEXT("sb.Dev.ResetPlayer"),
		TEXT("Teleport the player to the first PlayerStart."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USaltDevTestSubsystem* T = GetTests(World))
			{
				T->ResetPlayer();
			}
		}));
}
#endif

bool USaltDevTestSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
#endif
}

TStatId USaltDevTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltDevTestSubsystem, STATGROUP_Tickables);
}

// --- Helpers -------------------------------------------------------------------

void USaltDevTestSubsystem::FDeckTrack::Add(const FVector& Local, float Dt)
{
	if (Frames == 0)
	{
		First = Prev = Local;
	}
	else
	{
		const FVector Delta = Local - Prev;
		if (Frames >= 2)
		{
			// Second difference = frame-to-frame change of motion: large values are visible jitter.
			const float Accel = (Delta - PrevDelta).Size();
			SumSqAccel += Accel * Accel;
			MaxAccelStep = FMath::Max(MaxAccelStep, Accel);
		}
		PrevDelta = Delta;
		LastSpeed = Dt > 0.f ? Delta.Size() / Dt : 0.f;
		Prev = Local;
	}
	MaxDrift = FMath::Max(MaxDrift, (Local - First).Size());
	++Frames;
}

ASaltPlayerCharacter* USaltDevTestSubsystem::GetPlayer() const
{
	return Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
}

ASaltDevMovingPlatform* USaltDevTestSubsystem::FindPlatform() const
{
	for (TActorIterator<ASaltDevMovingPlatform> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FVector USaltDevTestSubsystem::FeetInDeckSpace() const
{
	const ASaltPlayerCharacter* P = GetPlayer();
	const ASaltDevMovingPlatform* Plat = Platform.Get();
	if (!P || !Plat)
	{
		return FVector::ZeroVector;
	}
	const FVector FeetWorld = P->GetActorLocation() - FVector(0.f, 0.f, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return Plat->GetActorTransform().InverseTransformPosition(FeetWorld);
}

FVector USaltDevTestSubsystem::EyeInDeckSpace() const
{
	const ASaltPlayerCharacter* P = GetPlayer();
	const ASaltDevMovingPlatform* Plat = Platform.Get();
	return (P && Plat) ? Plat->GetActorTransform().InverseTransformPosition(P->GetCamera()->GetComponentLocation()) : FVector::ZeroVector;
}

bool USaltDevTestSubsystem::IsPlayerOnDeck() const
{
	const ASaltPlayerCharacter* P = GetPlayer();
	const ASaltDevMovingPlatform* Plat = Platform.Get();
	if (!P || !Plat)
	{
		return false;
	}
	const UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(P->GetCharacterMovement()->GetMovementBaseObject());
	return Base && Base->GetOwner() == Plat && P->GetCharacterMovement()->IsMovingOnGround();
}

void USaltDevTestSubsystem::PlacePlayerOnDeck(const FVector& DeckLocalFeet, float RelativeYaw)
{
	ASaltPlayerCharacter* P = GetPlayer();
	ASaltDevMovingPlatform* Plat = Platform.Get();
	if (P && Plat)
	{
		const FVector World = Plat->GetActorTransform().TransformPosition(DeckLocalFeet + FVector(0.f, 0.f, Plat->GetDeckTopZ()));
		P->TeleportFeetTo(World, Plat->GetActorRotation().Yaw + RelativeYaw);
	}
}

void USaltDevTestSubsystem::Report(const FString& Name, bool bPass, const FString& Details)
{
	(bPass ? Passed : Failed)++;
	UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), *Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
}

// --- Commands --------------------------------------------------------------------

void USaltDevTestSubsystem::ConfigurePlatform(float MotionScale, bool bForce, float Speed, float TurnRate)
{
	for (TActorIterator<ASaltDevMovingPlatform> It(GetWorld()); It; ++It)
	{
		It->MotionScale = MotionScale;
		It->bUseForcedDrive = bForce;
		It->ForcedSpeed = Speed;
		It->ForcedTurnRate = TurnRate;
	}
}

void USaltDevTestSubsystem::StartPerf(float Seconds)
{
	PerfDuration = PerfRemaining = FMath::Max(1.f, Seconds);
	PerfFrame.Reset();
	PerfGpu.Reset();
	PerfGame.Reset();
	PerfRender.Reset();
	UE_LOG(LogSaltbound, Display, TEXT("SB_PERF START seconds=%.1f"), PerfDuration);
}

namespace
{
	void Stats(TArray<float> V, float& OutAvg, float& OutP95, float& OutMax)
	{
		OutAvg = OutP95 = OutMax = 0.f;
		if (V.Num() == 0)
		{
			return;
		}
		double Sum = 0.0;
		for (float X : V) { Sum += X; }
		OutAvg = (float)(Sum / V.Num());
		V.Sort();
		OutP95 = V[FMath::Min(V.Num() - 1, (int32)(V.Num() * 0.95f))];
		OutMax = V.Last();
	}
}

void USaltDevTestSubsystem::RunBenchmark(float SecondsPerPreset)
{
	Steps.Reset();
	Steps.Add({TEXT("Warmup"), 6.f, nullptr, nullptr, nullptr});
	for (const TCHAR* Preset : { TEXT("Day"), TEXT("Evening"), TEXT("Dusk") })
	{
		const FString Name(Preset);
		Steps.Add({TEXT("Apply_") + Name, 4.f, [this, Name]()
		{
			for (TActorIterator<ASaltLightingRig> It(GetWorld()); It; ++It)
			{
				It->ApplyPreset(FName(*Name));
			}
		}, nullptr, nullptr});
		Steps.Add({TEXT("Measure_") + Name, SecondsPerPreset + 0.5f, [this, Name, SecondsPerPreset]()
		{
			PerfLabel = Name;
			StartPerf(SecondsPerPreset);
		}, nullptr, nullptr});
	}
	Steps.Add({TEXT("Done"), 0.f, []() { UE_LOG(LogSaltbound, Display, TEXT("SB_BENCH DONE")); }, nullptr, nullptr});
	StepIndex = 0;
	StepTime = 0.f;
	bStepStarted = false;
}

void USaltDevTestSubsystem::ResetPlayer()
{
	ASaltPlayerCharacter* P = GetPlayer();
	for (TActorIterator<APlayerStart> It(GetWorld()); It && P; ++It)
	{
		P->TeleportFeetTo(It->GetActorLocation() - FVector(0.f, 0.f, 90.f), It->GetActorRotation().Yaw);
		return;
	}
}

void USaltDevTestSubsystem::RunMovementSuite(float MotionScale, float Speed, float TurnRate)
{
	Platform = FindPlatform();
	if (!Platform.IsValid() || !GetPlayer())
	{
		UE_LOG(LogSaltbound, Error, TEXT("SB_TEST Suite FAIL reason=no_platform_or_player"));
		return;
	}
	Passed = Failed = 0;
	Steps.Reset();
	const FString Cfg = FString::Printf(TEXT("motion=%.2f speed=%.0f turn=%.1f"), MotionScale, Speed, TurnRate);
	UE_LOG(LogSaltbound, Display, TEXT("SB_TEST Suite START %s"), *Cfg);

	auto Settle = [this](const FString& Name, float Seconds)
	{
		Steps.Add({Name, Seconds, nullptr, nullptr, nullptr});
	};

	// 1) Stand still on the moving deck: no sliding, no jitter.
	Steps.Add({TEXT("Setup"), 0.f, [this, MotionScale, Speed, TurnRate]()
	{
		ConfigurePlatform(MotionScale, true, Speed, TurnRate);
		PlacePlayerOnDeck(FVector(-120.f, 60.f, 0.f), 0.f);
	}, nullptr, nullptr});
	Settle(TEXT("SettleStand"), 1.5f);
	Steps.Add({TEXT("Stand"), 12.f,
		[this]() { Feet.Reset(); Eye.Reset(); OffDeckFrames = 0; },
		[this](float Dt) { Feet.Add(FeetInDeckSpace(), Dt); Eye.Add(EyeInDeckSpace(), Dt); OffDeckFrames += IsPlayerOnDeck() ? 0 : 1; },
		[this]()
		{
			const float EyeRms = Eye.Frames > 2 ? FMath::Sqrt(Eye.SumSqAccel / (Eye.Frames - 2)) : 0.f;
			const bool bPass = Feet.MaxDrift < 2.f && Eye.MaxAccelStep < 0.5f && OffDeckFrames == 0;
			Report(TEXT("StandOnMovingDeck"), bPass, FString::Printf(
				TEXT("frames=%d feetDrift=%.3fcm eyeJitterMax=%.4fcm eyeJitterRms=%.5fcm offDeckFrames=%d"),
				Feet.Frames, Feet.MaxDrift, Eye.MaxAccelStep, EyeRms, OffDeckFrames));
		}});

	// 2) Walk forward, stop, strafe, stop: speed relative to deck matches input, no sliding after stopping.
	auto Walk = [this](const FString& Name, FVector2D Input, float Seconds)
	{
		Steps.Add({Name, Seconds,
			[this, Input]() { MarkLocal = FeetInDeckSpace(); OffDeckFrames = 0; if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(Input); } },
			[this](float) { OffDeckFrames += IsPlayerOnDeck() ? 0 : 1; },
			[this, Name, Seconds]()
			{
				const FVector Moved = FeetInDeckSpace() - MarkLocal;
				const float Planar = FVector(Moved.X, Moved.Y, 0.f).Size();
				const ASaltPlayerCharacter* P = GetPlayer();
				const float Expected = P ? P->WalkSpeed * Seconds : 0.f;
				// Acceleration ramp costs a little distance; accept 80-105 % of the ideal distance.
				const bool bPass = Planar > Expected * 0.8f && Planar < Expected * 1.05f && OffDeckFrames == 0;
				Report(Name, bPass, FString::Printf(TEXT("deckDistance=%.1fcm expected~%.1fcm offDeckFrames=%d"), Planar, Expected, OffDeckFrames));
			}});
	};
	auto Stop = [this](const FString& Name)
	{
		Steps.Add({Name + TEXT("_Brake"), 0.5f, [this]() { if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(FVector2D::ZeroVector); } }, nullptr, nullptr});
		Steps.Add({Name, 2.0f,
			[this]() { Feet.Reset(); OffDeckFrames = 0; },
			[this](float Dt) { Feet.Add(FeetInDeckSpace(), Dt); OffDeckFrames += IsPlayerOnDeck() ? 0 : 1; },
			[this, Name]()
			{
				const bool bPass = Feet.MaxDrift < 2.f && OffDeckFrames == 0;
				Report(Name, bPass, FString::Printf(TEXT("slideAfterStop=%.3fcm offDeckFrames=%d"), Feet.MaxDrift, OffDeckFrames));
			}});
	};
	Steps.Add({TEXT("WalkSetup"), 0.f, [this]() { PlacePlayerOnDeck(FVector(-360.f, -100.f, 0.f), 0.f); }, nullptr, nullptr});
	Settle(TEXT("SettleWalk"), 1.0f);
	Walk(TEXT("WalkForward"), FVector2D(0.f, 1.f), 1.0f);
	Stop(TEXT("StopAfterForward"));
	Walk(TEXT("StrafeRight"), FVector2D(1.f, 0.f), 0.8f);
	Stop(TEXT("StopAfterStrafe"));

	// 2b) Jump on the moving deck: land close to the take-off point in deck space.
	Steps.Add({TEXT("JumpSetup"), 0.f, [this]() { PlacePlayerOnDeck(FVector(-120.f, 60.f, 0.f), 0.f); }, nullptr, nullptr});
	Settle(TEXT("SettleJump"), 1.0f);
	Steps.Add({TEXT("JumpPress"), 0.1f,
		[this]() { MarkLocal = FeetInDeckSpace(); bWasAirborne = false; bLanded = false; if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugJump(true); } },
		[this](float) { bWasAirborne |= !IsPlayerOnDeck(); },
		[this]() { if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugJump(false); } }});
	Steps.Add({TEXT("JumpLand"), 1.5f, nullptr,
		[this](float)
		{
			const bool bOnDeck = IsPlayerOnDeck();
			bWasAirborne |= !bOnDeck;
			if (bWasAirborne && bOnDeck && !bLanded)
			{
				bLanded = true;
				LandLocal = FeetInDeckSpace();
			}
		},
		[this]()
		{
			const FVector D = LandLocal - MarkLocal;
			const float Planar = FVector(D.X, D.Y, 0.f).Size();
			const bool bPass = bWasAirborne && bLanded && Planar < 15.f;
			Report(TEXT("JumpOnMovingDeck"), bPass, FString::Printf(TEXT("airborne=%d landedOnDeck=%d landingOffset=%.2fcm"), bWasAirborne ? 1 : 0, bLanded ? 1 : 0, Planar));
		}});

	// 3) Use the helm, steer, leave: player ends on deck at the exit point.
	Steps.Add({TEXT("HelmApproach"), 0.f, [this]()
	{
		PlacePlayerOnDeck(FVector(-380.f, 0.f, 0.f), 0.f);
	}, nullptr, nullptr});
	Settle(TEXT("SettleHelm"), 1.0f);
	Steps.Add({TEXT("HelmAim"), 0.4f, nullptr, [this](float)
	{
		// Keep aiming at the helm while the deck moves.
		ASaltPlayerCharacter* P = GetPlayer();
		ASaltDevMovingPlatform* Plat = Platform.Get();
		if (P && Plat && P->GetController())
		{
			const FVector To = Plat->GetHelm()->GetComponentLocation() - P->GetCamera()->GetComponentLocation();
			P->GetController()->SetControlRotation(To.Rotation());
		}
	}, nullptr});
	Steps.Add({TEXT("HelmEnter"), 1.0f, [this]()
	{
		ASaltPlayerCharacter* P = GetPlayer();
		const bool bFocused = P && P->GetFocusedInteractable() == Platform->GetHelm();
		if (P) { P->DebugInteract(); }
		Report(TEXT("HelmFocus"), bFocused, FString::Printf(TEXT("focused=%s"), bFocused ? TEXT("helm") : *GetNameSafe(P ? P->GetFocusedInteractable() : nullptr)));
	}, nullptr, [this]()
	{
		ASaltPlayerCharacter* P = GetPlayer();
		const FVector Local = Platform->GetHelm()->GetComponentTransform().InverseTransformPosition(
			P->GetActorLocation() - FVector(0.f, 0.f, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		const float Err = (Local - Platform->GetHelm()->StandOffset).Size();
		Report(TEXT("HelmEnter"), P->IsAtStation() && Err < 1.f, FString::Printf(TEXT("atStation=%d standError=%.3fcm"), P->IsAtStation() ? 1 : 0, Err));
	}});
	Steps.Add({TEXT("HelmSteer"), 3.0f,
		[this]() { Platform->bUseForcedDrive = false; Eye.Reset(); if (ASaltPlayerCharacter* P = GetPlayer()) { P->SetDebugMoveInput(FVector2D(1.f, 1.f)); } },
		[this](float Dt) { Eye.Add(EyeInDeckSpace(), Dt); },
		[this]()
		{
			ASaltPlayerCharacter* P = GetPlayer();
			P->SetDebugMoveInput(FVector2D::ZeroVector);
			const bool bPass = Platform->GetSpeed() > 50.f && Eye.MaxAccelStep < 0.5f;
			Report(TEXT("HelmSteer"), bPass, FString::Printf(TEXT("platformSpeed=%.1fcm/s eyeJitterMax=%.4fcm"), Platform->GetSpeed(), Eye.MaxAccelStep));
		}});
	Steps.Add({TEXT("HelmLeave"), 1.5f, [this]() { if (ASaltPlayerCharacter* P = GetPlayer()) { P->DebugInteract(); } }, nullptr, [this]()
	{
		ASaltPlayerCharacter* P = GetPlayer();
		const FVector Local = Platform->GetHelm()->GetComponentTransform().InverseTransformPosition(
			P->GetActorLocation() - FVector(0.f, 0.f, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		const FVector Err = Local - Platform->GetHelm()->ExitOffset;
		const bool bPass = !P->IsAtStation() && IsPlayerOnDeck() && FVector(Err.X, Err.Y, 0.f).Size() < 5.f;
		Report(TEXT("HelmLeave"), bPass, FString::Printf(TEXT("onDeck=%d exitPlanarError=%.2fcm"), IsPlayerOnDeck() ? 1 : 0, FVector(Err.X, Err.Y, 0.f).Size()));
	}});

	Steps.Add({TEXT("Finish"), 0.f, [this, Cfg]()
	{
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST Suite DONE %s passed=%d failed=%d"), *Cfg, Passed, Failed);
	}, nullptr, nullptr});

	StepIndex = 0;
	StepTime = 0.f;
	bStepStarted = false;
}

void USaltDevTestSubsystem::SetFloatingCamera(ACameraActor* Camera, bool bFloat, float HeightAboveWater)
{
	FloatCamera = bFloat ? Camera : nullptr;
	FloatHeight = HeightAboveWater;
	bFloatInitialised = false;
}

void USaltDevTestSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (ACameraActor* Cam = FloatCamera.Get())
	{
		// Rides the water like a small boat: follows the surface height at the camera, low-pass filtered
		// (~0.6 s) so short waves do not shake the view.
		if (const USaltOceanSubsystem* Ocean = GetWorld()->GetSubsystem<USaltOceanSubsystem>())
		{
			const FVector L = Cam->GetActorLocation();
			const float Water = (float)Ocean->SampleWater(FVector2D(L.X, L.Y)).Location.Z;
			FloatWater = bFloatInitialised ? FMath::FInterpTo(FloatWater, Water, DeltaTime, 1.6f) : Water;
			bFloatInitialised = true;
			Cam->SetActorLocation(FVector(L.X, L.Y, FloatWater + FloatHeight));
		}
	}
	if (PerfRemaining > 0.f)
	{
		PerfFrame.Add(DeltaTime * 1000.f);
		PerfGpu.Add((float)FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
		PerfGame.Add((float)FPlatformTime::ToMilliseconds(GGameThreadTime));
		PerfRender.Add((float)FPlatformTime::ToMilliseconds(GRenderThreadTime));
		PerfRemaining -= DeltaTime;
		if (PerfRemaining <= 0.f)
		{
			float FA, FP, FM, GA, GP, GM, TA, TP, TM, RA, RP, RM;
			Stats(PerfFrame, FA, FP, FM);
			Stats(PerfGpu, GA, GP, GM);
			Stats(PerfGame, TA, TP, TM);
			Stats(PerfRender, RA, RP, RM);
			FIntPoint Res(0, 0);
			if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
			{
				Res = GEngine->GameViewport->Viewport->GetSizeXY();
			}
			const IConsoleVariable* SP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_PERF label=%s frames=%d fpsAvg=%.1f frameAvg=%.2fms frameP95=%.2fms frameMax=%.2fms gpuAvg=%.2fms gpuP95=%.2fms game=%.2fms render=%.2fms res=%dx%d screenPct=%.1f"),
				*PerfLabel, PerfFrame.Num(), FA > 0.f ? 1000.f / FA : 0.f, FA, FP, FM, GA, GP, TA, RA, Res.X, Res.Y, SP ? SP->GetFloat() : -1.f);
		}
	}
	// Zero-duration steps chain within one frame; timed steps tick once per frame.
	while (Steps.IsValidIndex(StepIndex))
	{
		FStep& Step = Steps[StepIndex];
		if (!bStepStarted)
		{
			bStepStarted = true;
			StepTime = 0.f;
			if (Step.OnStart) { Step.OnStart(); }
			if (Step.Duration > 0.f) { return; }
		}
		else
		{
			StepTime += DeltaTime;
			if (Step.OnTick) { Step.OnTick(DeltaTime); }
			if (StepTime < Step.Duration) { return; }
		}
		if (Step.OnEnd) { Step.OnEnd(); }
		++StepIndex;
		bStepStarted = false;
	}
	if (StepIndex != INDEX_NONE)
	{
		Steps.Reset();
		StepIndex = INDEX_NONE;
	}
}

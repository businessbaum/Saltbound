// Saltbound - the hub ship's harbour passages (D70; see SaltHubPassage.h).

#include "Hub/SaltHubPassage.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltMooringPoint.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"
#include "Hub/SaltHubBridge.h"
#include "Hub/SaltHubDoors.h"
#include "Hub/SaltHubShip.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"

namespace SaltPassageLocal
{
	/** The turn from the doorway into the canal (cm): the basin is 12 m wide. */
	constexpr float TurnRadius = 600.f;
	/** The path's points this far apart (cm). */
	constexpr float Step = 50.f;
	/** The hub's "seen" names (USaltSaveSubsystem's hub set): after these, the passages are the short ones. */
	const FName ArrivalSeen(TEXT("HubArrivalSeen"));
	const FName DepartureSeen(TEXT("HubDepartureSeen"));

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float U)
	{
		const float U2 = U * U;
		const float U3 = U2 * U;
		return 0.5f * ((2.f * P1) + (-P0 + P2) * U + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * U2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * U3);
	}
}

ASaltHubPassage::ASaltHubPassage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ASaltHubPassage* ASaltHubPassage::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ASaltHubPassage> It(World); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ASaltHubPassage>(ASaltHubPassage::StaticClass(), FTransform::Identity, Params);
}

ASaltHubDoors* ASaltHubPassage::FindDoorsFor(const UWorld* World, FName MooringId, const FVector& Near)
{
	ASaltHubDoors* Best = nullptr;
	double BestD = TNumericLimits<double>::Max();
	for (TActorIterator<ASaltHubDoors> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (!MooringId.IsNone() && It->MooringId == MooringId)
		{
			return *It;
		}
		const double D = FVector::Dist2D(It->GetActorLocation(), Near);
		if (D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

FTransform ASaltHubPassage::Frame() const
{
	const ASaltHubDoors* Doors = DoorsRef.Get();
	return Doors ? FTransform(FRotator(0.f, Doors->GetActorRotation().Yaw, 0.f), Doors->GetActorLocation()) : FTransform::Identity;
}

FVector ASaltHubPassage::LocalToWorld(const FVector& Local) const
{
	return Frame().TransformPosition(Local);
}

bool ASaltHubPassage::PlayerAboard(ASaltBoat* Boat) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!Player || !Boat)
	{
		return false;
	}
	const UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(Player->GetMovementBaseObject());
	if (Base && Base->GetOwner() == Boat)
	{
		return true;
	}
	const USaltStationComponent* Seat = Player->GetActiveStation();
	return Seat && Seat->GetOwner() == Boat;
}

bool ASaltHubPassage::StartArrival(ASaltHubDoors* Doors, ASaltBoat* Boat, int32 InSide)
{
	if (IsRunning() || !Doors || !Boat || !PlayerAboard(Boat))
	{
		return false;
	}
	return Begin(ESaltPassage::Arrival, Doors, Boat, InSide >= 0 ? 1 : -1);
}

bool ASaltHubPassage::AskDeparture(ASaltBoat* Boat, ASaltPlayerCharacter* Player)
{
	if (IsRunning() || !Boat)
	{
		return false;
	}
	ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr;
	if (!PC)
	{
		return StartDeparture(Boat);
	}
	TWeakObjectPtr<ASaltHubPassage> Self(this);
	TWeakObjectPtr<ASaltBoat> WeakBoat(Boat);
	PC->Ask(NSLOCTEXT("Saltbound", "HubDepartTitle", "Sail out?"),
		NSLOCTEXT("Saltbound", "HubDepartBody", "The sea doors open and she takes you out to sea."),
		NSLOCTEXT("Saltbound", "HubDepartYes", "Sail out"), NSLOCTEXT("Saltbound", "HubDepartNo", "Stay moored"),
		[Self, WeakBoat]()
		{
			if (Self.IsValid() && WeakBoat.IsValid())
			{
				Self->StartDeparture(WeakBoat.Get());
			}
		});
	return true;
}

bool ASaltHubPassage::StartDeparture(ASaltBoat* Boat)
{
	if (IsRunning() || !Boat)
	{
		return false;
	}
	ASaltHubDoors* Doors = FindDoorsFor(GetWorld(), Boat->GetMooring().Id, Boat->GetActorLocation());
	if (!Doors || !PlayerAboard(Boat))
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_PASSAGE departure refused: %s"), Doors ? TEXT("the player is not aboard") : TEXT("no doors"));
		return false;
	}
	const int32* Entered = EntrySide.Find(Doors->Station);
	return Begin(ESaltPassage::Departure, Doors, Boat, Entered ? *Entered : 1);
}

bool ASaltHubPassage::Begin(ESaltPassage InKind, ASaltHubDoors* Doors, ASaltBoat* Boat, int32 InSide)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!PC || !Player)
	{
		return false;
	}
	Kind = InKind;
	DoorsRef = Doors;
	BoatRef = Boat;
	PlayerRef = Player;
	Side = InSide;
	Station = Doors->Station;
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	const FName Seen = Kind == ESaltPassage::Arrival ? SaltPassageLocal::ArrivalSeen : SaltPassageLocal::DepartureSeen;
	bLong = ForceLong >= 0 ? ForceLong == 1 : !(Saves && Saves->IsUnlocked(Seen));
	Elapsed = PathTime = 0.f;
	bBridgeDown = bDoorsShut = bSailIn = bHanded = bLive = false;

	// Where she is now, in the doors' frame.
	const FTransform F = Frame();
	const FVector L = F.InverseTransformPosition(Boat->GetActorLocation());
	const float LocalYaw = FRotator::NormalizeAxis(Boat->GetYawDeg() - (float)F.Rotator().Yaw);
	const float Speed = (float)Boat->GetVelocity().Size2D();
	Path.Reset();
	if (Kind == ESaltPassage::Arrival)
	{
		BuildArrival(FVector2D(L.X, L.Y), LocalYaw, Speed);
		EntrySide.Add(Station, Side);
	}
	else
	{
		BuildDeparture();
	}
	TimePath(Kind == ESaltPassage::Arrival ? (bLong ? 18.f : 30.f) : (bLong ? 26.f : 35.f));
	PathDuration = Duration = Path.Num() ? Path.Last().T : 1.f;

	// The shots (positions and looks in the doors' frame, cm, y towards the side she comes or goes by; chase keys in her frame),
	// timed by where she is on her path: off the doorway, through it, in the turn, alongside.
	const float S = (float)Side;
	const float Hw = Doors->HullHalfCm;
	const FVector Dock = Doors->DockPose.GetLocation();
	auto When = [this](TFunctionRef<bool(const FPathPoint&)> Test)
	{
		for (const FPathPoint& Q : Path)
		{
			if (Test(Q))
			{
				return Q.T;
			}
		}
		return Path.Num() ? Path.Last().T : 0.f;
	};
	Keys.Reset();
	auto Key = [this](EKey K, float T, const FVector& Pos, const FVector& Look, float Fov = 60.f)
	{
		FCamKey C;
		C.Kind = K;
		C.T = FMath::Min(FMath::Max(T, Keys.Num() ? Keys.Last().T + 1.2f : 0.f), FMath::Max(Duration - 0.5f, 0.f));      // in order
		C.Pos = Pos;
		C.Look = Look;
		C.Fov = Fov;
		Keys.Add(C);
	};
	const FVector ChasePos(-1150.f, 0.f, 520.f), ChaseLook(1300.f, 0.f, 150.f);
	const FVector InsideHigh(-300.f, -S * 600.f, 1100.f);              // over the basin, inside the raised bridge's leaves
	Key(EKey::StartView, 0.f, FVector::ZeroVector, FVector::ZeroVector);
	if (Kind == ESaltPassage::Arrival)
	{
		const float TOut = When([Hw](const FPathPoint& Q) { return FMath::Abs(Q.P.Y) < Hw + 900.f; });       // 9 m off the hull
		const float TIn = When([Hw](const FPathPoint& Q) { return FMath::Abs(Q.P.Y) < Hw - 1000.f; });       // in the basin
		const float TTurn = When([](const FPathPoint& Q) { return FMath::Abs(Q.Yaw) < 45.f; });                // half round
		if (bLong)
		{
			// High and wide off her quarter: the hull's whole side, the open doorway, the boat heading in; behind her through
			// the doorway; high in the basin as she turns into the canal (the doors shutting behind her); over the canal ahead
			// of her as she comes alongside; the player's eyes.
			Key(EKey::FixedOnBoat, FMath::Max(3.f, TOut * 0.55f), FVector(-4800.f, S * (Hw + 6200.f), 3400.f), FVector(0.f, 0.f, 300.f), 55.f);
			Key(EKey::Chase, TOut, ChasePos, ChaseLook, 62.f);
			Key(EKey::Chase, TIn + 1.f, ChasePos, ChaseLook, 62.f);
			Key(EKey::FixedOnBoat, TTurn, InsideHigh, FVector(0.f, 0.f, 150.f), 60.f);
			Key(EKey::FixedOnBoat, Duration - 4.f, FVector(Dock.X + 900.f, Dock.Y + 250.f, 560.f), FVector(-100.f, 0.f, 150.f), 58.f);
		}
		else
		{
			Key(EKey::Chase, TOut, ChasePos, ChaseLook, 62.f);
			Key(EKey::Chase, TIn + 0.5f, ChasePos, ChaseLook, 62.f);
			Key(EKey::FixedOnBoat, TTurn, InsideHigh, FVector(0.f, 0.f, 150.f), 60.f);
		}
	}
	else
	{
		const float TIn = When([Hw](const FPathPoint& Q) { return FMath::Abs(Q.P.Y) > Hw - 1000.f; });       // nearing the doorway
		const float TOut = When([Hw](const FPathPoint& Q) { return FMath::Abs(Q.P.Y) > Hw + 800.f; });       // out of it
		if (bLong)
		{
			// High in the basin as she backs off and turns; behind her out through the doorway; high and wide off the hull ahead
			// of the doors, looking back at the ship as she sails clear; the player's eyes.
			Key(EKey::FixedOnBoat, FMath::Max(3.f, TIn * 0.5f), InsideHigh, FVector(0.f, 0.f, 150.f), 60.f);
			Key(EKey::Chase, TIn - 1.f, ChasePos, ChaseLook, 62.f);
			Key(EKey::Chase, TOut, ChasePos, ChaseLook, 62.f);
			Key(EKey::FixedOnBoat, TOut + 0.6f * (Duration - TOut), FVector(3800.f, S * (Hw + 4800.f), 2600.f), FVector(-600.f, 0.f, 300.f), 55.f);
		}
		else
		{
			Key(EKey::Chase, TIn - 0.5f, ChasePos, ChaseLook, 62.f);
			Key(EKey::Chase, TOut, ChasePos, ChaseLook, 62.f);
		}
	}
	Key(EKey::EndView, Duration, FVector::ZeroVector, FVector::ZeroVector);
	Keys.Last().T = Duration;

	// The player's view now is the first key: the camera starts exactly there.
	Start = PlayerView();
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Start.Pos, (Start.Look - Start.Pos).Rotation(), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		Camera->GetCameraComponent()->SetFieldOfView(Start.Fov);
		PC->SetViewTarget(Camera);
	}
	SetPlayerControl(false);

	// The boat is the passage's now; the bridge on her way rises; the doors open; the inside stays.
	Boat->SetScripted(true);
	if (Kind == ESaltPassage::Arrival)
	{
		Boat->SetSailSet(false);                  // she glides in under her way
	}
	if (ASaltHubBridge* Bridge = ASaltHubBridge::Find(World, Station, Side))
	{
		Bridge->Raise();
	}
	Doors->Open();
	if (ASaltHubShip* Ship = ASaltHubShip::Find(World))
	{
		Ship->SetInsideHeld(true);
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_PASSAGE start %s station=%d side=%d long=%d path=%.1fs points=%d"),
		Kind == ESaltPassage::Arrival ? TEXT("arrival") : TEXT("departure"), Station, Side, bLong ? 1 : 0, PathDuration, Path.Num());
	return true;
}

// --- the boat's path --------------------------------------------------------------------------------------------------------
void ASaltHubPassage::AddPoint(const FVector2D& P, float Yaw, float Speed)
{
	// Straight from the last point, in steps, her heading and speed changing evenly on the way.
	if (Path.Num() == 0)
	{
		FPathPoint Q;
		Q.P = P;
		Q.Yaw = Yaw;
		Q.Speed = Speed;
		Path.Add(Q);
		return;
	}
	const FPathPoint A = Path.Last();
	const float Len = (float)(P - A.P).Size();
	const int32 N = FMath::Max(1, FMath::CeilToInt(Len / SaltPassageLocal::Step));
	for (int32 K = 1; K <= N; ++K)
	{
		const float U = (float)K / N;
		FPathPoint Q;
		Q.P = FMath::Lerp(A.P, P, (double)U);
		Q.Yaw = A.Yaw + FRotator::NormalizeAxis(Yaw - A.Yaw) * U;
		Q.Speed = FMath::Lerp(A.Speed, Speed, U);
		Path.Add(Q);
	}
}

void ASaltHubPassage::BuildArrival(const FVector2D& StartLocal, float StartYaw, float StartSpeed)
{
	using namespace SaltPassageLocal;
	const ASaltHubDoors* Doors = DoorsRef.Get();
	const float S = (float)Side;
	const float Hw = Doors->HullHalfCm;
	const FVector Dock = Doors->DockPose.GetLocation();
	const float DockYaw = (float)Doors->DockPose.Rotator().Yaw;
	const float VIn = bLong ? 250.f : 450.f;
	const float VTurn = bLong ? 180.f : 340.f;
	const float In = -S * 90.f;
	// Where she is; lined up with the doorway; through it; across the basin to the turn; round into the canal; alongside.
	AddPoint(StartLocal, StartYaw, FMath::Clamp(StartSpeed, 120.f, 500.f));
	const float Lined = FMath::Max(Hw + 300.f, FMath::Min((float)FMath::Abs(StartLocal.Y) - 400.f, Hw + 900.f));
	AddPoint(FVector2D(0.0, S * Lined), In, VIn);
	AddPoint(FVector2D(0.0, S * (Hw - 300.f)), In, VIn);
	const float Ya = (float)Dock.Y + S * TurnRadius;
	AddPoint(FVector2D(0.0, Ya), In, VTurn);
	const FVector2D C(TurnRadius, Ya);
	for (int32 K = 1; K <= 18; ++K)
	{
		const float Phi = HALF_PI * K / 18.f;
		AddPoint(C + FVector2D(-FMath::Cos(Phi), -S * FMath::Sin(Phi)) * TurnRadius, In * (1.f - (float)K / 18.f), VTurn);
	}
	AddPoint(FVector2D(Dock.X - 300.0, Dock.Y), DockYaw, VTurn * 0.6f);
	AddPoint(FVector2D(Dock.X, Dock.Y), DockYaw, 0.f);
}

void ASaltHubPassage::BuildDeparture()
{
	using namespace SaltPassageLocal;
	const ASaltHubDoors* Doors = DoorsRef.Get();
	const float S = (float)Side;
	const float Hw = Doors->HullHalfCm;
	const FVector Dock = Doors->DockPose.GetLocation();
	const float DockYaw = (float)Doors->DockPose.Rotator().Yaw;
	const float Out = S * 90.f;
	// Astern off the berth into the basin; round to face the doorway; out through it; clear of the hull under sail.
	AddPoint(FVector2D(Dock.X, Dock.Y), DockYaw, bLong ? 90.f : 180.f);
	AddPoint(FVector2D(TurnRadius - 100.0, Dock.Y), DockYaw, bLong ? 140.f : 260.f);
	AddPoint(FVector2D(0.0, Dock.Y + S * 150.0), Out, bLong ? 120.f : 180.f);
	AddPoint(FVector2D(0.0, S * (Hw - 300.f)), Out, bLong ? 300.f : 420.f);
	AddPoint(FVector2D(0.0, S * (Hw + 1500.f)), Out, 380.f);
}

void ASaltHubPassage::TimePath(float TurnRateDeg)
{
	// Each step takes its length at the mean of its ends' speeds, or as long as its turn takes, whichever is longer.
	for (int32 I = 1; I < Path.Num(); ++I)
	{
		const FPathPoint& A = Path[I - 1];
		FPathPoint& B = Path[I];
		const float Len = (float)(B.P - A.P).Size();
		const float V = FMath::Max(0.5f * (A.Speed + B.Speed), 30.f);
		const float Turn = FMath::Abs(FRotator::NormalizeAxis(B.Yaw - A.Yaw)) / FMath::Max(TurnRateDeg, 1.f);
		B.T = A.T + FMath::Max(Len / V, Turn);
	}
}

void ASaltHubPassage::SamplePath(float T, FVector2D& OutP, float& OutYaw) const
{
	if (Path.Num() == 0)
	{
		return;
	}
	if (T <= 0.f || Path.Num() == 1)
	{
		OutP = Path[0].P;
		OutYaw = Path[0].Yaw;
		return;
	}
	int32 I = 1;
	while (I < Path.Num() - 1 && Path[I].T < T)
	{
		++I;
	}
	const FPathPoint& A = Path[I - 1];
	const FPathPoint& B = Path[I];
	const float U = FMath::Clamp((T - A.T) / FMath::Max(B.T - A.T, 1e-4f), 0.f, 1.f);
	OutP = FMath::Lerp(A.P, B.P, (double)U);
	OutYaw = A.Yaw + FRotator::NormalizeAxis(B.Yaw - A.Yaw) * U;
}

// --- the camera ---------------------------------------------------------------------------------------------------------------
ASaltHubPassage::FView ASaltHubPassage::PlayerView() const
{
	FView Out;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ASaltPlayerCharacter* Player = PlayerRef.Get();
	FMinimalViewInfo POV;
	if (PC && PC->GetViewTarget() == Player && PC->PlayerCameraManager)
	{
		POV = PC->PlayerCameraManager->GetCameraCacheView();          // what is on the screen now
	}
	else if (Player)
	{
		Player->CalcCamera(0.f, POV);                                  // the player's eyes as they would be seen
	}
	Out.Pos = POV.Location;
	Out.Look = POV.Location + POV.Rotation.Vector() * 1000.0;
	Out.Fov = POV.FOV;
	Out.Roll = (float)POV.Rotation.Roll;
	return Out;
}

ASaltHubPassage::FView ASaltHubPassage::EvaluateKey(const FCamKey& Key) const
{
	FView Out;
	Out.Fov = Key.Fov;
	const ASaltBoat* Boat = BoatRef.Get();
	const FTransform BoatFrame = Boat ? FTransform(FRotator(0.f, Boat->GetYawDeg(), 0.f), Boat->GetActorLocation()) : Frame();
	switch (Key.Kind)
	{
	case EKey::StartView:
		return Start;
	case EKey::EndView:
		return bLive ? Live : PlayerView();
	case EKey::Fixed:
		Out.Pos = LocalToWorld(Key.Pos);
		Out.Look = LocalToWorld(Key.Look);
		break;
	case EKey::FixedOnBoat:
		Out.Pos = LocalToWorld(Key.Pos);
		Out.Look = BoatFrame.TransformPosition(Key.Look);
		break;
	case EKey::Chase:
		Out.Pos = BoatFrame.TransformPosition(Key.Pos);
		Out.Look = BoatFrame.TransformPosition(Key.Look);
		break;
	}
	return Out;
}

void ASaltHubPassage::UpdateCamera(float DeltaSeconds)
{
	if (!Camera || Keys.Num() < 2)
	{
		return;
	}
	// The player's eyes (the last key) go on living: the character's camera is not asked by the engine while ours is shown.
	if (ASaltPlayerCharacter* Player = PlayerRef.Get())
	{
		FMinimalViewInfo POV;
		Player->CalcCamera(DeltaSeconds, POV);
		Live.Pos = POV.Location;
		Live.Look = POV.Location + POV.Rotation.Vector() * 1000.0;
		Live.Fov = POV.FOV;
		Live.Roll = (float)POV.Rotation.Roll;
		bLive = true;
	}
	const float T = FMath::Clamp(Elapsed, 0.f, Duration);
	int32 I = 0;
	while (I < Keys.Num() - 2 && Keys[I + 1].T <= T)
	{
		++I;
	}
	const float U = FMath::Clamp((T - Keys[I].T) / FMath::Max(Keys[I + 1].T - Keys[I].T, 1e-3f), 0.f, 1.f);
	// Catmull-Rom through the keys as they are now; the ends are held (no speed at the player's view at either end).
	const FView V1 = EvaluateKey(Keys[I]);
	const FView V2 = EvaluateKey(Keys[I + 1]);
	const FView V0 = I > 0 ? EvaluateKey(Keys[I - 1]) : V2;
	const FView V3 = I + 2 < Keys.Num() ? EvaluateKey(Keys[I + 2]) : V1;
	const FVector Pos = SaltPassageLocal::CatmullRom(V0.Pos, V1.Pos, V2.Pos, V3.Pos, U);
	const FVector Look = SaltPassageLocal::CatmullRom(V0.Look, V1.Look, V2.Look, V3.Look, U);
	const float Ease = U * U * (3.f - 2.f * U);
	FRotator Rot = FRotationMatrix::MakeFromX((Look - Pos).GetSafeNormal()).Rotator();
	Rot.Roll = FMath::Lerp(V1.Roll, V2.Roll, Ease);                       // the deck's lean at the player's ends only
	Camera->SetActorLocationAndRotation(Pos, Rot);
	Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(V1.Fov, V2.Fov, Ease));

	// The last stretch ends at the player's eyes: point them first (along her bow, a little down), so they hold still while
	// the camera comes to them.
	if (!bHanded && I == Keys.Num() - 2)
	{
		bHanded = true;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			const ASaltBoat* Boat = BoatRef.Get();
			float Yaw = Boat ? Boat->GetYawDeg() : (float)PC->GetControlRotation().Yaw;
			if (Kind == ESaltPassage::Arrival && DoorsRef.IsValid())
			{
				Yaw = (float)DoorsRef->GetActorRotation().Yaw + (float)DoorsRef->DockPose.Rotator().Yaw + 15.f;   // along the canal
			}
			else if (DoorsRef.IsValid())
			{
				Yaw = (float)DoorsRef->GetActorRotation().Yaw + Side * 90.f;                                       // out to sea
			}
			PC->SetControlRotation(FRotator(-6.f, Yaw, 0.f));
		}
	}
}

// --- the passage ---------------------------------------------------------------------------------------------------------------
void ASaltHubPassage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Kind == ESaltPassage::None)
	{
		return;
	}
	ASaltBoat* Boat = BoatRef.Get();
	ASaltHubDoors* Doors = DoorsRef.Get();
	if (!Boat || !Doors)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_PASSAGE lost its boat or doors"));
		Finish();
		return;
	}
	// The boat waits where she must: at the doorway until the doors stand open and the harbour is there.
	const float Hw = Doors->HullHalfCm;
	FVector2D Next;
	float NextYaw = 0.f;
	SamplePath(PathTime + DeltaSeconds, Next, NextYaw);
	bool bWait = false;
	FVector2D Now;
	float NowYaw = 0.f;
	SamplePath(PathTime, Now, NowYaw);
	if (Kind == ESaltPassage::Arrival)
	{
		// Still outside (or in the doorway) and about to go in.
		bWait = FMath::Abs(Now.Y) > Hw - 300.f && FMath::Abs(Next.Y) < Hw + 500.f && (!Doors->IsOpen() || !ASaltHubShip::IsInsideReady(GetWorld()));
	}
	else
	{
		// Still inside and about to go out: she waits short of the leaves' swing (they reach 12 m in from the hull's side and
		// never swing into a boat), with her bow 5 m ahead of her middle.
		bWait = FMath::Abs(Now.Y) < Hw - 300.f && FMath::Abs(Next.Y) > Hw - 1250.f && !Doors->IsOpen();
	}
	if (bWait)
	{
		Doors->Open();                                   // asked again: a boat in the swing may have held them
	}
	if (!bWait)
	{
		PathTime = FMath::Min(PathTime + DeltaSeconds, PathDuration);
		Elapsed = PathTime;
	}
	FVector2D P;
	float Yaw = 0.f;
	SamplePath(PathTime, P, Yaw);
	const FVector World = LocalToWorld(FVector(P.X, P.Y, 0.0));
	Boat->SetScriptedPose(World, (float)Frame().Rotator().Yaw + Yaw, bWait ? 0.f : DeltaSeconds);
	Events();
	UpdateCamera(DeltaSeconds);
	if (PathTime >= PathDuration)
	{
		Finish();
	}
}

void ASaltHubPassage::Events()
{
	ASaltBoat* Boat = BoatRef.Get();
	ASaltHubDoors* Doors = DoorsRef.Get();
	const FVector L = Frame().InverseTransformPosition(Boat->GetActorLocation());
	const float Hw = Doors->HullHalfCm;
	if (Kind == ESaltPassage::Arrival)
	{
		// The doors shut behind her once she is clear of their swing; the bridge comes down as she comes alongside.
		if (!bDoorsShut && FMath::Abs(L.Y) < Hw - 900.f && Doors->Close())
		{
			bDoorsShut = true;
		}
		if (!bBridgeDown && PathTime > PathDuration - 2.5f)
		{
			bBridgeDown = true;
			if (ASaltHubBridge* Bridge = ASaltHubBridge::Find(GetWorld(), Station, Side))
			{
				Bridge->Lower();
			}
		}
	}
	else
	{
		// The sail falls as she passes the doorway; the bridge comes down and the doors shut once she is out.
		if (!bSailIn && FMath::Abs(L.Y) > Hw - 300.f)
		{
			bSailIn = true;
			Boat->SetSailSet(true);
		}
		if (!bBridgeDown && FMath::Abs(L.Y) > Hw)
		{
			bBridgeDown = true;
			if (ASaltHubBridge* Bridge = ASaltHubBridge::Find(GetWorld(), Station, Side))
			{
				Bridge->Lower();
			}
		}
		if (!bDoorsShut && FMath::Abs(L.Y) > Hw + 1000.f && Doors->Close())
		{
			bDoorsShut = true;
		}
	}
}

void ASaltHubPassage::Finish()
{
	UWorld* World = GetWorld();
	ASaltBoat* Boat = BoatRef.Get();
	const ESaltPassage Was = Kind;
	Kind = ESaltPassage::None;
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (Boat)
	{
		Boat->SetScripted(false);
		if (Was == ESaltPassage::Arrival)
		{
			// Moored at the docking station; the end of a voyage saves the game (as the windlass does, D46).
			FSaltMooring Mooring;
			const FName Id = DoorsRef.IsValid() ? DoorsRef->MooringId : NAME_None;
			for (TObjectIterator<USaltMooringPointComponent> It; It; ++It)
			{
				if (It->GetWorld() == World && It->MooringId == Id)
				{
					Mooring = It->GetMooring();
					break;
				}
			}
			if (Mooring.IsValid())
			{
				Boat->MoorAt(Mooring);
			}
			if (Saves)
			{
				Saves->SetUnlocked(SaltPassageLocal::ArrivalSeen, true);
				Saves->Autosave(World, TEXT("moored"));
			}
		}
		else
		{
			// Out at sea, under way and under sail: hers again.
			FVector2D P;
			float Yaw = 0.f;
			SamplePath(PathDuration, P, Yaw);
			Boat->PlaceAt(LocalToWorld(FVector(P.X, P.Y, 0.0)), (float)Frame().Rotator().Yaw + Yaw, Path.Num() ? Path.Last().Speed : 300.f);
			Boat->SetSailSet(true);
			if (Saves)
			{
				Saves->SetUnlocked(SaltPassageLocal::DepartureSeen, true);
			}
		}
	}
	if (ASaltHubBridge* Bridge = ASaltHubBridge::Find(World, Station, Side))
	{
		Bridge->Lower();
	}
	// Back to the player's eyes (the camera is already there) and their hands.
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PlayerRef.IsValid())
	{
		PC->SetViewTargetWithBlend(PlayerRef.Get(), 0.2f, VTBlend_EaseInOut);
	}
	if (Camera)
	{
		Camera->SetLifeSpan(1.f);
		Camera = nullptr;
	}
	SetPlayerControl(true);
	if (ASaltHubShip* Ship = ASaltHubShip::Find(World))
	{
		Ship->SetInsideHeld(false);
	}
	LastDescription = FString::Printf(TEXT("kind=%s station=%d side=%d long=%d seconds=%.1f moored=%s"),
		Was == ESaltPassage::Arrival ? TEXT("arrival") : TEXT("departure"), Station, Side, bLong ? 1 : 0, PathDuration,
		Boat && Boat->GetHold() == ESaltBoatHold::Moored ? *Boat->GetMooring().Id.ToString() : TEXT("-"));
	UE_LOG(LogSaltbound, Display, TEXT("SB_PASSAGE done %s"), *LastDescription);
}

FString ASaltHubPassage::Describe() const
{
	if (IsRunning())
	{
		return FString::Printf(TEXT("running kind=%s station=%d side=%d long=%d t=%.1f/%.1f"), Kind == ESaltPassage::Arrival ? TEXT("arrival") : TEXT("departure"),
			Station, Side, bLong ? 1 : 0, PathTime, PathDuration);
	}
	return LastDescription;
}

void ASaltHubPassage::SetPlayerControl(bool bOn)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	PC->SetIgnoreMoveInput(!bOn);
	PC->SetIgnoreLookInput(!bOn);
	if (ASaltPlayerCharacter* Player = PlayerRef.Get())
	{
		if (bOn)
		{
			Player->EnableInput(PC);
		}
		else
		{
			Player->DisableInput(PC);
		}
	}
	if (ASaltPlayerController* SPC = Cast<ASaltPlayerController>(PC))
	{
		SPC->SetHUDVisible(bOn);
	}
}

// --- dev commands ----------------------------------------------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
namespace SaltPassageCommands
{
	FAutoConsoleCommandWithWorldAndArgs CmdPassage(
		TEXT("sb.Hub.Passage"),
		TEXT("sb.Hub.Passage arrive <station 0|1> <side 1|-1> [long|short] | depart [long|short] | info: the harbour's passages (D70). arrive puts the boat "
			"and the player off the doors and runs the arrival; depart runs the departure from the berth she lies at (no question)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltHubPassage* Passage = ASaltHubPassage::Get(World);
			ASaltBoat* Boat = nullptr;
			for (TActorIterator<ASaltBoat> It(World); It; ++It)
			{
				Boat = *It;
				break;
			}
			const FString Cmd = Args.Num() ? Args[0].ToLower() : TEXT("info");
			auto Length = [&Args, Passage](int32 At)
			{
				Passage->ForceLong = Args.Num() > At ? (Args[At].Equals(TEXT("long"), ESearchCase::IgnoreCase) ? 1 : 0) : -1;
			};
			if (!Passage || !Boat || Cmd == TEXT("info"))
			{
				UE_LOG(LogSaltbound, Display, TEXT("SB_PASSAGE info %s"), Passage ? *Passage->Describe() : TEXT("-"));
				return;
			}
			if (Cmd == TEXT("arrive"))
			{
				const int32 St = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
				const int32 Sd = Args.Num() > 2 && FCString::Atoi(*Args[2]) < 0 ? -1 : 1;
				ASaltHubDoors* Doors = nullptr;
				for (TActorIterator<ASaltHubDoors> It(World); It; ++It)
				{
					if (It->Station == St)
					{
						Doors = *It;
					}
				}
				APlayerController* PC = World->GetFirstPlayerController();
				ASaltPlayerCharacter* Player = PC ? Cast<ASaltPlayerCharacter>(PC->GetPawn()) : nullptr;
				if (!Doors || !Player)
				{
					UE_LOG(LogSaltbound, Warning, TEXT("SB_PASSAGE no doors at station %d or no player"), St);
					return;
				}
				if (ASaltHubShip* Ship = ASaltHubShip::Find(World))
				{
					Ship->RequireInside(true);
				}
				Length(3);
				// Off the doorway, heading in, under way; the player on her deck; the doors open.
				const FTransform F(FRotator(0.f, Doors->GetActorRotation().Yaw, 0.f), Doors->GetActorLocation());
				const FVector At = F.TransformPosition(FVector(0.0, Sd * (Doors->HullHalfCm + 1500.0), 0.0));
				Boat->SetHold(ESaltBoatHold::Free);
				Boat->PlaceAt(At, (float)F.Rotator().Yaw - Sd * 90.f, 250.f);
				Player->TeleportFeetTo(Boat->GetActorLocation() + FVector(0.0, 0.0, 110.0), Boat->GetYawDeg());
				Doors->SetOpenAmountNow(1.f);
				World->GetTimerManager().SetTimerForNextTick([Passage, Doors, Boat, Sd]()
				{
					const bool bOk = Passage->StartArrival(Doors, Boat, Sd);
					UE_LOG(LogSaltbound, Display, TEXT("SB_PASSAGE arrive started=%d"), bOk ? 1 : 0);
				});
				return;
			}
			if (Cmd == TEXT("depart"))
			{
				Length(1);
				const bool bOk = Passage->StartDeparture(Boat);
				UE_LOG(LogSaltbound, Display, TEXT("SB_PASSAGE depart started=%d"), bOk ? 1 : 0);
			}
		}));
}
#endif

// Saltbound - the sailing boat: a kinematic rigid body moved by our own integrator (D7, D36).

#include "Boat/SaltBoat.h"

#include "Boat/SaltHullShape.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Interaction/SaltStationComponent.h"
#include "Ocean/SaltOcean.h"
#include "Ocean/SaltOceanSubsystem.h"
#include "Saltbound.h"
#include "World/SaltWindSubsystem.h"

namespace
{
	constexpr double RhoWater = 1025.0;   // kg/m3
	constexpr double RhoAir = 1.225;
	constexpr double Gravity = 9.80665;
	constexpr float MaxSubstep = 1.f / 120.f;
	constexpr double MsToKnots = 1.943844;
	/** Contact search reaches this far beyond the collision capsules (m), so contacts exist before they touch. */
	constexpr float ContactMargin = 0.15f;
	/** Line tension cap (N): mooring pulls a displaced boat in gently instead of snapping it back. */
	constexpr double MaxLineTension = 2500.0;

	/** Soft sail: lift peaks at about 25 degrees angle of attack and fades to zero with the sail square to the
	 *  wind, where only drag (pushing downwind) remains. */
	void SailCoefficients(float Alpha, float& CL, float& CD)
	{
		const float A = FMath::Clamp(Alpha, 0.f, HALF_PI);
		const float Peak = FMath::DegreesToRadians(25.f);
		CL = A < Peak ? 1.4f * FMath::Sin(HALF_PI * A / Peak) : 1.4f * FMath::Cos(HALF_PI * (A - Peak) / (HALF_PI - Peak));
		CD = 0.08f + 1.2f * FMath::Square(FMath::Sin(A));
	}

	/** Rudder blade: linear lift up to the stall at 25 degrees, flat-plate behaviour beyond. */
	void RudderCoefficients(float Alpha, float& CL, float& CD)
	{
		const float A = FMath::Clamp(Alpha, 0.f, HALF_PI);
		const float Stall = FMath::DegreesToRadians(25.f);
		CL = A < Stall ? 1.2f * FMath::Sin(HALF_PI * A / Stall) : 0.9f * FMath::Sin(2.f * A);
		CD = 0.02f + 1.1f * FMath::Square(FMath::Sin(A));
	}

	/**
	 * Force on a thin symmetric foil (body XY plane) whose chord lies along Chord, in a fluid of density Rho that
	 * flows past it at Flow (fluid velocity relative to the foil). Lift is perpendicular to the flow, on the side
	 * the flow pushes the foil to; drag acts along the flow.
	 */
	template <typename CoeffFn>
	FVector2D FoilForce(const FVector2D& Flow, const FVector2D& Chord, double Area, double Rho, CoeffFn Coeffs, float* OutAlpha = nullptr)
	{
		const double Speed = Flow.Size();
		if (Speed < 1e-3 || Area <= 0.0)
		{
			if (OutAlpha) { *OutAlpha = 0.f; }
			return FVector2D::ZeroVector;
		}
		const FVector2D F = Flow / Speed;
		const FVector2D C = Chord.GetSafeNormal();
		const float Alpha = FMath::Acos(FMath::Clamp((float)FMath::Abs(F | C), 0.f, 1.f));
		FVector2D N(-C.Y, C.X);
		if ((F | N) < 0.0)
		{
			N = -N;
		}
		FVector2D LiftDir = N - F * (N | F);
		LiftDir = LiftDir.SizeSquared() > 1e-12 ? LiftDir.GetSafeNormal() : FVector2D::ZeroVector;
		float CL, CD;
		Coeffs(Alpha, CL, CD);
		if (OutAlpha) { *OutAlpha = Alpha; }
		return (LiftDir * CL + F * CD) * (0.5 * Rho * Speed * Speed * Area);
	}

	/** Capsule shape and pose (cm) for a body-space segment. */
	void CapsulePose(const FVector& A, const FVector& B, float Radius, float Margin, FVector& OutCentre, FQuat& OutRot, FCollisionShape& OutShape)
	{
		const FVector D = B - A;
		const double Len = D.Size();
		OutCentre = (A + B) * 50.0;
		OutRot = Len > 1e-6 ? FQuat::FindBetweenNormals(FVector::UpVector, D / Len) : FQuat::Identity;
		OutShape = FCollisionShape::MakeCapsule((Radius + Margin) * 100.f, (float)(Len * 0.5 + Radius + Margin) * 100.f);
	}

	ASaltBoat* FindBoat(UWorld* World)
	{
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdBoatReset(
		TEXT("sb.Boat.Reset"),
		TEXT("Return the boat to its mooring (start position), sail lowered."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASaltBoat* Boat = FindBoat(World)) { Boat->ResetToStart(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBoatRecover(
		TEXT("sb.Boat.Recover"),
		TEXT("Put the boat upright at its last safe position, at rest."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASaltBoat* Boat = FindBoat(World)) { Boat->Recover(); }
		}));

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorldAndArgs CmdBoatInfo(
		TEXT("sb.Boat.Info"),
		TEXT("Log the boat's state (SB_BOAT line)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASaltBoat* Boat = FindBoat(World)) { UE_LOG(LogSaltbound, Display, TEXT("%s"), *Boat->Describe()); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBoatControl(
		TEXT("sb.Boat.Control"),
		TEXT("Fixed controls for tests: sb.Boat.Control <Rudder -1..1> <Sheet 0..1> [Sail 0|1]. sb.Boat.Control off returns to the helm."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltBoat* Boat = FindBoat(World);
			if (!Boat || Args.Num() < 1)
			{
				return;
			}
			if (Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase))
			{
				Boat->SetControlOverride(false);
				return;
			}
			Boat->SetControlOverride(true, FCString::Atof(*Args[0]), Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.5f);
			if (Args.Num() > 2)
			{
				Boat->SetSailRaised(FCString::Atoi(*Args[2]) != 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBoatPlace(
		TEXT("sb.Boat.Place"),
		TEXT("Place the boat (free, at the water surface): sb.Boat.Place <X> <Y> <Yaw> [Speed m/s]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltBoat* Boat = FindBoat(World);
			if (Boat && Args.Num() >= 3)
			{
				Boat->SetHold(ESaltBoatHold::Free);
				Boat->PlaceAt(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), 0.f), FCString::Atof(*Args[2]),
					Args.Num() > 3 ? FCString::Atof(*Args[3]) * 100.f : 0.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBoatHold(
		TEXT("sb.Boat.Hold"),
		TEXT("sb.Boat.Hold Free|Moored|Anchored"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASaltBoat* Boat = FindBoat(World);
			if (Boat && Args.Num() >= 1)
			{
				const FString& H = Args[0];
				Boat->SetHold(H.Equals(TEXT("Moored"), ESearchCase::IgnoreCase) ? ESaltBoatHold::Moored
					: H.Equals(TEXT("Anchored"), ESearchCase::IgnoreCase) ? ESaltBoatHold::Anchored : ESaltBoatHold::Free);
			}
		}));
#endif
}

ASaltBoat::ASaltBoat()
{
	using namespace SaltHull;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetMobility(EComponentMobility::Movable);
	Hull->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	SetRootComponent(Hull);

	auto Part = [this](FName Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		return C;
	};
	auto Pivot = [this](FName Name, USceneComponent* Parent, const FVector& LocationM)
	{
		USceneComponent* C = CreateDefaultSubobject<USceneComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetRelativeLocation(LocationM * 100.0);
		return C;
	};

	BoomPivot = Pivot(TEXT("BoomPivot"), Hull, FVector(MastX, 0.f, SheerZ(MastX) + BoomHeight));
	Boom = Part(TEXT("Boom"), BoomPivot);
	Sail = Part(TEXT("Sail"), BoomPivot);
	GaffPivot = Pivot(TEXT("GaffPivot"), BoomPivot, FVector(0.f, 0.f, ThroatHeight - BoomHeight));
	Gaff = Part(TEXT("Gaff"), GaffPivot);
	// The rudder turns about its raked stock: the pivot's +Z runs along the stock.
	RudderPivot = Pivot(TEXT("RudderPivot"), Hull, FVector(RudderHeelX, 0.f, RudderHeelZ));
	RudderPivot->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Atan2(RudderHeelX - RudderHeadX, RudderHeadZ - RudderHeelZ)), 0.f, 0.f));
	Rudder = Part(TEXT("Rudder"), RudderPivot);
	const FVector Hub(WheelX, 0.f, SheerZ(WheelX) + WheelHeight);
	Wheel = Part(TEXT("Wheel"), Hull);
	Wheel->SetRelativeLocation(Hub * 100.0);

	Helm = CreateDefaultSubobject<USaltStationComponent>(TEXT("Helm"));
	Helm->SetupAttachment(Hull);
	Helm->SetRelativeLocation(Hub * 100.0);
	Helm->SetBoxExtent(FVector(18.f, 52.f, 52.f));
	Helm->StandOffset = (FVector(HelmsmanX, 0.f, SheerZ(HelmsmanX)) - Hub) * 100.0;
	Helm->ExitOffset = (FVector(HelmsmanX + 0.25f, 0.62f, SheerZ(HelmsmanX + 0.25f)) - Hub) * 100.0;
	Helm->YawLimit = 160.f;
	Helm->PitchMin = -50.f;
	Helm->PitchMax = 75.f;
	Helm->PromptText = NSLOCTEXT("Saltbound", "BoatHelmUse", "Take the helm");
	Helm->LeavePromptText = NSLOCTEXT("Saltbound", "BoatHelmLeave", "Leave the helm");

	Halyard = CreateDefaultSubobject<USaltInteractableComponent>(TEXT("Halyard"));
	Halyard->SetupAttachment(Hull);
	Halyard->SetRelativeLocation(FVector(MastX - 0.2f, 0.f, SheerZ(MastX) + 1.1f) * 100.0);
	Halyard->SetBoxExtent(FVector(22.f, 28.f, 55.f));
	Halyard->PromptText = NSLOCTEXT("Saltbound", "BoatRaiseSail", "Raise the sail");
}

void ASaltBoat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveContent();
	// The editor shows the boat as it starts: sail lowered onto the boom.
	UpdateVisuals(0.f);
}

void ASaltBoat::ResolveContent()
{
	auto Resolve = [](UStaticMeshComponent* C, const TCHAR* Part)
	{
		if (C && !C->GetStaticMesh())
		{
			C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *SaltContent::BoatMesh(Part)));
		}
	};
	Resolve(Hull, TEXT("Hull"));
	Resolve(Boom, TEXT("Boom"));
	Resolve(Sail, TEXT("Sail"));
	Resolve(Gaff, TEXT("Gaff"));
	Resolve(Rudder, TEXT("Rudder"));
	Resolve(Wheel, TEXT("Wheel"));
}

void ASaltBoat::BeginPlay()
{
	Super::BeginPlay();
	ResolveContent();
	BuildHullModel();
	StartTransform = GetActorTransform();
	SetPose(GetActorLocation() * 0.01, GetActorQuat());
	Helm->OnAction.AddDynamic(this, &ASaltBoat::HandleHelmAction);
	Halyard->OnInteract.AddDynamic(this, &ASaltBoat::HandleHalyard);
	SetHold(StartHold);
	bInitialised = true;
	UE_LOG(LogSaltbound, Log, TEXT("Boat %s: mass %.0f kg, displacement %.2f m3, centre of mass (%.2f, %.2f, %.2f) m, %d columns, %d capsules"),
		*GetName(), Mass, DesignVolume, CoM.X, CoM.Y, CoM.Z, Columns.Num(), Capsules.Num());
}

void ASaltBoat::BuildHullModel()
{
	using namespace SaltHull;
	Columns.Reset();
	Segments.Reset();
	Capsules.Reset();

	// Buoyancy columns: the deck plan in strips (10 along, 6 across), each from the hull surface up to the deck.
	constexpr int32 NX = 10;
	constexpr int32 NY = 6;
	const float Dx = 2.f * HalfLength / NX;
	double Vol = 0.0, MomX = 0.0;
	auto AddColumn = [&](const FVector& Bottom, float Height, float Area)
	{
		FColumn C;
		C.Bottom = Bottom;
		C.Height = Height;
		C.Area = Area;
		Columns.Add(C);
		const double Submerged = FMath::Clamp(-Bottom.Z, 0.f, Height);
		Vol += Area * Submerged;
		MomX += Area * Submerged * Bottom.X;
	};
	for (int32 I = 0; I < NX; ++I)
	{
		const float Xc = -HalfLength + (I + 0.5f) * Dx;
		const float Bs = HalfBeamAtSheer(Xc);
		const float Zs = SheerZ(Xc);
		const float Dy = 2.f * Bs / NY;
		for (int32 J = 0; J < NY && Bs > 0.05f; ++J)
		{
			const float Yc = -Bs + (J + 0.5f) * Dy;
			const float Zb = SectionZ(Xc, Yc);
			if (Zb < Zs - 0.01f)
			{
				AddColumn(FVector(Xc, Yc, Zb), Zs - Zb, Dx * Dy);
			}
		}
		// Lateral plane (hull and keel below the waterline) for keel lift and crossflow drag.
		const float Depth = -DeepestZ(Xc);
		if (Depth > 0.02f)
		{
			Segments.Add({FVector(Xc, 0.f, -Depth * 0.5f), Dx * Depth});
		}
	}
	// Keel: narrow columns between the keel bottom and the canoe body.
	constexpr int32 NK = 8;
	const float Dk = (KeelForeX - KeelAftX) / NK;
	for (int32 I = 0; I < NK; ++I)
	{
		const float Xc = KeelAftX + (I + 0.5f) * Dk;
		const float Zk = KeelZ(Xc);
		const float Zb = BottomZ(Xc);
		if (Zb - Zk > 0.02f)
		{
			AddColumn(FVector(Xc, 0.f, Zk), Zb - Zk, Dk * 2.f * KeelHalfThickness);
		}
	}
	DesignVolume = FMath::Max((float)Vol, 0.1f);
	Mass = RhoWater * DesignVolume;
	CoM = FVector(MomX / FMath::Max(Vol, 1e-3), 0.0, CentreOfMassHeight);

	KeelArea = 0.f;
	FVector Sum = FVector::ZeroVector;
	for (const FSegment& S : Segments)
	{
		KeelArea += S.Area;
		Sum += S.Point * S.Area;
	}
	KeelPoint = KeelArea > 0.f ? Sum / KeelArea : FVector::ZeroVector;

	// Sail centre of effort: centroid of the gaff sail (tack, throat, peak, clew) in the boom's frame.
	{
		const float GaffRad = FMath::DegreesToRadians(GaffAngleDeg);
		const FVector2D P[4] = { FVector2D(0.0, 0.0), FVector2D(0.0, ThroatHeight - BoomHeight),
			FVector2D(GaffLength * FMath::Cos(GaffRad), ThroatHeight - BoomHeight + GaffLength * FMath::Sin(GaffRad)),
			FVector2D(BoomLength - 0.15f, 0.0) };
		double Area2 = 0.0;
		FVector2D C = FVector2D::ZeroVector;
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D& A = P[K];
			const FVector2D& B = P[(K + 1) % 4];
			const double Cross = A.X * B.Y - B.X * A.Y;
			Area2 += Cross;
			C += (A + B) * Cross;
		}
		if (FMath::Abs(Area2) > 1e-6)
		{
			C /= 3.0 * Area2;
			SailCEAft = C.X;
			SailCEUp = C.Y;
		}
		UE_LOG(LogSaltbound, Log, TEXT("Boat sail: geometric area %.1f m2 (simulated %.1f m2), centre of effort %.2f m aft, %.2f m up"),
			FMath::Abs(Area2) * 0.5, SailArea, SailCEAft, SailCEUp);
	}

	// Collision capsules: a ring just above the waterline, a ring at the bulwark, the keel and the bilges. Each
	// capsule is inset by its radius so its surface lies on the hull.
	auto Ring = [this](TFunctionRef<float(float)> HeightAt, TFunctionRef<float(float, float)> HalfAt, float Radius)
	{
		const float Xs[] = { 3.8f, 2.4f, 0.8f, -0.8f, -2.4f, -3.8f };
		// The stem / stern tip at this height: where the profile reaches it (searched along X).
		auto TipX = [&](float Sign)
		{
			float X0 = 3.f;
			for (float Xt = 3.f; Xt <= HalfLength; Xt += 0.02f)
			{
				if (HalfAt(Sign * Xt, HeightAt(Sign * Xt)) <= 0.02f) { break; }
				X0 = Xt;
			}
			return Sign * FMath::Max(X0 - Radius, 0.f);
		};
		TArray<FVector> Pts;
		const float Fore = TipX(1.f);
		const float Aft = TipX(-1.f);
		Pts.Add(FVector(Fore, 0.f, HeightAt(Fore)));
		for (float Xp : Xs)
		{
			Pts.Add(FVector(Xp, FMath::Max(HalfAt(Xp, HeightAt(Xp)) - Radius, 0.f), HeightAt(Xp)));
		}
		Pts.Add(FVector(Aft, 0.f, HeightAt(Aft)));
		for (int32 K = UE_ARRAY_COUNT(Xs) - 1; K >= 0; --K)
		{
			const float Xp = Xs[K];
			Pts.Add(FVector(Xp, -FMath::Max(HalfAt(Xp, HeightAt(Xp)) - Radius, 0.f), HeightAt(Xp)));
		}
		for (int32 K = 0; K < Pts.Num(); ++K)
		{
			Capsules.Add({Pts[K], Pts[(K + 1) % Pts.Num()], Radius});
		}
	};
	Ring([](float) { return 0.25f; }, [](float Xp, float Z) { return HalfBreadthAt(Xp, Z); }, 0.25f);
	Ring([](float) { return 0.7f; }, [](float Xp, float Z) { return HalfBreadthAt(Xp, Z); }, 0.25f);
	Ring([](float Xp) { return SheerZ(Xp) + BulwarkHeight * 0.5f; }, [](float Xp, float) { return HalfBeamAtSheer(Xp); }, 0.22f);
	const float KeelR = 0.16f;
	Capsules.Add({FVector(KeelForeX - 0.4f, 0.f, KeelZ(KeelForeX - 0.4f) + KeelR), FVector(KeelAftX + KeelR, 0.f, KeelAftZ + KeelR), KeelR});
	for (float Side : { -1.f, 1.f })
	{
		const float BilgeZ = -0.35f, BilgeR = 0.25f, Bx = 2.6f;
		const float Y = FMath::Min(HalfBreadthAt(Bx, BilgeZ), HalfBreadthAt(-Bx, BilgeZ)) - BilgeR;
		Capsules.Add({FVector(Bx, Side * Y, BilgeZ), FVector(-Bx, Side * Y, BilgeZ), BilgeR});
	}
}

void ASaltBoat::SetPose(const FVector& OriginWorldM, const FQuat& Rotation)
{
	Q = Rotation.GetNormalized();
	X = OriginWorldM + Q.RotateVector(CoM);
	for (FColumn& C : Columns)
	{
		C.bHasWater = false;
	}
	Contacts.Reset();
}

void ASaltBoat::ClearMotion()
{
	V = W = AngVel = FVector::ZeroVector;
}

void ASaltBoat::PlaceAt(const FVector& Location, float Yaw, float ForwardSpeed)
{
	const USaltOceanSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<USaltOceanSubsystem>() : nullptr;
	const double WaterZ = Sub ? Sub->SampleWater(FVector2D(Location.X, Location.Y)).Location.Z : 0.0;
	const FQuat Rot = FRotator(0.f, Yaw, 0.f).Quaternion();
	SetPose(FVector(Location.X, Location.Y, WaterZ) * 0.01, Rot);
	ClearMotion();
	V = Rot.GetForwardVector() * (ForwardSpeed * 0.01);
	CapsizeTimer = 0.f;
	ApplyTransform(0.f);
}

void ASaltBoat::ResetToStart()
{
	SetHold(ESaltBoatHold::Free);
	PlaceAt(StartTransform.GetLocation(), StartTransform.Rotator().Yaw);
	bSailRaised = false;
	Hoist = 0.f;
	Sheet = 0.5f;
	RudderAngle = 0.f;
	BoomAngle = 0.f;
	SetHold(ESaltBoatHold::Moored);
	UE_LOG(LogSaltbound, Display, TEXT("Boat reset to its mooring"));
}

void ASaltBoat::Recover()
{
	FVector Origin = StartTransform.GetLocation();
	float Yaw = StartTransform.Rotator().Yaw;
	if (SafePoses.Num() > 0)
	{
		Origin = SafePoses.Last().Origin;
		Yaw = SafePoses.Last().Yaw;
	}
	SetHold(ESaltBoatHold::Free);
	PlaceAt(Origin, Yaw);
	bSailRaised = false;
	Hoist = 0.f;
	RudderAngle = 0.f;
	UE_LOG(LogSaltbound, Display, TEXT("Boat recovered upright at (%.0f, %.0f)"), Origin.X, Origin.Y);
}

void ASaltBoat::SetSailRaised(bool bRaised)
{
	bSailRaised = bRaised;
}

void ASaltBoat::SetControlOverride(bool bEnable, float Rudder01, float Sheet01)
{
	bOverride = bEnable;
	OverrideRudder = FMath::Clamp(Rudder01, -1.f, 1.f);
	OverrideSheet = FMath::Clamp(Sheet01, 0.f, 1.f);
}

void ASaltBoat::MoorAtStart()
{
	using namespace SaltHull;
	const FVector Bow(HalfLength - 0.3f, 0.f, SheerZ(HalfLength - 0.3f));
	const FVector Stern(-HalfLength + 0.3f, 0.f, SheerZ(HalfLength - 0.3f));
	const FTransform StartM(StartTransform.GetRotation(), StartTransform.GetLocation() * 0.01);
	Lines.Reset();
	Lines.Add({Bow, StartM.TransformPosition(Bow), 0.25f});
	Lines.Add({Stern, StartM.TransformPosition(Stern), 0.25f});
}

void ASaltBoat::SetHold(ESaltBoatHold NewHold)
{
	using namespace SaltHull;
	Hold = NewHold;
	Lines.Reset();
	if (Hold == ESaltBoatHold::Moored)
	{
		MoorAtStart();
	}
	else if (Hold == ESaltBoatHold::Anchored)
	{
		// The anchor drops just ahead of the bow; the rode lets the boat fall back and swing to the wind.
		const FVector Bow(HalfLength - 0.3f, 0.f, SheerZ(HalfLength - 0.3f));
		Lines.Add({Bow, BodyToWorld(Bow) + Q.GetForwardVector() * 2.0, AnchorRode});
	}
}

void ASaltBoat::ToggleHold()
{
	if (Hold != ESaltBoatHold::Free)
	{
		SetHold(ESaltBoatHold::Free);
		return;
	}
	const double Speed = V.Size2D();
	const double ToMooring = FVector::Dist2D(GetActorLocation(), StartTransform.GetLocation()) * 0.01;
	if (Speed < 1.5 && ToMooring < MooringRadius)
	{
		SetHold(ESaltBoatHold::Moored);
	}
	else if (Speed < 1.5)
	{
		SetHold(ESaltBoatHold::Anchored);
	}
}

void ASaltBoat::HandleHelmAction(USaltStationComponent* Station, int32 Action)
{
	if (Action == 0)
	{
		ToggleSail();
	}
	else if (Action == 1)
	{
		ToggleHold();
	}
}

void ASaltBoat::HandleHalyard(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	ToggleSail();
}

// --- Simulation ------------------------------------------------------------------------------------------

void ASaltBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialised || DeltaSeconds <= 0.f)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	Time += Dt;
	if (Time - Dt <= 0.0)
	{
		// Sample this frame's waves (the ocean may register after this actor's BeginPlay).
		if (const USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>())
		{
			if (ASaltOcean* Ocean = Sub->GetOcean())
			{
				AddTickPrerequisiteActor(Ocean);
			}
		}
	}

	FVector2D Wind = FVector2D::ZeroVector;
	if (const USaltWindSubsystem* WindSub = GetWorld()->GetSubsystem<USaltWindSubsystem>())
	{
		Wind = WindSub->GetWindVelocity(FVector2D(X.X * 100.0, X.Y * 100.0)) * 0.01;
	}
	LastTrueWind = Wind;

	UpdateControls(Dt);
	SampleWater(Dt);
	FindContacts();

	const FVector OldX = X;
	const FQuat OldQ = Q;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / MaxSubstep), 1, 16);
	const float H = Dt / Steps;
	for (int32 S = 0; S < Steps; ++S)
	{
		Step(H, Wind);
	}
	if (X.ContainsNaN() || V.ContainsNaN() || W.ContainsNaN() || Q.ContainsNaN())
	{
		UE_LOG(LogSaltbound, Warning, TEXT("Boat simulation became invalid; recovering"));
		X = OldX;
		Q = OldQ;
		Recover();
		return;
	}
	PreventTunnelling(OldX, OldQ);
	ApplyTransform(Dt);
	UpdateVisuals(Dt);
	UpdateHints();
	UpdateRecovery(Dt);
}

void ASaltBoat::SampleWater(float DeltaSeconds)
{
	const USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>();
	for (FColumn& C : Columns)
	{
		const FVector Mid = BodyToWorld(C.Bottom + FVector(0.0, 0.0, C.Height * 0.5));
		const double Z = Sub ? Sub->SampleWater(FVector2D(Mid.X * 100.0, Mid.Y * 100.0)).Location.Z * 0.01 : 0.0;
		// Along the column's path: includes the slope the hull moves over (encounter velocity).
		C.WaterVz = (C.bHasWater && DeltaSeconds > 0.f) ? FMath::Clamp((Z - C.WaterZ) / DeltaSeconds, -5.0, 5.0) : 0.0;
		C.WaterZ = Z;
		C.bHasWater = true;
	}
}

void ASaltBoat::FindContacts()
{
	Contacts.Reset();
	MaxContactDepth = 0.f;
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltBoatContact), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	// Broad phase: one box around the whole boat (with room for a frame's motion). In open water nothing is near.
	const FVector BoxCentre = BodyToWorld(FVector(0.0, 0.0, -0.1)) * 100.0;
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(SaltHull::HalfLength + 0.8, SaltHull::HalfBeam + 0.8, 2.3) * 100.0);
	bNearStatic = World->OverlapAnyTestByObjectType(BoxCentre, Q, Objects, Box, Params);
	if (!bNearStatic)
	{
		return;
	}
	for (const FCapsule& C : Capsules)
	{
		const FVector A = BodyToWorld(C.A);
		const FVector B = BodyToWorld(C.B);
		FVector Centre;
		FQuat Rot;
		FCollisionShape Shape;
		CapsulePose(A, B, C.Radius, ContactMargin, Centre, Rot, Shape);
		TArray<FOverlapResult> Overlaps;
		if (!World->OverlapMultiByObjectType(Overlaps, Centre, Rot, Objects, Shape, Params))
		{
			continue;
		}
		for (const FOverlapResult& O : Overlaps)
		{
			UPrimitiveComponent* Prim = O.GetComponent();
			FMTDResult MTD;
			if (!Prim || !Prim->ComputePenetration(MTD, Shape, Centre, Rot) || MTD.Direction.IsNearlyZero())
			{
				continue;
			}
			const FVector N = MTD.Direction.GetSafeNormal();
			const float Depth = MTD.Distance * 0.01f - ContactMargin;   // < 0: a gap within the margin
			// Deepest point of the real capsule against the plane: the lower end along -N, or the middle when the
			// capsule lies along the surface.
			const double DA = FVector::DotProduct(A, N);
			const double DB = FVector::DotProduct(B, N);
			const FVector Axis = FMath::Abs(DA - DB) < 0.05 ? (A + B) * 0.5 : (DA < DB ? A : B);
			const FVector Point = Axis - N * C.Radius;
			FContact Contact;
			Contact.BodyPoint = CoM + Q.UnrotateVector(Point - X);
			Contact.PlanePoint = Point + N * Depth;
			Contact.Normal = N;
			Contacts.Add(Contact);
			MaxContactDepth = FMath::Max(MaxContactDepth, Depth);
		}
	}
}

void ASaltBoat::Step(float H, const FVector2D& WindWorld)
{
	using namespace SaltHull;
	FVector F(0.0, 0.0, -Mass * Gravity);
	FVector T = FVector::ZeroVector;
	auto AddForceAt = [&](const FVector& Force, const FVector& At)
	{
		F += Force;
		T += FVector::CrossProduct(At - X, Force);
	};
	const FQuat Inv = Q.Inverse();
	const FVector Up = Q.GetUpVector();

	// Buoyancy and heave damping per column.
	double Submerged = 0.0;
	for (const FColumn& C : Columns)
	{
		const FVector Pb = BodyToWorld(C.Bottom);
		const FVector Pt = Pb + Up * C.Height;
		const bool bUpright = Pb.Z <= Pt.Z;
		const FVector Low = bUpright ? Pb : Pt;
		const FVector High = bUpright ? Pt : Pb;
		const double Frac = FMath::Clamp((C.WaterZ - Low.Z) / FMath::Max(High.Z - Low.Z, 1e-3), 0.0, 1.0);
		if (Frac <= 0.0)
		{
			continue;
		}
		const double Vol = C.Area * C.Height * Frac;
		Submerged += Vol;
		const FVector At = Low + (High - Low) * (Frac * 0.5);
		const double VRel = PointVelocity(At).Z - C.WaterVz;
		const double Damp = VerticalDamping * C.Area * FMath::Min(Frac * C.Height / 0.3, 1.0);
		AddForceAt(FVector(0.0, 0.0, RhoWater * Gravity * Vol - Damp * VRel), At);
	}
	Immersion = FMath::Clamp((float)(Submerged / DesignVolume), 0.f, 1.f);
	const double Wet = Immersion;

	// Hull, keel and rudder in body space.
	const FVector Vb = Inv.RotateVector(V);
	const FVector Wb = Inv.RotateVector(W);
	auto BodyVel = [&](const FVector& P) { return Vb + FVector::CrossProduct(Wb, P - CoM); };
	auto AddBodyForceAt = [&](const FVector& Fb, const FVector& P) { AddForceAt(Q.RotateVector(Fb), BodyToWorld(P)); };

	const double U = Vb.X;
	const double Ur = FMath::Abs(U) / FMath::Max(HullSpeed, 0.1f);
	const double Resistance = HullSpeedResistance * (0.35 * Ur * Ur + 0.65 * FMath::Pow(Ur, 6.0));
	AddBodyForceAt(FVector(-FMath::Sign(U) * Resistance * Wet, 0.0, 0.0), CoM);

	// Keel as a low-aspect wing: lateral lift against leeway, capped at the stall angle.
	const FVector Vk = BodyVel(KeelPoint);
	const double Along = FMath::Abs(Vk.X);
	const double MaxLat = FMath::Tan(FMath::DegreesToRadians(KeelStallDeg)) * Along;
	const double Lift = -0.5 * RhoWater * KeelArea * KeelLiftSlope * Along * FMath::Clamp(Vk.Y, -MaxLat, MaxLat);
	AddBodyForceAt(FVector(0.0, Lift * Wet, 0.0), KeelPoint);
	// Crossflow drag along the hull (sideways drift, turning).
	for (const FSegment& S : Segments)
	{
		const double Vy = BodyVel(S.Point).Y;
		AddBodyForceAt(FVector(0.0, -0.5 * RhoWater * CrossflowDrag * S.Area * FMath::Abs(Vy) * Vy * Wet, 0.0), S.Point);
	}
	const FVector DampB(-RollDamping * Wb.X, -PitchDamping * Wb.Y, -(YawDamping + YawDampingPerSpeed * FMath::Abs(U)) * Wb.Z);
	T += Q.RotateVector(DampB * Wet);

	// Rudder: a foil turned against the flow (+ angle = bear away to starboard: the stern is pushed to port).
	const FVector RudderPoint(RudderHeelX - RudderChord * 0.5f, 0.f, (RudderHeelZ + RudderBladeTopZ) * 0.5f);
	const FVector Vr = BodyVel(RudderPoint);
	const double Delta = FMath::DegreesToRadians(-RudderAngle);
	const FVector2D Fr = FoilForce(FVector2D(-Vr.X, -Vr.Y), FVector2D(FMath::Cos(Delta), FMath::Sin(Delta)), RudderArea, RhoWater, RudderCoefficients);
	AddBodyForceAt(FVector(Fr.X, Fr.Y, 0.0) * Wet, RudderPoint);
	// A little steerage at rest (sculling with the rudder), fading out with speed.
	const double RestFactor = 1.0 - FMath::Clamp(FMath::Abs(U) / 1.5, 0.0, 1.0);
	T += Up * (RestTurnTorque * (RudderAngle / FMath::Max(MaxRudderAngle, 1.f)) * RestFactor * Wet);

	// Sail in the apparent wind at its centre of effort (the boom swings to the sheet's limit, UpdateControls).
	const FVector AirWorld(WindWorld.X, WindWorld.Y, 0.0);
	const double Phi = FMath::DegreesToRadians(BoomAngle);
	const FVector BoomDir(-FMath::Cos(Phi), FMath::Sin(Phi), 0.0);
	const FVector Tack(MastX, 0.f, SheerZ(MastX) + BoomHeight);
	const FVector CE = Tack + BoomDir * SailCEAft + FVector(0.0, 0.0, SailCEUp * FMath::Max(Hoist, 0.05f));
	const FVector Apparent = Inv.RotateVector(AirWorld - PointVelocity(BodyToWorld(CE)));
	LastApparentWind = FVector2D(Apparent.X, Apparent.Y);
	if (Hoist > 0.01f)
	{
		float Alpha = 0.f;
		const FVector2D Fs = FoilForce(LastApparentWind, FVector2D(BoomDir.X, BoomDir.Y), SailArea * Hoist, RhoAir, SailCoefficients, &Alpha);
		AddBodyForceAt(FVector(Fs.X, Fs.Y, 0.0), CE);
		SailAlpha = FMath::RadiansToDegrees(Alpha);
	}
	else
	{
		SailAlpha = 0.f;
	}

	// Windage of hull and rig.
	const FVector WindagePoint = BodyToWorld(FVector(0.0, 0.0, 1.5));
	FVector AirRel = AirWorld - PointVelocity(WindagePoint);
	AirRel.Z = 0.0;
	const double AirSpeed = AirRel.Size();
	if (AirSpeed > 0.01)
	{
		const FVector AirB = Inv.RotateVector(AirRel);
		const double Area = WindageArea.X * FMath::Abs(AirB.X) / AirSpeed + WindageArea.Y * FMath::Abs(AirB.Y) / AirSpeed;
		AddForceAt(AirRel * (0.5 * RhoAir * Area * AirSpeed), WindagePoint);
	}

	// Mooring and anchor lines: horizontal springs with slack.
	for (const FLine& L : Lines)
	{
		const FVector P = BodyToWorld(L.BodyPoint);
		FVector D = P - L.Fixed;
		D.Z = 0.0;
		const double Dist = D.Size();
		if (Dist <= L.Slack || Dist < 1e-4)
		{
			continue;
		}
		const FVector Dir = D / Dist;
		const double Rate = FVector::DotProduct(PointVelocity(P), Dir);
		const double Tension = FMath::Clamp(LineStiffness * (Dist - L.Slack) + LineDamping * Rate, 0.0, MaxLineTension);
		AddForceAt(-Dir * Tension, P);
	}

	// Contacts with static geometry: stiff damped springs with Coulomb friction.
	for (const FContact& C : Contacts)
	{
		const FVector P = BodyToWorld(C.BodyPoint);
		const double Depth = FVector::DotProduct(C.PlanePoint - P, C.Normal);
		if (Depth <= 0.0)
		{
			continue;
		}
		const FVector Vel = PointVelocity(P);
		const double Vn = FVector::DotProduct(Vel, C.Normal);
		const double Fn = FMath::Max(0.0, ContactStiffness * Depth - ContactDamping * Vn);
		const FVector Vt = Vel - C.Normal * Vn;
		const double VtLen = Vt.Size();
		const FVector Friction = VtLen > 1e-4 ? -Vt / VtLen * FMath::Min(ContactFriction * Fn, ContactDamping * 0.5 * VtLen) : FVector::ZeroVector;
		AddForceAt(C.Normal * Fn + Friction, P);
	}

	// Integrate: semi-implicit Euler; added mass in heave and added inertia in roll and pitch.
	FVector A = F / Mass;
	A.Z = F.Z / (Mass * (1.0 + AddedMass.X * Wet));
	V += A * H;
	X += V * H;
	const FVector Inertia(Mass * FMath::Square(Gyration.X) * (1.0 + AddedMass.Y * Wet),
		Mass * FMath::Square(Gyration.Y) * (1.0 + AddedMass.Z * Wet), Mass * FMath::Square(Gyration.Z));
	FVector WbNew = Wb + (Inv.RotateVector(T) - FVector::CrossProduct(Wb, Wb * Inertia)) / Inertia * H;
	W = Q.RotateVector(WbNew);
	const double Rate = W.Size();
	if (Rate * H > 1e-9)
	{
		Q = (FQuat(W / Rate, Rate * H) * Q).GetNormalized();
	}
}

void ASaltBoat::PreventTunnelling(const FVector& OldX, const FQuat& OldQ)
{
	// Contact springs act within the margin; a fast boat could still pass a thin post between two frames.
	const FVector Move = X - OldX;
	if (!bNearStatic || Move.SizeSquared() < FMath::Square(0.01))
	{
		return;
	}
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltBoatSweep), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	double MinTime = 1.0;
	FVector HitNormal = FVector::ZeroVector;
	for (const FCapsule& C : Capsules)
	{
		FVector From, To;
		FQuat Rot;
		FCollisionShape Shape;
		CapsulePose(OldX + OldQ.RotateVector(C.A - CoM), OldX + OldQ.RotateVector(C.B - CoM), C.Radius, 0.f, From, Rot, Shape);
		CapsulePose(BodyToWorld(C.A), BodyToWorld(C.B), C.Radius, 0.f, To, Rot, Shape);
		FHitResult Hit;
		if (World->SweepSingleByObjectType(Hit, From, To, Rot, Objects, Shape, Params) && !Hit.bStartPenetrating && Hit.Time < MinTime)
		{
			MinTime = Hit.Time;
			HitNormal = Hit.Normal;
		}
	}
	if (MinTime < 1.0)
	{
		X = OldX + Move * MinTime;
		const double Vn = FVector::DotProduct(V, HitNormal);
		if (Vn < 0.0)
		{
			V -= HitNormal * (Vn * 1.2);
		}
	}
}

void ASaltBoat::ApplyTransform(float DeltaSeconds)
{
	const FVector Origin = X - Q.RotateVector(CoM);
	SetActorLocationAndRotation(Origin * 100.0, Q, false, nullptr, ETeleportType::None);
	AngVel = W;
	// CharacterMovement reads the base's linear velocity (jumping off the deck keeps it).
	const FVector OriginVelocity = PointVelocity(Origin) * 100.0;
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* P : Prims)
	{
		P->ComponentVelocity = OriginVelocity;
	}
}

// --- Controls, visuals, hints -------------------------------------------------------------------------------

void ASaltBoat::UpdateControls(float DeltaSeconds)
{
	const FVector2D In = Helm ? Helm->GetControlInput() : FVector2D::ZeroVector;
	float RudderTarget = In.X * MaxRudderAngle;
	if (bOverride)
	{
		if (bAutopilot)
		{
			const float Error = FRotator::NormalizeAxis(AutopilotYaw - GetYawDeg());
			OverrideRudder = FMath::Clamp(Error / 20.f - GetYawRateDeg() / 8.f, -1.f, 1.f);
		}
		RudderTarget = OverrideRudder * MaxRudderAngle;
		Sheet = OverrideSheet;
	}
	else
	{
		// W hauls the sheet in, S eases it out.
		Sheet = FMath::Clamp(Sheet - In.Y * SheetRate * DeltaSeconds, 0.f, 1.f);
	}
	RudderAngle = FMath::FInterpConstantTo(RudderAngle, RudderTarget, DeltaSeconds,
		FMath::IsNearlyZero(RudderTarget) ? RudderReturnRate : RudderRate);
	Hoist = FMath::FInterpConstantTo(Hoist, bSailRaised ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(HoistTime, 0.1f));

	// The boom swings free with the apparent wind up to the sheet's limit. Running dead downwind it stays on its
	// side until the wind is 15 degrees on the other side (sailing by the lee), then gybes across.
	const float Limit = FMath::Lerp(SheetMinAngle, SheetMaxAngle, Sheet);
	float Target = FMath::Clamp(BoomAngle, -Limit, Limit);
	if (Hoist > 0.01f && LastApparentWind.Size() > 0.3)
	{
		const float Free = FMath::RadiansToDegrees(FMath::Atan2(LastApparentWind.Y, -LastApparentWind.X));
		const bool bByTheLee = FMath::Abs(Free) > 165.f && FMath::Sign(Free) != FMath::Sign(BoomAngle) && FMath::Abs(BoomAngle) > 1.f;
		Target = bByTheLee ? FMath::Sign(BoomAngle) * Limit : FMath::Clamp(Free, -Limit, Limit);
	}
	BoomAngle = FMath::FInterpConstantTo(BoomAngle, Target, DeltaSeconds, BoomSwingRate);
}

void ASaltBoat::UpdateVisuals(float DeltaSeconds)
{
	using namespace SaltHull;
	BoomPivot->SetRelativeRotation(FRotator(0.f, -BoomAngle, 0.f));
	// The sail bellies out to leeward: the side the apparent wind pushes it to (in the boom's frame).
	const double Phi = FMath::DegreesToRadians(BoomAngle);
	const double Push = FVector2D::DotProduct(LastApparentWind, FVector2D(FMath::Sin(Phi), FMath::Cos(Phi)));
	if (FMath::Abs(Push) > 0.2)
	{
		SailSide = Push > 0.0 ? 1.f : -1.f;
	}
	SailSideVisual = FMath::FInterpConstantTo(SailSideVisual, SailSide, DeltaSeconds, 3.f);
	// Luffing (little angle of attack): the sail flattens and shivers.
	const float Drive = FMath::SmoothStep(2.f, 9.f, SailAlpha);
	const float Shiver = (1.f - Drive) * 0.3f * FMath::Sin((float)Time * 17.f) * (Hoist > 0.5f ? 1.f : 0.f);
	float Camber = SailSideVisual * (0.25f + 0.75f * Drive) + Shiver;
	if (FMath::Abs(Camber) < 0.03f)
	{
		Camber = Camber < 0.f ? -0.03f : 0.03f;
	}
	const float S = FMath::Max(Hoist, 0.04f);
	Sail->SetRelativeScale3D(FVector(1.f, Camber, S));
	// The gaff follows the head of the sail down onto the boom.
	const float GaffRad = FMath::DegreesToRadians(GaffAngleDeg);
	GaffPivot->SetRelativeLocation(FVector(0.f, 0.f, (ThroatHeight - BoomHeight) * S * 100.f + 6.f * (1.f - S)));
	GaffPivot->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(FMath::Atan2(S * FMath::Sin(GaffRad), FMath::Cos(GaffRad))), 0.f, 0.f));
	Rudder->SetRelativeRotation(FRotator(0.f, -RudderAngle, 0.f));
	Wheel->SetRelativeRotation(FRotator(0.f, 0.f, RudderAngle * WheelRatio));
}

void ASaltBoat::UpdateHints()
{
	const double Speed = V.Size2D();
	const double ToMooring = FVector::Dist2D(GetActorLocation(), StartTransform.GetLocation()) * 0.01;
	FString HoldAction;
	switch (Hold)
	{
	case ESaltBoatHold::Moored: HoldAction = TEXT("C  cast off"); break;
	case ESaltBoatHold::Anchored: HoldAction = TEXT("C  weigh anchor"); break;
	default: HoldAction = Speed >= 1.5 ? FString() : (ToMooring < MooringRadius ? TEXT("C  moor") : TEXT("C  drop anchor")); break;
	}
	// True wind relative to the bow (+ = from starboard).
	FString WindText = TEXT("no wind");
	const double TrueSpeed = LastTrueWind.Size();
	if (TrueSpeed > 0.2)
	{
		const float FromYaw = FMath::RadiansToDegrees(FMath::Atan2(-LastTrueWind.Y, -LastTrueWind.X));
		const float Rel = FRotator::NormalizeAxis(FromYaw - (float)Q.Rotator().Yaw);
		const float A = FMath::Abs(Rel);
		const TCHAR* Side = Rel > 0.f ? TEXT("starboard") : TEXT("port");
		const FString Where = A < 25.f ? FString(TEXT("dead ahead")) : A > 160.f ? FString(TEXT("astern"))
			: FString::Printf(TEXT("on the %s %s"), Side, A < 70.f ? TEXT("bow") : (A < 115.f ? TEXT("beam") : TEXT("quarter")));
		WindText = FString::Printf(TEXT("wind %.0f kn %s"), TrueSpeed * MsToKnots, *Where);
	}
	const FString Line1 = FString::Printf(TEXT("A / D  steer      W / S  sheet in / out      Space  %s      %s"),
		bSailRaised ? TEXT("lower sail") : TEXT("raise sail"), *HoldAction);
	const FString Line2 = FString::Printf(TEXT("%.1f kn      sheet %.0f %% out      %s%s"), Speed * MsToKnots, Sheet * 100.f, *WindText,
		Hold == ESaltBoatHold::Moored ? TEXT("      moored") : (Hold == ESaltBoatHold::Anchored ? TEXT("      at anchor") : TEXT("")));
	Helm->ControlsHintText = FText::FromString(Line1 + TEXT("\n") + Line2);
	Halyard->PromptText = bSailRaised ? NSLOCTEXT("Saltbound", "BoatLowerSail", "Lower the sail") : NSLOCTEXT("Saltbound", "BoatRaiseSail", "Raise the sail");
}

void ASaltBoat::UpdateRecovery(float DeltaSeconds)
{
	const FVector Up = Q.GetUpVector();
	CapsizeTimer = Up.Z < 0.34 ? CapsizeTimer + DeltaSeconds : 0.f;
	if (CapsizeTimer > 3.f)
	{
		UE_LOG(LogSaltbound, Display, TEXT("Boat capsized; recovering"));
		Recover();
		return;
	}
	SafePoseTimer += DeltaSeconds;
	if (SafePoseTimer > 2.f && Contacts.Num() == 0 && Up.Z > 0.9 && Immersion > 0.5f)
	{
		SafePoseTimer = 0.f;
		SafePoses.Add({GetActorLocation(), (float)Q.Rotator().Yaw});
		if (SafePoses.Num() > 8)
		{
			SafePoses.RemoveAt(0);
		}
	}
}

// --- State -------------------------------------------------------------------------------------------------

FVector ASaltBoat::GetVelocity() const
{
	return PointVelocity(X - Q.RotateVector(CoM)) * 100.0;
}

float ASaltBoat::GetSpeedKnots() const
{
	return (float)(V.Size2D() * MsToKnots);
}

float ASaltBoat::GetHeelDeg() const
{
	return (float)Q.Rotator().Roll;
}

float ASaltBoat::GetPitchDeg() const
{
	return (float)Q.Rotator().Pitch;
}

float ASaltBoat::GetWaterlineOffset() const
{
	const USaltOceanSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<USaltOceanSubsystem>() : nullptr;
	const FVector L = GetActorLocation();
	return (float)(L.Z - (Sub ? Sub->SampleWater(FVector2D(L.X, L.Y)).Location.Z : 0.0));
}

FString ASaltBoat::Describe() const
{
	const FVector L = GetActorLocation();
	const FVector Vb = Q.UnrotateVector(V);
	const float Yaw = (float)Q.Rotator().Yaw;
	const float CourseYaw = V.Size2D() > 0.05 ? FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) : Yaw;
	const float FromYaw = FMath::RadiansToDegrees(FMath::Atan2(-LastTrueWind.Y, -LastTrueWind.X));
	const TCHAR* HoldName = Hold == ESaltBoatHold::Moored ? TEXT("Moored") : (Hold == ESaltBoatHold::Anchored ? TEXT("Anchored") : TEXT("Free"));
	return FString::Printf(TEXT("SB_BOAT pos=(%.0f,%.0f,%.1f) yaw=%.1f course=%.1f speed=%.2fkn surge=%.2f sway=%.3f yawRate=%.2f heel=%.2f pitch=%.2f ")
		TEXT("rudder=%.1f sheet=%.2f boom=%.1f alpha=%.1f hoist=%.2f hold=%s immersion=%.2f waterline=%.1fcm contacts=%d depth=%.1fcm ")
		TEXT("twa=%.1f tws=%.2f aws=%.2f mass=%.0f"),
		L.X, L.Y, L.Z, Yaw, CourseYaw, GetSpeedKnots(), Vb.X, Vb.Y, FMath::RadiansToDegrees(W.Z), GetHeelDeg(), GetPitchDeg(),
		RudderAngle, Sheet, BoomAngle, SailAlpha, Hoist, HoldName, Immersion, GetWaterlineOffset(), Contacts.Num(), GetContactDepth(),
		FRotator::NormalizeAxis(FromYaw - Yaw), LastTrueWind.Size(), LastApparentWind.Size(), Mass);
}

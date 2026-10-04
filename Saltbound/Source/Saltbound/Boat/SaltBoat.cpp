// Saltbound - the sailing boat: a kinematic rigid body moved by our own integrator (D7, D36).

#include "Boat/SaltBoat.h"

#include "Boat/SaltHullShape.h"
#include "CableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SaltInteractableComponent.h"
#include "Interaction/SaltStationComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Ocean/SaltOcean.h"
#include "Ocean/SaltOceanSubsystem.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Props/SaltSwingComponent.h"
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
		TEXT("Fixed controls for tests: sb.Boat.Control <Rudder -1..1> [Sail 0|1]. sb.Boat.Control off returns to the helm."),
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
			Boat->SetControlOverride(true, FCString::Atof(*Args[0]));
			if (Args.Num() > 1)
			{
				Boat->SetSailSet(FCString::Atoi(*Args[1]) != 0);
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
	GaffPivot->SetRelativeRotation(FRotator(-GaffAngleDeg, 0.f, 0.f));
	Gaff = Part(TEXT("Gaff"), GaffPivot);
	// The sail's own vertex animation (furl, fill, flutter) moves it beyond the mesh bounds.
	Sail->SetBoundsScale(2.f);
	// The rudder turns about its raked stock: the pivot's +Z runs along the stock.
	RudderPivot = Pivot(TEXT("RudderPivot"), Hull, FVector(RudderHeelX, 0.f, RudderHeelZ));
	RudderPivot->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Atan2(RudderHeelX - RudderHeadX, RudderHeadZ - RudderHeelZ)), 0.f, 0.f));
	Rudder = Part(TEXT("Rudder"), RudderPivot);
	const FVector Hub(WheelX, 0.f, SheerZ(WheelX) + WheelHeight);
	Wheel = Part(TEXT("Wheel"), Hull);
	Wheel->SetRelativeLocation(Hub * 100.0);
	OarPivot = Pivot(TEXT("OarPivot"), Hull, OarBoss());
	OarPivot->SetRelativeRotation(FRotator(-OarRakeDeg, 0.f, 0.f));
	SteeringOar = Part(TEXT("SteeringOar"), OarPivot);
	PennantPivot = Pivot(TEXT("PennantPivot"), Hull, FVector(MastX - MastHeadAft, -MastHeadPort, SheerZ(MastX) + MastHeight + PennantHeight));
	Pennant = Part(TEXT("Pennant"), PennantPivot);
	Pennant->SetBoundsScale(3.f);   // its vertex animation moves it beyond the mesh bounds
	EnsignPivot = Pivot(TEXT("EnsignPivot"), Hull, FVector(EnsignX, 0.f, SheerZ(EnsignX) + DeckCamber + EnsignHeight - 0.02f));
	Ensign = Part(TEXT("Ensign"), EnsignPivot);
	Ensign->SetBoundsScale(3.f);

	Helm = CreateDefaultSubobject<USaltStationComponent>(TEXT("Helm"));
	Helm->SetupAttachment(Hull);
	Helm->YawLimit = 160.f;
	Helm->PitchMin = -50.f;
	Helm->PitchMax = 75.f;
	Helm->PromptText = NSLOCTEXT("Saltbound", "BoatHelmUse", "Take the helm");
	Helm->LeavePromptText = NSLOCTEXT("Saltbound", "BoatHelmLeave", "Leave the helm");

	// The sail is set and let go at the halyards' pin rails inside the bulwark by the shrouds, on either side (D53).
	auto Interaction = [this](FName Name, const FVector& LocationM, const FVector& ExtentCm)
	{
		USaltInteractableComponent* C = CreateDefaultSubobject<USaltInteractableComponent>(Name);
		C->SetupAttachment(Hull);
		C->SetRelativeLocation(LocationM * 100.0);
		C->SetBoxExtent(ExtentCm);
		return C;
	};
	SailRopePort = Interaction(TEXT("SailRopePort"), FVector(PinRailX, -PinRailY(), SheerZ(PinRailX) + 0.48f), FVector(38.f, 18.f, 30.f));
	SailRopeStarboard = Interaction(TEXT("SailRopeStarboard"), FVector(PinRailX, PinRailY(), SheerZ(PinRailX) + 0.48f), FVector(38.f, 18.f, 30.f));
	SailRopePort->PromptText = SailRopeStarboard->PromptText = NSLOCTEXT("Saltbound", "BoatLowerSail", "Lower the sail");
	// Windlass on the foredeck: cast off, moor, drop or raise the anchor.
	Windlass = Interaction(TEXT("Windlass"), FVector(3.55f, 0.f, SheerZ(3.55f) + 0.45f), FVector(30.f, 55.f, 35.f));
	Windlass->PromptText = NSLOCTEXT("Saltbound", "BoatCastOff", "Cast off");

	// Windlass drum and rope (they turn while the anchor runs out or comes up).
	WindlassDrum = Part(TEXT("WindlassDrum"), Hull);
	WindlassDrum->SetRelativeLocation(FVector(WindlassX, 0.f, SheerZ(WindlassX) + DeckCamber + WindlassDrumHeight) * 100.0);
	WindlassRope = Part(TEXT("WindlassRope"), WindlassDrum);
	// Boarding ladders: climb up from the water, or down from the deck.
	const float LadderY = HalfBeamAtSheer(LadderX) + LadderOut + 0.12f;
	LadderPort = Interaction(TEXT("LadderPort"), FVector(LadderX, -LadderY, 0.4f), FVector(32.f, 28.f, 70.f));
	LadderStarboard = Interaction(TEXT("LadderStarboard"), FVector(LadderX, LadderY, 0.4f), FVector(32.f, 28.f, 70.f));
	LadderPort->PromptText = LadderStarboard->PromptText = NSLOCTEXT("Saltbound", "BoatClimbLadder", "Climb the ladder");
	ApplySteeringLayout();
	// Stores (D52): the lashed water barrel and crate on the port side aft (build_boat.py deck_gear).
	BarrelLid = Interaction(TEXT("BarrelLid"), FVector(-1.65f, -1.0f, SheerZ(-1.65f) + 0.38f), FVector(30.f, 30.f, 38.f));
	CrateLid = Interaction(TEXT("CrateLid"), FVector(-2.25f, -0.95f, SheerZ(-2.25f) + 0.24f), FVector(32.f, 26.f, 26.f));
	BarrelLid->PromptText = NSLOCTEXT("Saltbound", "OpenBarrel", "Open the barrel");
	CrateLid->PromptText = NSLOCTEXT("Saltbound", "OpenCrate", "Open the crate");
	BarrelStore = CreateDefaultSubobject<USaltInventoryComponent>(TEXT("BarrelStore"));
	CrateStore = CreateDefaultSubobject<USaltInventoryComponent>(TEXT("CrateStore"));
	BarrelStore->InitStore(TEXT("Boat_Barrel"), NSLOCTEXT("Saltbound", "Barrel", "Barrel"), 10);
	CrateStore->InitStore(TEXT("Boat_Crate"), NSLOCTEXT("Saltbound", "Crate", "Crate"), 10);
	CrateStore->SetSlots({ { TEXT("Driftwood"), 4 }, { TEXT("RopeCoil"), 1 } });
	// Gangway to the pier while moored (placed every frame; the player walks on it).
	Gangway = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gangway"));
	Gangway->SetupAttachment(Hull);
	Gangway->SetMobility(EComponentMobility::Movable);
	Gangway->SetUsingAbsoluteLocation(true);
	Gangway->SetUsingAbsoluteRotation(true);
	Gangway->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Gangway->SetCanEverAffectNavigation(false);

	// Anchor at the cathead (starboard bow), its rode, the mainsheet and the mooring lines (physics ropes).
	// The anchor rope leaves through a hawse hole in the starboard bow (D52); the anchor itself is never seen (it still
	// goes down and holds the boat).
	Hawse = Pivot(TEXT("Hawse"), Hull, FVector(HawseX, HalfBreadthAt(HawseX, HawseZ) + 0.06f, HawseZ));
	Anchor = Part(TEXT("Anchor"), Hawse);
	Anchor->SetRelativeLocation(FVector(0.f, 0.f, -4.f));
	Anchor->SetVisibility(false);
	Anchor->SetHiddenInGame(true);
	auto Rope = [this](FName Name, USceneComponent* Start)
	{
		UCableComponent* C = CreateDefaultSubobject<UCableComponent>(Name);
		C->SetupAttachment(Start);
		C->bAttachStart = true;
		C->bAttachEnd = true;
		C->CableWidth = 2.6f;
		C->NumSegments = 14;
		C->NumSides = 6;
		C->SolverIterations = 6;
		C->CableGravityScale = 1.f;
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		return C;
	};
	AnchorLine = Rope(TEXT("AnchorLine"), Hawse);
	MooringPierFore = Pivot(TEXT("MooringPierFore"), Hull, FVector::ZeroVector);
	MooringPierAft = Pivot(TEXT("MooringPierAft"), Hull, FVector::ZeroVector);
	MooringPierFore->SetUsingAbsoluteLocation(true);
	MooringPierAft->SetUsingAbsoluteLocation(true);
	MooringLineFore = Rope(TEXT("MooringLineFore"), Pivot(TEXT("BittFore"), Hull, FVector(4.0f, 0.22f, SheerZ(4.0f) + 0.38f)));
	MooringLineAft = Rope(TEXT("MooringLineAft"), Pivot(TEXT("BittAft"), Hull, FVector(-4.15f, 0.22f, SheerZ(4.15f) + 0.38f)));

	// The halyards (D53): the gaff hangs from them, from the blocks on the mast to its throat eye and its span block.
	// Taut lines: almost no slack or gravity, many solver passes.
	ThroatBlockPoint = Pivot(TEXT("ThroatBlockPoint"), Hull, ThroatBlock());
	PeakBlockPoint = Pivot(TEXT("PeakBlockPoint"), Hull, PeakBlock());
	GaffThroatEye = Pivot(TEXT("GaffThroatEye"), Gaff, SaltHull::GaffThroatEye());
	GaffSpanBlock = Pivot(TEXT("GaffSpanBlock"), Gaff, SaltHull::GaffSpanBlock());
	ThroatHalyard = Rope(TEXT("ThroatHalyard"), ThroatBlockPoint);
	PeakHalyard = Rope(TEXT("PeakHalyard"), PeakBlockPoint);
	for (UCableComponent* C : { ThroatHalyard.Get(), PeakHalyard.Get() })
	{
		C->CableWidth = 2.4f;
		C->NumSegments = 8;
		C->SolverIterations = 16;
		C->CableGravityScale = 0.04f;   // the gaff's weight hangs on them: no visible sag
	}
	ThroatHalyard->NumSegments = 3;

	// The bell (D53) hangs in the ring under its arm forward of the mast and swings with the boat; its clapper swings
	// inside it on the staple and strikes the bell. Pull the bell rope to ring it.
	BellSwing = CreateDefaultSubobject<USaltSwingComponent>(TEXT("BellSwing"));
	BellSwing->SetupAttachment(Hull);
	BellSwing->SetRelativeLocation(BellPivot() * 100.0);
	BellSwing->Length = 12.f;
	BellSwing->MaxAngle = 30.f;
	BellSwing->Damping = 0.06f;
	BellSwing->PushRadius = 12.f;
	Bell = Part(TEXT("Bell"), BellSwing);
	Bell->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	ClapperSwing = CreateDefaultSubobject<USaltSwingComponent>(TEXT("ClapperSwing"));
	ClapperSwing->SetupAttachment(Bell);
	ClapperSwing->SetRelativeLocation(FVector(0.f, 0.f, -5.6f));     // the staple under the crown
	ClapperSwing->Length = 12.f;
	ClapperSwing->MaxAngle = 22.f;                                     // the ball meets the bell's sound bow
	ClapperSwing->Damping = 0.08f;
	ClapperSwing->Restitution = 0.5f;
	BellClapper = Part(TEXT("BellClapper"), ClapperSwing);
	BellRope = Interaction(TEXT("BellRope"), BellPivot() + FVector(0.f, 0.f, -0.36f), FVector(16.f, 16.f, 22.f));
	BellRope->PromptText = NSLOCTEXT("Saltbound", "RingBell", "Ring the bell");

	// The lantern (D53) hangs on the hook aft of the mast and swings; its light burns inside it (the lighting rig lights
	// it at dusk, or the player does).
	LanternSwing = CreateDefaultSubobject<USaltSwingComponent>(TEXT("LanternSwing"));
	LanternSwing->SetupAttachment(Hull);
	LanternSwing->SetRelativeLocation(LanternPivot() * 100.0);
	LanternSwing->Length = 20.f;
	LanternSwing->MaxAngle = 35.f;
	LanternSwing->Damping = 0.05f;
	LanternSwing->PushRadius = 14.f;
	Lantern = Part(TEXT("Lantern"), LanternSwing);
	Lantern->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Lantern->SetCastShadow(false);
	LanternLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LanternLight"));
	LanternLight->SetupAttachment(Lantern);
	LanternLight->SetRelativeLocation(FVector(0.f, 0.f, -21.5f));
	LanternLight->SetIntensityUnits(ELightUnits::Candelas);
	LanternLight->SetIntensity(28.f);
	LanternLight->SetUseTemperature(true);
	LanternLight->SetTemperature(2100.f);
	LanternLight->SetAttenuationRadius(900.f);
	LanternLight->SetSourceRadius(3.f);
	LanternLight->SetSoftSourceRadius(4.f);
	LanternLight->SetCastShadows(true);
	LanternLight->bUseInverseSquaredFalloff = true;
	LanternLight->SetVisibility(false);
	LanternUse = Interaction(TEXT("LanternUse"), LanternPivot() + FVector(0.f, 0.f, -0.2f), FVector(14.f, 14.f, 20.f));
	LanternUse->PromptText = NSLOCTEXT("Saltbound", "BoatLanternLight", "Light the lantern");
}

void ASaltBoat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveContent();
	ApplySteeringLayout();
	StartTransform = Transform;
	// The editor shows the boat as it starts: moored, the sail taken in (the material's default).
	UpdateVisuals(0.f);
	UpdateRopes(0.f);
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
	Resolve(Anchor, TEXT("Anchor"));
	Resolve(WindlassDrum, TEXT("WindlassDrum"));
	Resolve(WindlassRope, TEXT("WindlassRope"));
	Resolve(Gangway, TEXT("Gangway"));
	Resolve(Pennant, TEXT("Pennant"));
	Resolve(Ensign, TEXT("Ensign"));
	Resolve(SteeringOar, TEXT("SteeringOar"));
	Resolve(Bell, TEXT("Bell"));
	Resolve(BellClapper, TEXT("BellClapper"));
	Resolve(Lantern, TEXT("Lantern"));
	UMaterialInterface* RopeMaterial = LoadObject<UMaterialInterface>(nullptr, SaltContent::RopeMaterial);
	for (UCableComponent* C : { AnchorLine.Get(), MooringLineFore.Get(), MooringLineAft.Get(), ThroatHalyard.Get(), PeakHalyard.Get() })
	{
		if (C && RopeMaterial && C->GetMaterial(0) != RopeMaterial)
		{
			C->SetMaterial(0, RopeMaterial);
		}
	}
}

void ASaltBoat::SetupSailMaterials()
{
	SailMaterials.Reset();
	for (int32 I = 0; I < Sail->GetNumMaterials(); ++I)
	{
		UMaterialInterface* Base = Sail->GetMaterial(I);
		if (!Base)
		{
			continue;
		}
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Base);
		if (!Mid)
		{
			Mid = UMaterialInstanceDynamic::Create(Base, this);
			Sail->SetMaterial(I, Mid);
		}
		SailMaterials.Add(Mid);
	}
	PennantMaterial = Pennant->GetMaterial(0) ? Pennant->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	EnsignMaterials.Reset();
	for (int32 I = 0; I < Ensign->GetNumMaterials(); ++I)
	{
		if (Ensign->GetMaterial(I))
		{
			EnsignMaterials.Add(Ensign->CreateAndSetMaterialInstanceDynamic(I));
		}
	}
}

void ASaltBoat::BeginPlay()
{
	Super::BeginPlay();
	ResolveContent();
	ApplySteeringLayout();
	BuildHullModel();
	StartTransform = GetActorTransform();
	SetPose(GetActorLocation() * 0.01, GetActorQuat());
	SailRopePort->OnInteract.AddDynamic(this, &ASaltBoat::HandleSailRope);
	SailRopeStarboard->OnInteract.AddDynamic(this, &ASaltBoat::HandleSailRope);
	Windlass->OnInteract.AddDynamic(this, &ASaltBoat::HandleWindlass);
	LadderPort->OnInteract.AddDynamic(this, &ASaltBoat::HandleLadder);
	LadderStarboard->OnInteract.AddDynamic(this, &ASaltBoat::HandleLadder);
	BarrelLid->OnInteract.AddDynamic(this, &ASaltBoat::HandleStore);
	CrateLid->OnInteract.AddDynamic(this, &ASaltBoat::HandleStore);
	BellRope->OnInteract.AddDynamic(this, &ASaltBoat::HandleBell);
	LanternUse->OnInteract.AddDynamic(this, &ASaltBoat::HandleLantern);
	// The clapper swings after the bell has moved this frame.
	ClapperSwing->AddTickPrerequisiteComponent(BellSwing);
	SetupSailMaterials();
	const int32 GlassSlot = Lantern->GetMaterialIndex(TEXT("MI_LanternGlass"));
	LanternGlassMaterial = GlassSlot != INDEX_NONE ? Lantern->CreateAndSetMaterialInstanceDynamic(GlassSlot) : nullptr;
	SetLanternLit(bLanternLit);
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
		UE_LOG(LogSaltbound, Log, TEXT("Boat sail: area %.1f m2, centre of effort %.2f m aft, %.2f m up"),
			FMath::Abs(Area2) * 0.5, SailCEAft, SailCEUp);
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
	// Everything that stands out collides too (D43: "the boat is one thing"): stem head, cathead with the stowed
	// anchor, rudder blade and head, chain wales and deadeyes, fenders, ladders, and the boom wherever it swings.
	Capsules.Add({FVector(4.92f, 0.f, 1.0f), FVector(5.0f, 0.f, 1.62f), 0.1f});
	if (Steering == ESaltSteering::Wheel)
	{
		Capsules.Add({FVector(RudderHeelX - 0.3f, 0.f, RudderHeelZ + 0.1f), FVector(RudderHeadX - 0.25f, 0.f, RudderBladeTopZ - 0.05f), 0.28f});
		Capsules.Add({FVector(RudderHeadX + 0.12f, 0.f, 0.f), FVector(RudderHeadX, 0.f, RudderHeadZ), 0.1f});
	}
	else
	{
		// The steering oar: its shaft from the head down to the blade, and the blade aft of it.
		Capsules.Add({OarAlong(OarHead), OarAlong(OarBladeTop), 0.08f});
		const FVector BladeMid = OarAft() * (OarBladeChord * 0.5f);
		Capsules.Add({OarAlong(OarBladeTop - 0.2f) + BladeMid, OarAlong(OarTip + 0.2f) + BladeMid, 0.25f});
	}
	for (float Side : { -1.f, 1.f })
	{
		const float Cx0 = MastX - 0.05f, Cx1 = MastX + 0.85f;
		const float Cy = Side * (HalfBeamAtSheer(MastX + 0.35f) + 0.16f);
		Capsules.Add({FVector(Cx0, Cy, SheerZ(MastX)), FVector(Cx1, Cy, SheerZ(MastX)), 0.1f});
		Capsules.Add({FVector(Cx0, Cy, SheerZ(MastX) + 0.8f), FVector(Cx1, Cy, SheerZ(MastX) + 0.8f), 0.12f});
		const float Ly = Side * (HalfBeamAtSheer(LadderX) + LadderOut);
		Capsules.Add({FVector(LadderX, Ly, LadderBottomZ + 0.25f), FVector(LadderX, Ly, SheerZ(LadderX)), 0.25f});
	}
	for (float Fx : { -1.8f, 1.8f })
	{
		const float Fy = HalfBeamAtSheer(Fx) + 0.14f;
		Capsules.Add({FVector(Fx, Fy, SheerZ(Fx) - 0.45f), FVector(Fx, Fy, SheerZ(Fx) - 0.15f), 0.12f});
	}
	BoomCapsule = Capsules.Num();
	Capsules.Add({FVector::ZeroVector, FVector::ZeroVector, 0.12f});
	UpdateBoomCapsule();
}

void ASaltBoat::UpdateBoomCapsule()
{
	using namespace SaltHull;
	if (!Capsules.IsValidIndex(BoomCapsule))
	{
		return;
	}
	const double Phi = FMath::DegreesToRadians(BoomAngle);
	const FVector Goose(MastX, 0.f, SheerZ(MastX) + BoomHeight);
	Capsules[BoomCapsule].A = Goose + FVector(-FMath::Cos(Phi), FMath::Sin(Phi), 0.0) * 0.4;
	Capsules[BoomCapsule].B = Goose + FVector(-FMath::Cos(Phi), FMath::Sin(Phi), 0.0) * (BoomLength - 0.1);
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
	ClearTrail();
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

void ASaltBoat::RestoreState(const FVector& Location, float Yaw, ESaltBoatHold InHold, bool bSail)
{
	if (InHold == ESaltBoatHold::Moored)
	{
		ResetToStart();
	}
	else
	{
		SetHold(ESaltBoatHold::Free);
		PlaceAt(Location, Yaw);
		RudderAngle = 0.f;
		SetHold(InHold);
		if (InHold == ESaltBoatHold::Anchored)
		{
			AnchorAlpha = AnchorHold = 1.f;
			bHoldSet = true;
		}
	}
	bSailSet = bSail;
	Hoist = bSail ? 1.f : 0.f;
	Fill = FillVel = 0.f;
}

void ASaltBoat::ResetToStart()
{
	SetHold(ESaltBoatHold::Free);
	PlaceAt(StartTransform.GetLocation(), StartTransform.Rotator().Yaw);
	bSailSet = false;
	Hoist = 0.f;
	Fill = FillVel = 0.f;
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
	bSailSet = false;
	Hoist = 0.f;
	Fill = FillVel = 0.f;
	RudderAngle = 0.f;
	UE_LOG(LogSaltbound, Display, TEXT("Boat recovered upright at (%.0f, %.0f)"), Origin.X, Origin.Y);
}

void ASaltBoat::SetSailSet(bool bSet)
{
	bSailSet = bSet;
}

void ASaltBoat::SetControlOverride(bool bEnable, float Rudder01)
{
	bOverride = bEnable;
	OverrideRudder = FMath::Clamp(Rudder01, -1.f, 1.f);
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
		// The anchor goes down from the cathead to the seabed below; the rode lets the boat fall back and swing
		// to the wind.
		const FVector Fair = Hawse->GetComponentLocation();
		AnchorGround = Fair - FVector(0.0, 0.0, 1500.0);
		if (UWorld* World = GetWorld())
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltAnchor), true, this);
			if (World->LineTraceSingleByObjectType(Hit, Fair, Fair - FVector(0.0, 0.0, 4000.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				AnchorGround = Hit.ImpactPoint + FVector(0.0, 0.0, 6.0);
			}
		}
		AnchorDepth = FMath::Max((float)(Fair.Z - AnchorGround.Z), 100.f);
		HoldPoint = X - Q.RotateVector(CoM);
		HoldYaw = (float)Q.Rotator().Yaw;
		bHoldSet = false;
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
	SetHold(Speed < 1.5 && ToMooring < MooringRadius ? ESaltBoatHold::Moored : ESaltBoatHold::Anchored);
}

void ASaltBoat::HandleSailRope(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	ToggleSail();
}

void ASaltBoat::HandleWindlass(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	ToggleHold();
	if (Hold == ESaltBoatHold::Moored)
	{
		// Home again: the end of a voyage saves the game (D46).
		if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this))
		{
			Saves->Autosave(GetWorld(), TEXT("moored"));
		}
	}
}

void ASaltBoat::HandleStore(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	if (ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr)
	{
		PC->OpenStore(Interactable == CrateLid ? CrateStore : BarrelStore);
	}
}

void ASaltBoat::HandleBell(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	RingBell();
}

void ASaltBoat::RingBell()
{
	// The rope pulls the clapper over to one side of the bell; it swings across and strikes (the bell rocks a little).
	const FVector Pull = Hull->GetComponentQuat().RotateVector(FVector(0.f, 1.f, 0.f));
	ClapperSwing->Kick(Pull * 140.f);
	BellSwing->Kick(-Pull * 25.f);
}

void ASaltBoat::HandleLantern(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	SetLanternLit(!bLanternLit);
}

void ASaltBoat::SetLanternLit(bool bLit)
{
	bLanternLit = bLit;
	LanternLight->SetVisibility(bLit);
	LanternLight->SetIntensity(28.f);
	LanternUse->PromptText = bLit ? NSLOCTEXT("Saltbound", "BoatLanternDouse", "Douse the lantern")
		: NSLOCTEXT("Saltbound", "BoatLanternLight", "Light the lantern");
	if (LanternGlassMaterial)
	{
		LanternGlassMaterial->SetScalarParameterValue(TEXT("GlowIntensity"), bLit ? 80.f : 0.f);
	}
}

void ASaltBoat::UpdateLantern(float DeltaSeconds)
{
	if (!bLanternLit)
	{
		return;
	}
	// The harbour lanterns' flicker: a slow breath and a small fast flutter.
	LanternFlicker += DeltaSeconds;
	const float Factor = 1.f + 0.08f * (0.7f * FMath::PerlinNoise1D(LanternFlicker * 0.9f + 17.f) + 0.3f * FMath::PerlinNoise1D(LanternFlicker * 7.3f + 41.f));
	LanternLight->SetIntensity(28.f * Factor);
	if (LanternGlassMaterial)
	{
		LanternGlassMaterial->SetScalarParameterValue(TEXT("GlowIntensity"), 80.f * Factor);
	}
}

FSaltLadderLine ASaltBoat::GetLadderLine(bool bStarboard, float CapsuleHalfHeight, float CapsuleRadius) const
{
	using namespace SaltHull;
	// Capsule centre: just outside the ladder (facing the hull), from the feet on the lowest rung (head out of the
	// water) up to the feet at the sheer; at the top, in over the gap onto the deck. Hull space, cm.
	const float Side = bStarboard ? 1.f : -1.f;
	const float Half = CapsuleHalfHeight * 0.01f;
	const float OnLadderY = Side * (HalfBeamAtSheer(LadderX) + LadderOut + 0.04f + CapsuleRadius * 0.01f + 0.04f);
	const float DeckZ = SheerZ(LadderX) + DeckCamber * 0.3f;
	FSaltLadderLine Line;
	Line.Base = Hull;
	Line.Bottom = FVector(LadderX, OnLadderY, LadderBottomZ + 0.08f + Half) * 100.0;
	Line.Top = FVector(LadderX, OnLadderY, DeckZ + Half + 0.1f) * 100.0;
	Line.TopExit = FVector(LadderX, Side * (HalfBeamAtSheer(LadderX) - 0.75f), DeckZ + Half + 0.03f) * 100.0;
	Line.FacingYaw = bStarboard ? -90.f : 90.f;
	Line.RungStep = 30.f;
	return Line;
}

void ASaltBoat::HandleLadder(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	if (!Player || Player->IsOnLadder())
	{
		return;
	}
	// D54: take hold of the ladder; from the water where the body is, from the deck at the top.
	const FSaltLadderLine Line = GetLadderLine(Interactable == LadderStarboard, Player->GetCapsuleHalfHeightForClimb(),
		Player->GetCapsuleComponent()->GetScaledCapsuleRadius());
	Player->MountLadder(Line, !Player->IsSwimming());
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
	UpdateBoomCapsule();
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
	UpdateAnchor(Dt);
	UpdateRopes(Dt);
	UpdateLantern(Dt);
	UpdateGangway();
	UpdateTrail(Dt);
	PushOceanMask(bMasksOcean);
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
	const float BoomReach = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(BoomAngle))) * SaltHull::BoomLength;
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(SaltHull::HalfLength + 0.8, FMath::Max(SaltHull::HalfBeam, BoomReach) + 0.8, 3.3) * 100.0);
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
	const double Resistance = HullSpeedResistance * (0.35 * Ur * Ur + 0.65 * FMath::Pow(Ur, 6.0)) + CoastDrag * FMath::Abs(U);
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
	const FVector RudderPoint = RudderFoilPoint();
	const FVector Vr = BodyVel(RudderPoint);
	const double Delta = FMath::DegreesToRadians(-RudderAngle);
	const FVector2D Fr = FoilForce(FVector2D(-Vr.X, -Vr.Y), FVector2D(FMath::Cos(Delta), FMath::Sin(Delta)), RudderArea, RhoWater, RudderCoefficients);
	AddBodyForceAt(FVector(Fr.X, Fr.Y, 0.0) * Wet, RudderPoint);
	// A little steerage at rest (sculling with the rudder), fading out with speed.
	const double RestFactor = 1.0 - FMath::Clamp(FMath::Abs(U) / 1.5, 0.0, 1.0);
	T += Up * (RestTurnTorque * (RudderAngle / FMath::Max(MaxRudderAngle, 1.f)) * RestFactor * Wet * (1.0 - AnchorHold));

	// Anchored (D43): a smooth stop, then the boat holds its place and heading (no swinging on the rode).
	if (AnchorHold > 0.001f)
	{
		const FVector Origin = X - Q.RotateVector(CoM);
		FVector Brake = -V * AnchorBrake;
		FVector Pull = bHoldSet ? (HoldPoint - Origin) * AnchorHoldSpring : FVector::ZeroVector;
		Brake.Z = Pull.Z = 0.0;
		AddForceAt((Brake + Pull) * AnchorHold, X);
		const float YawError = FMath::DegreesToRadians(FRotator::NormalizeAxis(HoldYaw - (float)Q.Rotator().Yaw));
		T.Z += ((bHoldSet ? AnchorYawSpring * YawError : 0.f) - AnchorYawBrake * W.Z) * AnchorHold;
	}

	// Sail (simple, D41): the set sail pushes the boat forward. The wind only shades the force a little (stronger
	// wind a bit faster, wind from ahead a bit slower) and its sideways share heels the boat to leeward.
	const FVector AirWorld(WindWorld.X, WindWorld.Y, 0.0);
	const double Phi = FMath::DegreesToRadians(BoomAngle);
	const FVector BoomDir(-FMath::Cos(Phi), FMath::Sin(Phi), 0.0);
	const FVector Tack(MastX, 0.f, SheerZ(MastX) + BoomHeight);
	const FVector CE = Tack + BoomDir * SailCEAft + FVector(0.0, 0.0, SailCEUp);
	const FVector Apparent = Inv.RotateVector(AirWorld - PointVelocity(BodyToWorld(CE)));
	LastApparentWind = FVector2D(Apparent.X, Apparent.Y);
	SailDrive = 0.f;
	if (Hoist > 0.01f)
	{
		const double TrueSpeed = AirWorld.Size();
		const FVector TrueB = Inv.RotateVector(AirWorld);
		// 0 = wind from dead ahead, 180 = from astern.
		const double FromDeg = TrueSpeed > 0.1 ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-TrueB.X / TrueSpeed, -1.0, 1.0))) : 90.0;
		const double AngleFactor = 0.6 + 0.4 * FMath::SmoothStep(15.0, 80.0, FromDeg) - 0.1 * FMath::SmoothStep(140.0, 180.0, FromDeg);
		const double SpeedFactor = FMath::Clamp(TrueSpeed / 7.0, 0.3, 1.5);
		const double Factor = FMath::Lerp(1.0, SpeedFactor * AngleFactor, (double)WindInfluence);
		const double Fx = SailThrust * Hoist * Factor * (1.0 - AnchorHold);
		const double AppSpeed = FMath::Max(LastApparentWind.Size(), 0.1);
		const double Fy = Fx * SailHeel * FMath::Clamp(Apparent.Y / AppSpeed, -1.0, 1.0);
		// Applied on the centreline at the sail's height (heel, no twist): with the boom far out, the real centre of
		// effort would turn the strong drive into a yaw the rudder cannot hold.
		AddBodyForceAt(FVector(Fx, Fy, 0.0), FVector(MastX - 0.8f, 0.f, SheerZ(MastX) + BoomHeight + SailCEUp));
		SailDrive = (float)Fx;
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
	}
	RudderAngle = FMath::FInterpConstantTo(RudderAngle, RudderTarget, DeltaSeconds,
		FMath::IsNearlyZero(RudderTarget) ? RudderReturnRate : RudderRate);
	Hoist = FMath::FInterpConstantTo(Hoist, bSailSet ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(SetTime, 0.1f));

	// The boom trims itself: about half the angle the wind blows past (abeam 45 degrees, running 75), centred when
	// the sail is taken in. Dead downwind it stays on its side until the wind is 15 degrees on the other side.
	float Target = 0.f;
	const double AppSpeed = LastApparentWind.Size();
	if (Hoist > 0.05f && AppSpeed > 0.3)
	{
		const float Free = FMath::RadiansToDegrees(FMath::Atan2(LastApparentWind.Y, -LastApparentWind.X));
		const bool bByTheLee = FMath::Abs(Free) > 165.f && FMath::Sign(Free) != FMath::Sign(BoomAngle) && FMath::Abs(BoomAngle) > 1.f;
		const float Side = bByTheLee ? FMath::Sign(BoomAngle) : FMath::Sign(Free);
		Target = Side * FMath::Min(FMath::Abs(Free) * 0.45f + 5.f, 75.f) * Hoist;
		// Wind from ahead: the sail cannot fill and shivers.
		Luff = 1.f - FMath::SmoothStep(18.f, 40.f, FMath::Abs(Free));
	}
	else
	{
		Luff = 0.f;
	}
	BoomAngle = FMath::FInterpConstantTo(BoomAngle, Target, DeltaSeconds, BoomSwingRate);

	// Belly fill: a spring with little damping, so the sail pops full when it catches the wind and breathes with
	// the gusts (the wind speed against its own slow average).
	const float WindNow = (float)LastTrueWind.Size();
	AvgWind = AvgWind <= 0.f ? WindNow : FMath::Lerp(AvgWind, WindNow, FMath::Min(DeltaSeconds / 8.f, 1.f));
	const float Gust = AvgWind > 0.1f ? FMath::Clamp(WindNow / AvgWind, 0.8f, 1.25f) : 1.f;
	const float FillTarget = Hoist > 0.6f ? FMath::Lerp(1.f, 0.35f, Luff) * Gust : 0.12f * Hoist;
	const float Omega = 2.f * PI * 1.3f;
	FillVel += (Omega * Omega * (FillTarget - Fill) - 2.f * 0.22f * Omega * FillVel) * DeltaSeconds;
	Fill += FillVel * DeltaSeconds;
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
	// The sail material moves the cloth (M_SB_Sail): brailed up to the gaff when taken in, the belly filled to
	// leeward, a shiver along the leech when the wind comes from ahead and a little life always.
	const float Furl = 1.f - FMath::SmoothStep(0.f, 1.f, Hoist);
	for (UMaterialInstanceDynamic* Mid : SailMaterials)
	{
		Mid->SetScalarParameterValue(TEXT("Furl"), Furl);
		Mid->SetScalarParameterValue(TEXT("Fill"), FMath::Max(Fill, 0.f));
		Mid->SetScalarParameterValue(TEXT("Side"), SailSideVisual);
		Mid->SetScalarParameterValue(TEXT("Flutter"), 0.15f + 0.85f * Luff * Hoist);
	}
	Rudder->SetRelativeRotation(FRotator(0.f, -RudderAngle, 0.f));
	Wheel->SetRelativeRotation(FRotator(0.f, 0.f, RudderAngle * WheelRatio));
	SteeringOar->SetRelativeRotation(FRotator(0.f, -RudderAngle, 0.f));
	// The pennant and the ensign fly where the wind blows past the moving boat (it keeps its last direction in a calm)
	// and stream out as the wind freshens; they hang in light air. D54: the boat's way only, not the masthead's sway
	// as the boat rolls (that swung them from side to side), and they turn slowly.
	const FVector FlagAir = Q.Inverse().RotateVector(FVector(LastTrueWind.X - V.X, LastTrueWind.Y - V.Y, 0.0));
	const double AppSpeed = FVector2D(FlagAir.X, FlagAir.Y).Size();
	if (AppSpeed > 0.3)
	{
		const float Target = FMath::RadiansToDegrees(FMath::Atan2(FlagAir.Y, FlagAir.X));
		PennantYaw += FRotator::NormalizeAxis(Target - PennantYaw) * FMath::Min(DeltaSeconds * 1.2f, 1.f);
	}
	PennantWind = FMath::FInterpTo(PennantWind, FMath::Clamp((float)AppSpeed / 9.f, 0.f, 1.f), DeltaSeconds, 1.5f);
	PennantPivot->SetRelativeRotation(FRotator(0.f, PennantYaw, 0.f));
	EnsignPivot->SetRelativeRotation(FRotator(0.f, PennantYaw, 0.f));
	if (PennantMaterial)
	{
		PennantMaterial->SetScalarParameterValue(TEXT("Wind"), PennantWind);
	}
	for (UMaterialInstanceDynamic* Mid : EnsignMaterials)
	{
		Mid->SetScalarParameterValue(TEXT("Wind"), PennantWind);
	}
}

void ASaltBoat::UpdateHints()
{
	const double Speed = V.Size2D();
	const double ToMooring = FVector::Dist2D(GetActorLocation(), StartTransform.GetLocation()) * 0.01;
	FText WindlassText;
	switch (Hold)
	{
	case ESaltBoatHold::Moored: WindlassText = NSLOCTEXT("Saltbound", "BoatCastOff", "Cast off"); break;
	case ESaltBoatHold::Anchored: WindlassText = NSLOCTEXT("Saltbound", "BoatRaiseAnchor", "Raise the anchor"); break;
	default:
		WindlassText = Speed < 1.5 && ToMooring < MooringRadius ? NSLOCTEXT("Saltbound", "BoatMoor", "Moor the boat")
			: NSLOCTEXT("Saltbound", "BoatDropAnchor", "Drop the anchor");
		break;
	}
	Windlass->PromptText = WindlassText;
	const FText RopeText = bSailSet ? NSLOCTEXT("Saltbound", "BoatRaiseSail", "Raise the sail") : NSLOCTEXT("Saltbound", "BoatLowerSail", "Lower the sail");
	SailRopePort->PromptText = RopeText;
	SailRopeStarboard->PromptText = RopeText;
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
	const TCHAR* SailState = Hoist > 0.95f ? TEXT("sail down") : (Hoist < 0.05f ? TEXT("sail up") : TEXT("sail moving"));
	const FString Line1 = TEXT("A / D  steer");
	const FString Line2 = FString::Printf(TEXT("%.1f kn      %s      %s%s"), Speed * MsToKnots, SailState, *WindText,
		Hold == ESaltBoatHold::Moored ? TEXT("      moored") : (Hold == ESaltBoatHold::Anchored ? TEXT("      at anchor") : TEXT("")));
	Helm->ControlsHintText = FText::FromString(Line1 + TEXT("\n") + Line2);
}

void ASaltBoat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PushOceanMask(false);
	Super::EndPlay(EndPlayReason);
}

void ASaltBoat::PushOceanMask(bool bEnable)
{
	// The ocean material (setup_ocean.py, D42) cuts the sea out inside the hull shape (SaltHullShape.h) at this pose.
	if (!OceanCollection)
	{
		OceanCollection = LoadObject<UMaterialParameterCollection>(nullptr, SaltContent::OceanParameterCollection);
	}
	if (!OceanCollection || !GetWorld())
	{
		return;
	}
	const FVector L = GetActorLocation();
	const FVector Ax = Q.GetForwardVector(), Ay = Q.GetRightVector(), Az = Q.GetUpVector();
	UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TEXT("BoatPos"), FLinearColor(L.X, L.Y, L.Z, bEnable ? 1.f : 0.f));
	UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TEXT("BoatAxisX"), FLinearColor(Ax.X, Ax.Y, Ax.Z, 0.f));
	UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TEXT("BoatAxisY"), FLinearColor(Ay.X, Ay.Y, Ay.Z, 0.f));
	UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TEXT("BoatAxisZ"), FLinearColor(Az.X, Az.Y, Az.Z, 0.f));
	// Bow wave and wake (D44, SaltBoatWake.ush): shaped by the speed through the water.
	const float Forward = FMath::Max((float)FVector::DotProduct(V, FVector(Ax.X, Ax.Y, 0.0)), 0.f);
	UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TEXT("BoatWake"),
		FLinearColor(Forward, TrailClock, WakeStrength, bEnable && WakeStrength > 0.f ? 1.f : 0.f));
	// Trail (D45, SaltBoatWake.ush): the stern now, then the samples (newest first), two points per vector; the
	// speed there four per vector. Unused points repeat the last one with no strength.
	static const FName TrailP[8] = { TEXT("BoatTrailP0"), TEXT("BoatTrailP1"), TEXT("BoatTrailP2"), TEXT("BoatTrailP3"),
		TEXT("BoatTrailP4"), TEXT("BoatTrailP5"), TEXT("BoatTrailP6"), TEXT("BoatTrailP7") };
	static const FName TrailS[4] = { TEXT("BoatTrailS0"), TEXT("BoatTrailS1"), TEXT("BoatTrailS2"), TEXT("BoatTrailS3") };
	FVector2D P[16];
	float S[16];
	P[0] = FVector2D(GetActorTransform().TransformPosition(FVector(-470.0, 0.0, 0.0)));
	S[0] = bEnable ? TrailStrengthAt((float)V.Size2D()) * WakeStrength : 0.f;
	for (int32 K = 1; K < 16; ++K)
	{
		const bool bHas = TrailPoints.IsValidIndex(K - 1);
		P[K] = bHas ? TrailPoints[K - 1] : P[K - 1];
		S[K] = bHas && bEnable ? TrailStrength[K - 1] * WakeStrength : 0.f;
	}
	for (int32 I = 0; I < 8; ++I)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TrailP[I], FLinearColor(P[2 * I].X, P[2 * I].Y, P[2 * I + 1].X, P[2 * I + 1].Y));
	}
	for (int32 I = 0; I < 4; ++I)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TrailS[I], FLinearColor(S[4 * I], S[4 * I + 1], S[4 * I + 2], S[4 * I + 3]));
	}
	// How far the bow wave and Kelvin wake reach (D48): the way the boat has come on its recent straight course. They
	// are drawn along the boat's heading, so they must not reach back past where it set off from or turned.
	UKismetMaterialLibrary::SetVectorParameterValue(this, OceanCollection, TEXT("BoatWakeExtent"), FLinearColor(WakeExtentShown, 0.f, 0.f, 0.f));
}

float ASaltBoat::GetWakeExtent() const
{
	const FVector2D Heading = FVector2D(Q.GetForwardVector()).GetSafeNormal();
	FVector2D Ahead(GetActorTransform().TransformPosition(FVector(-470.0, 0.0, 0.0)));
	float Length = 0.f;
	for (int32 K = 0; K < TrailPoints.Num(); ++K)
	{
		const FVector2D Step = Ahead - TrailPoints[K];
		const double Len = Step.Size();
		// Stop where the boat was (nearly) still or headed more than 25 degrees off its present course.
		if (TrailStrength[K] < 0.05f || (Len > 50.0 && FVector2D::DotProduct(Step / Len, Heading) < 0.906))
		{
			break;
		}
		Length += (float)Len * 0.01f;
		Ahead = TrailPoints[K];
	}
	return Length;
}

void ASaltBoat::UpdateTrail(float DeltaSeconds)
{
	// The stern's path, one sample a second: the sea draws the trail along it (it follows the turns).
	TrailClock += DeltaSeconds;
	if (TrailPoints.Num() == 0 || TrailClock >= 1.f)
	{
		TrailClock = TrailPoints.Num() == 0 ? 0.f : FMath::Fmod(TrailClock, 1.f);
		TrailPoints.Insert(FVector2D(GetActorTransform().TransformPosition(FVector(-470.0, 0.0, 0.0))), 0);
		TrailStrength.Insert(TrailStrengthAt((float)V.Size2D()), 0);
		TrailPoints.SetNum(FMath::Min(TrailPoints.Num(), 15));
		TrailStrength.SetNum(TrailPoints.Num());
	}
	// The wake's reach eases (it would otherwise step back by one sample spacing each second) and stops at 40 m, where
	// the Kelvin wake has faded anyway.
	WakeExtentShown = FMath::FInterpTo(WakeExtentShown, FMath::Min(GetWakeExtent(), 40.f), DeltaSeconds, 3.f);
}

float ASaltBoat::TrailStrengthAt(float SpeedMs)
{
	// 0 at rest .. 1 at about top speed (4 m/s, 7.8 kn): a slow boat leaves a thin, sparse trail, a fast one a broad
	// dense one (D49).
	return FMath::Clamp(SpeedMs / 4.f, 0.f, 1.f);
}

void ASaltBoat::ClearTrail()
{
	TrailPoints.Reset();
	TrailStrength.Reset();
	TrailClock = 0.f;
	WakeExtentShown = 0.f;
}

void ASaltBoat::ApplySteeringLayout()
{
	using namespace SaltHull;
	const bool bWheel = Steering == ESaltSteering::Wheel;
	Wheel->SetVisibility(bWheel);
	Rudder->SetVisibility(bWheel);
	SteeringOar->SetVisibility(!bWheel);
	// The helm station: at the wheel's hub, or at the oar's handle with the helmsman behind it.
	const FVector At = bWheel ? FVector(WheelX, 0.f, SheerZ(WheelX) + WheelHeight)
		: OarAlong(OarHead) + FVector(0.f, -OarHandleLength * 0.6f, 0.04f);
	const FVector Stand = bWheel ? FVector(HelmsmanX, 0.f, SheerZ(HelmsmanX)) : FVector(OarHelmsmanX, OarHelmsmanY, SheerZ(OarHelmsmanX));
	const FVector Exit = bWheel ? FVector(HelmsmanX + 0.25f, 0.62f, SheerZ(HelmsmanX + 0.25f))
		: FVector(OarHelmsmanX + 0.45f, 0.05f, SheerZ(OarHelmsmanX + 0.45f));
	Helm->SetRelativeLocation(At * 100.0);
	Helm->SetBoxExtent(bWheel ? FVector(18.f, 52.f, 52.f) : FVector(28.f, 45.f, 26.f));
	Helm->StandOffset = (Stand - At) * 100.0;
	Helm->ExitOffset = (Exit - At) * 100.0;
}

FVector ASaltBoat::RudderFoilPoint() const
{
	using namespace SaltHull;
	if (Steering == ESaltSteering::Wheel)
	{
		return FVector(RudderHeelX - RudderChord * 0.5f, 0.f, (RudderHeelZ + RudderBladeTopZ) * 0.5f);
	}
	return OarAlong((OarTip + OarBladeTop) * 0.5f) + OarAft() * (OarBladeChord * 0.5f);
}

void ASaltBoat::UpdateRopes(float DeltaSeconds)
{
	// The cables simulate themselves (gravity, swing); here they get their ends and a length with some slack.
	auto Fit = [](UCableComponent* C, USceneComponent* End, float Slack, bool bVisible)
	{
		if (!C || !End)
		{
			return;
		}
		if (C->GetAttachedComponent() != End)
		{
			C->SetAttachEndToComponent(End);
			C->EndLocation = FVector::ZeroVector;
		}
		const float Dist = (float)FVector::Dist(C->GetComponentLocation(), End->GetComponentLocation());
		C->CableLength = Dist * Slack + 5.f;
		C->TileMaterial = FMath::Max(C->CableLength / 100.f, 0.5f);
		C->SetVisibility(bVisible);
	};
	// Mooring lines to the pier (their pier ends stay where the boat was moored).
	using namespace SaltHull;
	const FVector Reach = StartTransform.GetRotation().RotateVector(FVector(0.0, MooringReach, 0.0));
	const FVector PierZ(0.0, 0.0, StartTransform.GetLocation().Z + 115.0);
	auto PierPoint = [&](const FVector& BittBody)
	{
		FVector P = StartTransform.TransformPosition(BittBody * 100.0) + Reach;
		P.Z = PierZ.Z;
		return P;
	};
	MooringPierFore->SetWorldLocation(PierPoint(FVector(4.0, 0.22, 0.0)));
	MooringPierAft->SetWorldLocation(PierPoint(FVector(-4.15, 0.22, 0.0)));
	const bool bMoored = Hold == ESaltBoatHold::Moored;
	Fit(MooringLineFore, MooringPierFore, 1.05f, bMoored);
	Fit(MooringLineAft, MooringPierAft, 1.05f, bMoored);
	// Anchor rode from the cathead down to the anchor.
	Fit(AnchorLine, Anchor, 1.08f, AnchorAlpha > 0.01f);
	// The halyards hold the gaff up: taut, a centimetre to spare.
	for (UCableComponent* C : { ThroatHalyard.Get(), PeakHalyard.Get() })
	{
		USceneComponent* End = C == PeakHalyard ? GaffSpanBlock.Get() : GaffThroatEye.Get();
		Fit(C, End, 1.f, true);
		if (C && End)
		{
			C->CableLength = (float)FVector::Dist(C->GetComponentLocation(), End->GetComponentLocation()) + 1.f;
		}
	}
}

void ASaltBoat::UpdateAnchor(float DeltaSeconds)
{
	using namespace SaltHull;
	const float Target = Hold == ESaltBoatHold::Anchored ? 1.f : 0.f;
	const float Duration = Target > AnchorAlpha ? AnchorDropTime : AnchorRaiseTime;
	const float OldAlpha = AnchorAlpha;
	AnchorAlpha = FMath::FInterpConstantTo(AnchorAlpha, Target, DeltaSeconds, 1.f / FMath::Max(Duration, 0.1f));
	// The hold bites as the anchor reaches the bottom and lets go as soon as it is weighed.
	AnchorHold = Target > 0.f ? FMath::SmoothStep(0.35f, 1.f, AnchorAlpha) : 0.f;
	// A pure brake until the boat has come to rest; then it holds that place and heading (no swinging back).
	if (AnchorHold < 0.95f)
	{
		bHoldSet = false;
	}
	else if (!bHoldSet && V.Size2D() < 0.25 && FMath::Abs(W.Z) < 0.01)
	{
		bHoldSet = true;
	}
	if (!bHoldSet)
	{
		HoldPoint = X - Q.RotateVector(CoM);
		HoldYaw = (float)Q.Rotator().Yaw;
	}
	// The windlass turns with the rode running out (fast) or being hove in (slower); the coil on its drum thins.
	const float RodeCm = (float)AnchorDepth * 1.3f + 150.f;
	WindlassAngle += (AnchorAlpha - OldAlpha) * RodeCm / (2.f * PI * WindlassDrumRadius * 100.f) * 360.f;
	WindlassDrum->SetRelativeRotation(FRotator(WindlassAngle, 0.f, 0.f));
	const float Coil = FMath::Lerp(1.f, 0.82f, AnchorAlpha);
	WindlassRope->SetRelativeScale3D(FVector(Coil, FMath::Lerp(1.f, 0.7f, AnchorAlpha), Coil));
	if (AnchorAlpha <= 0.001f)
	{
		// Stowed: hanging from the cathead.
		Anchor->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -4.f), FRotator::ZeroRotator);
		return;
	}
	// Down to the seabed (and back up), lying over as it settles; it stays put while the boat swings around it.
	const FVector Fair = Hawse->GetComponentLocation();
	const float A = FMath::InterpEaseInOut(0.f, 1.f, AnchorAlpha, 2.f);
	const FVector P = FMath::Lerp(Fair - FVector(0.0, 0.0, 4.0), AnchorGround, A);
	const float Yaw = (float)Q.Rotator().Yaw;
	Anchor->SetWorldLocationAndRotation(P, FRotator(-75.f * FMath::SmoothStep(0.85f, 1.f, AnchorAlpha), Yaw, 0.f));
}

void ASaltBoat::UpdateGangway()
{
	using namespace SaltHull;
	// Moored: the gangway rests on the pier (fixed) and its other end on the boat at the gap, so it follows the boat.
	const bool bShow = Hold == ESaltBoatHold::Moored;
	Gangway->SetVisibility(bShow);
	Gangway->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (!bShow)
	{
		return;
	}
	FVector Pier = StartTransform.TransformPosition(FVector(LadderX * 100.f, MooringReach + 15.f, 0.f));
	Pier.Z = StartTransform.GetLocation().Z + 111.f;
	const FVector OnBoat = Hull->GetComponentTransform().TransformPosition(FVector(LadderX, HalfBeamAtSheer(LadderX) - 0.2f, SheerZ(LadderX) + 0.03f) * 100.0);
	Gangway->SetWorldLocationAndRotation(Pier, (OnBoat - Pier).Rotation());
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
		TEXT("rudder=%.1f drive=%.0fN boom=%.1f fill=%.2f sail=%.2f hold=%s immersion=%.2f waterline=%.1fcm contacts=%d depth=%.1fcm ")
		TEXT("twa=%.1f tws=%.2f aws=%.2f mass=%.0f"),
		L.X, L.Y, L.Z, Yaw, CourseYaw, GetSpeedKnots(), Vb.X, Vb.Y, FMath::RadiansToDegrees(W.Z), GetHeelDeg(), GetPitchDeg(),
		RudderAngle, SailDrive, BoomAngle, Fill, Hoist, HoldName, Immersion, GetWaterlineOffset(), Contacts.Num(), GetContactDepth(),
		FRotator::NormalizeAxis(FromYaw - Yaw), LastTrueWind.Size(), LastApparentWind.Size(), Mass);
}

// Saltbound - the sailing boat: a kinematic rigid body moved by our own integrator (D7, D36).

#include "Boat/SaltBoat.h"
#include "Hub/SaltHubPassage.h"

#include "Boat/SaltHullDamage.h"
#include "Boat/SaltHullShape.h"
#include "CableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Core/SaltPlaces.h"
#include "Core/SaltStats.h"
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
#include "UObject/UObjectIterator.h"
#include "World/SaltWindSubsystem.h"

namespace
{
	/** D56 A/B: 1 brings back the old behaviour where the sail's drive and the windage on a heeled boat turned it. */
	TAutoConsoleVariable<int32> CVarRigTurn(TEXT("sb.Boat.RigTurn"), 0,
		TEXT("1: forces high on the rig turn a heeled boat as before D56 (the waves then steer it); 0: they heel it only (default)."));

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
	HullDamage = CreateDefaultSubobject<USaltHullDamageComponent>(TEXT("HullDamage"));

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

	// D65: the square sail hangs from its yard, slung across the mast's front and fixed square to the boat.
	YardPivot = Pivot(TEXT("YardPivot"), Hull, FVector(YardX, 0.f, SheerZ(MastX) + YardHeight));
	Yard = Part(TEXT("Yard"), YardPivot);
	// A new name (SaltBoat.h): the levels' saved boats kept the old sail's attachment to the boom (gone) and its pointer.
	SquareSail = Part(TEXT("SquareSail"), YardPivot);
	// The sail's own vertex animation (belly, rolling up) moves it beyond the mesh bounds (the import widens them too).
	SquareSail->SetBoundsScale(1.5f);
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
	// D59: 30 % bigger (build_boat.py BARREL and CRATE: the barrel 0.27 m round and 0.86 m high, the crate 0.68 x 0.5 x 0.48 m).
	BarrelLid = Interaction(TEXT("BarrelLid"), FVector(-1.65f, -1.0f, SheerZ(-1.65f) + 0.48f), FVector(36.f, 36.f, 48.f));
	CrateLid = Interaction(TEXT("CrateLid"), FVector(-2.42f, -0.95f, SheerZ(-2.42f) + 0.3f), FVector(40.f, 31.f, 32.f));
	BarrelLid->PromptText = NSLOCTEXT("Saltbound", "OpenBarrel", "Open the barrel");
	CrateLid->PromptText = NSLOCTEXT("Saltbound", "OpenCrate", "Open the crate");
	BarrelStore = CreateDefaultSubobject<USaltInventoryComponent>(TEXT("BarrelStore"));
	CrateStore = CreateDefaultSubobject<USaltInventoryComponent>(TEXT("CrateStore"));
	BarrelStore->InitStore(TEXT("Boat_Barrel"), NSLOCTEXT("Saltbound", "Barrel", "Barrel"), SaltStats::Boat(BoatClass).BarrelSlots);
	CrateStore->InitStore(TEXT("Boat_Crate"), NSLOCTEXT("Saltbound", "Crate", "Crate"), SaltStats::Boat(BoatClass).CrateSlots);
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

	// D65: the sheets from the pin rails' forward pins to the clews, and the brails from their blocks under the yard down to
	// the foot (hauled, they roll the sail up); their sail ends follow the cloth (UpdateRopes). The brails hang taut.
	ClewPort = Pivot(TEXT("ClewPort"), YardPivot, FVector::ZeroVector);
	ClewStarboard = Pivot(TEXT("ClewStarboard"), YardPivot, FVector::ZeroVector);
	SheetPort = Rope(TEXT("SheetPort"), Pivot(TEXT("SheetPinPort"), Hull, SheetPin(-1.f)));
	SheetStarboard = Rope(TEXT("SheetStarboard"), Pivot(TEXT("SheetPinStarboard"), Hull, SheetPin(1.f)));
	BrailFootPort = Pivot(TEXT("BrailFootPort"), YardPivot, FVector::ZeroVector);
	BrailFootStarboard = Pivot(TEXT("BrailFootStarboard"), YardPivot, FVector::ZeroVector);
	BrailPort = Rope(TEXT("BrailPort"), Pivot(TEXT("BrailBlockPort"), YardPivot, BrailBlock(-1.f)));
	BrailStarboard = Rope(TEXT("BrailStarboard"), Pivot(TEXT("BrailBlockStarboard"), YardPivot, BrailBlock(1.f)));
	for (UCableComponent* C : { SheetPort.Get(), SheetStarboard.Get() })
	{
		C->CableWidth = 2.4f;
		C->NumSegments = 12;
	}
	for (UCableComponent* C : { BrailPort.Get(), BrailStarboard.Get() })
	{
		C->CableWidth = 2.f;
		C->NumSegments = 10;
		C->SolverIterations = 16;
		C->CableGravityScale = 0.1f;
	}

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

	// D57: the chart on the mast. Its mesh is built in the boat's frame (build_boat.py, build_mast_chart), so it sits on
	// the hull at no offset; the board faces aft and to starboard, its middle 1.1 m above the deck.
	MastChart = Part(TEXT("MastChart"), Hull);
	MastChartUse = Interaction(TEXT("MastChartUse"), FVector(MastX - 0.13f, 0.13f, SheerZ(MastX) + 1.10f), FVector(14.f, 14.f, 20.f));
	MastChartUse->PromptText = NSLOCTEXT("Saltbound", "BoatChartLook", "Look at the chart");
}

void ASaltBoat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveContent();
	ApplySteeringLayout();
	StartTransform = Transform;
	UpdateHomeMooring();
	Mooring = Home;
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
	Resolve(Yard, TEXT("Yard"));
	Resolve(SquareSail, TEXT("Sail"));
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
	Resolve(MastChart, TEXT("MastChart"));
	UMaterialInterface* RopeMaterial = LoadObject<UMaterialInterface>(nullptr, SaltContent::RopeMaterial);
	for (UCableComponent* C : { AnchorLine.Get(), MooringLineFore.Get(), MooringLineAft.Get(), SheetPort.Get(), SheetStarboard.Get(),
		BrailPort.Get(), BrailStarboard.Get() })
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
	for (int32 I = 0; SquareSail && I < SquareSail->GetNumMaterials(); ++I)
	{
		UMaterialInterface* Base = SquareSail->GetMaterial(I);
		if (!Base)
		{
			continue;
		}
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Base);
		if (!Mid)
		{
			Mid = UMaterialInstanceDynamic::Create(Base, this);
			SquareSail->SetMaterial(I, Mid);
		}
		// D65: the sail's shape as SaltHullShape.h has it (the sheets' and brails' ends are put where these move the cloth).
		using namespace SaltHull;
		Mid->SetScalarParameterValue(TEXT("HeadZ"), SailHeadZ * 100.f);
		Mid->SetScalarParameterValue(TEXT("Drop"), SailDrop * 100.f);
		Mid->SetScalarParameterValue(TEXT("HalfHead"), SailHalfHead * 100.f);
		Mid->SetScalarParameterValue(TEXT("HalfFoot"), SailHalfFoot * 100.f);
		Mid->SetScalarParameterValue(TEXT("RollLayer"), SailRollLayer * 100.f);
		Mid->SetScalarParameterValue(TEXT("RollCore"), SailRollCore * 100.f);
		Mid->SetScalarParameterValue(TEXT("CutMin"), SailCutMin * 100.f);
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

void ASaltBoat::ApplyStats()
{
	// D59: the boat's numbers come from its row in the stats table (Core/SaltStats, Docs/Stats.md).
	const FSaltBoatStats& S = SaltStats::Boat(BoatClass);
	SailThrust = S.SailThrust;
	SailDriveAbeam = S.SailDriveAbeam;
	SailDriveHeadToWind = S.SailDriveHeadToWind;
	HullSpeed = S.HullSpeed;
	HullSpeedResistance = S.HullResistance;
	CoastDrag = S.CoastDrag;
	SetTime = S.SetTime;
	TrimTime = S.TrimTime;
	SailBelly = S.SailBelly;
	MaxRudderAngle = S.MaxRudderAngle;
	RudderRate = S.RudderRate;
	RestTurnTorque = S.RestTurnTorque;
}

void ASaltBoat::BeginPlay()
{
	Super::BeginPlay();
	ApplyStats();
	ResolveContent();
	ApplySteeringLayout();
	BuildHullModel();
	StartTransform = GetActorTransform();
	UpdateHomeMooring();
	Mooring = Home;
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
	MastChartUse->OnInteract.AddDynamic(this, &ASaltBoat::HandleChart);
	// The clapper swings after the bell has moved this frame.
	ClapperSwing->AddTickPrerequisiteComponent(BellSwing);
	SetupSailMaterials();
	const int32 ChartSlot = MastChart->GetMaterialIndex(TEXT("MI_MastChart"));
	MastChartMaterial = ChartSlot != INDEX_NONE ? MastChart->CreateAndSetMaterialInstanceDynamic(ChartSlot) : nullptr;
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

	// D65: the square sail's centre of effort: the centroid of its cloth (a trapezoid from the head to the wider foot).
	{
		const float Below = SailDrop * (SailHalfHead + 2.f * SailHalfFoot) / (3.f * (SailHalfHead + SailHalfFoot));
		SailCEHeight = YardHeight + SailHeadZ - Below;
		UE_LOG(LogSaltbound, Log, TEXT("Boat sail: area %.1f m2, centre of effort %.2f m above the deck"),
			SailDrop * (SailHalfHead + SailHalfFoot), SailCEHeight);
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
	// anchor, rudder blade and head, chain wales and deadeyes, fenders, ladders. The rig aloft does not (the mast, and since
	// D65 the yard and the square sail, which hang over the deck and never swing out).
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
		// D65: the chain wales and deadeyes lie aft of the mast (build_boat.py rig_and_fittings).
		const float Cx0 = MastX - 0.75f, Cx1 = MastX + 0.05f;
		const float Cy = Side * (HalfBeamAtSheer(MastX - 0.325f) + 0.16f);
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

void ASaltBoat::UpdateHomeMooring()
{
	Home.Id = TEXT("Home");
	Home.Pose = FTransform(FRotator(0.0, StartTransform.Rotator().Yaw, 0.0), StartTransform.GetLocation());
	Home.Reach = MooringReach;
	Home.PierHeight = MooringPierHeight;
}

bool ASaltBoat::FindMooring(FSaltMooring& Out, double* OutDistanceM) const
{
	double Best = FVector::Dist2D(GetActorLocation(), Home.Pose.GetLocation()) * 0.01;
	Out = Home;
	for (TObjectIterator<USaltMooringPointComponent> It; It; ++It)
	{
		if (It->GetWorld() != GetWorld() || !It->IsRegistered())
		{
			continue;
		}
		const double D = FVector::Dist2D(GetActorLocation(), It->GetComponentLocation()) * 0.01;
		if (D < Best)
		{
			Best = D;
			Out = It->GetMooring();
		}
	}
	if (OutDistanceM)
	{
		*OutDistanceM = Best;
	}
	return Best < MooringRadius;
}

void ASaltBoat::MoorAt(const FSaltMooring& InMooring)
{
	SetHold(ESaltBoatHold::Free);
	Mooring = InMooring.IsValid() ? InMooring : Home;
	PlaceAt(Mooring.Pose.GetLocation(), Mooring.Pose.Rotator().Yaw);
	bSailSet = false;
	SailTarget = 0.f;
	Hoist = 0.f;
	Fill = FillVel = 0.f;
	RudderAngle = 0.f;
	SailLean = 0.f;
	SetHold(ESaltBoatHold::Moored);
	UE_LOG(LogSaltbound, Display, TEXT("Boat moored at %s"), *Mooring.Id.ToString());
}

void ASaltBoat::RestoreState(const FVector& Location, float Yaw, ESaltBoatHold InHold, bool bSail, const FSaltMooring& InMooring)
{
	if (InHold == ESaltBoatHold::Moored)
	{
		if (InMooring.IsValid() && InMooring.Id != Home.Id)
		{
			MoorAt(InMooring);
		}
		else
		{
			ResetToStart();
		}
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
	SailTarget = bSail ? 1.f : 0.f;
	Hoist = bSail ? 1.f : 0.f;
	Fill = FillVel = 0.f;
}

void ASaltBoat::ResetToStart()
{
	SetHold(ESaltBoatHold::Free);
	Mooring = Home;
	PlaceAt(StartTransform.GetLocation(), StartTransform.Rotator().Yaw);
	bSailSet = false;
	SailTarget = 0.f;
	Hoist = 0.f;
	Fill = FillVel = 0.f;
	RudderAngle = 0.f;
	SailLean = 0.f;
	SetHold(ESaltBoatHold::Moored);
	UE_LOG(LogSaltbound, Display, TEXT("Boat reset to its mooring"));
}

void ASaltBoat::Recover()
{
	FVector Origin = Mooring.Pose.GetLocation();
	float Yaw = Mooring.Pose.Rotator().Yaw;
	if (SafePoses.Num() > 0)
	{
		Origin = SafePoses.Last().Origin;
		Yaw = SafePoses.Last().Yaw;
	}
	SetHold(ESaltBoatHold::Free);
	PlaceAt(Origin, Yaw);
	bSailSet = false;
	SailTarget = 0.f;
	Hoist = 0.f;
	Fill = FillVel = 0.f;
	RudderAngle = 0.f;
	UE_LOG(LogSaltbound, Display, TEXT("Boat recovered upright at (%.0f, %.0f)"), Origin.X, Origin.Y);
}

void ASaltBoat::SetSailSet(bool bSet)
{
	bSailSet = bSet;
	SailTarget = bSet ? 1.f : 0.f;
}

void ASaltBoat::SetSailAmount(float Amount)
{
	SailTarget = FMath::Clamp(Amount, 0.f, 1.f);
	bSailSet = SailTarget > 0.f;
}

void ASaltBoat::SetControlOverride(bool bEnable, float Rudder01)
{
	bOverride = bEnable;
	OverrideRudder = FMath::Clamp(Rudder01, -1.f, 1.f);
}

void ASaltBoat::SetScripted(bool bEnable)
{
	if (bEnable == bScripted)
	{
		return;
	}
	bScripted = bEnable;
	if (bEnable)
	{
		SetHold(ESaltBoatHold::Free);
		ScriptedHeel = 0.f;
	}
	// Either way the water under her and her contacts are found afresh.
	for (FColumn& C : Columns)
	{
		C.bHasWater = false;
	}
	Contacts.Reset();
}

void ASaltBoat::SetScriptedPose(const FVector& LocationCm, float Yaw, float DeltaSeconds)
{
	const USaltOceanSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<USaltOceanSubsystem>() : nullptr;
	const double WaterZ = Sub ? Sub->SampleWater(FVector2D(LocationCm.X, LocationCm.Y)).Location.Z : 0.0;
	const FVector OldOrigin = X - Q.RotateVector(CoM);
	const float YawRate = DeltaSeconds > 0.f ? FRotator::NormalizeAxis(Yaw - (float)Q.Rotator().Yaw) / DeltaSeconds : 0.f;
	if (DeltaSeconds > 0.f)
	{
		ScriptedHeel = FMath::FInterpTo(ScriptedHeel, FMath::Clamp(-YawRate * 0.12f, -3.f, 3.f), DeltaSeconds, 1.5f);
	}
	const FVector Origin = FVector(LocationCm.X, LocationCm.Y, WaterZ) * 0.01;
	Q = FRotator(0.f, Yaw, ScriptedHeel).Quaternion();
	X = Origin + Q.RotateVector(CoM);
	if (DeltaSeconds > 0.f)
	{
		V = (Origin - OldOrigin) / DeltaSeconds;
		W = FVector(0.0, 0.0, FMath::DegreesToRadians(YawRate));
	}
}

void ASaltBoat::MoorAtStart()
{
	using namespace SaltHull;
	const FVector Bow(HalfLength - 0.3f, 0.f, SheerZ(HalfLength - 0.3f));
	const FVector Stern(-HalfLength + 0.3f, 0.f, SheerZ(HalfLength - 0.3f));
	const FTransform StartM(Mooring.Pose.GetRotation(), Mooring.Pose.GetLocation() * 0.01);
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
	FSaltMooring Near;
	if (Speed < 1.5 && FindMooring(Near))
	{
		Mooring = Near;
		SetHold(ESaltBoatHold::Moored);
		return;
	}
	SetHold(ESaltBoatHold::Anchored);
}

void ASaltBoat::HandleSailRope(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	// FH1 (D77): the prologue's ripped sail is not set until it has been mended.
	if (bSailLocked && !bSailSet)
	{
		if (ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr)
		{
			PC->ShowNotice(SailLockedText);
		}
		return;
	}
	ToggleSail();
}

void ASaltBoat::HandleWindlass(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	// D70: casting off at one of the hub's berths is the departure (asked first; the passage takes her out).
	if (Hold == ESaltBoatHold::Moored && Mooring.Id.ToString().StartsWith(TEXT("Hub")))
	{
		if (ASaltHubPassage* Passage = ASaltHubPassage::Get(GetWorld()))
		{
			if (!Passage->IsRunning() && !bScripted)
			{
				Passage->AskDeparture(this, Player);
			}
			return;
		}
	}
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

FVector2D ASaltBoat::GetChartCentre() const
{
	return SaltChart::ToUV(GetActorLocation());
}

float ASaltBoat::GetMastChartParameter(FName Name) const
{
	float Value = -1.f;
	if (MastChartMaterial)
	{
		MastChartMaterial->GetScalarParameterValue(FMaterialParameterInfo(Name), Value);
	}
	return Value;
}

FTransform ASaltBoat::GetChartFaceLocal() const
{
	// build_boat.py (build_mast_chart): the sheet's middle 1.10 m above the deck, facing aft and to starboard (135 degrees),
	// 0.194 m out from the mast's axis (the mast's 0.135 m there, the chocks 0.035, the board 0.022, the sheet 0.0015).
	constexpr float FaceYaw = 135.f;
	constexpr float FaceOut = 0.194f;
	constexpr float FaceUp = 1.10f;
	const float Yaw = FMath::DegreesToRadians(FaceYaw);
	const FVector Out(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
	const FVector At = FVector(SaltHull::MastX + Out.X * FaceOut, Out.Y * FaceOut, SaltHull::SheerZ(SaltHull::MastX) + FaceUp) * 100.0;
	return FTransform(FRotationMatrix::MakeFromXZ(-Out, FVector::UpVector).ToQuat(), At);
}

FTransform ASaltBoat::GetChartViewLocal(float FieldOfView, float Aspect) const
{
	const float TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FieldOfView, 30.f, 120.f) * 0.5f));
	const float TanV = TanH / FMath::Max(Aspect, 0.5f);
	// The sheet 78 % of the view's height, and never more than 70 % of its width (narrow windows).
	const float Back = 100.f * FMath::Max(SaltChart::SheetHeightM * 0.5f / (0.78f * TanV), SaltChart::SheetWidthM * 0.5f / (0.7f * TanH));
	const FTransform Face = GetChartFaceLocal();
	return FTransform(Face.GetRotation(), Face.GetLocation() - Face.GetUnitAxis(EAxis::X) * Back);
}

void ASaltBoat::UpdateMastChart()
{
	// The chart follows the boat: its window centred on her, north up, her arrow along her heading.
	if (MastChartMaterial)
	{
		const FVector2D C = GetChartCentre();
		MastChartMaterial->SetScalarParameterValue(TEXT("CentreU"), (float)C.X);
		MastChartMaterial->SetScalarParameterValue(TEXT("CentreV"), (float)C.Y);
		MastChartMaterial->SetScalarParameterValue(TEXT("Heading"), FMath::DegreesToRadians(GetYawDeg()));
	}
}

void ASaltBoat::HandleChart(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player)
{
	if (ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr)
	{
		PC->OpenChart(this);
	}
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

void ASaltBoat::TakeLaddersOnTouch()
{
	// D59 (owner: "just running in the ladder does nothing"): moving into a ladder takes hold of it as E does. From the
	// water: swimming at it, within its width and an arm's length of it. From the deck: walking out at its head (not where
	// the gangway lies while moored: that way leads ashore).
	ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetWorld() ? GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr : nullptr);
	if (!Player || !Hull || Player->IsOnLadder() || Player->IsClimbing() || Player->IsAtStation() || Player->IsViewFocused())
	{
		return;
	}
	const FTransform Xf = Hull->GetComponentTransform();
	FVector Push = Xf.InverseTransformVectorNoScale(Player->GetLastMovementInputVector());
	Push.Z = 0.0;
	if (Push.Size() < 0.3)
	{
		return;
	}
	Push.Normalize();
	const FVector Body = Xf.InverseTransformPosition(Player->GetActorLocation());
	const float Half = Player->GetCapsuleHalfHeightForClimb();
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	for (const bool bStarboard : { false, true })
	{
		const float Side = bStarboard ? 1.f : -1.f;
		const FSaltLadderLine Line = GetLadderLine(bStarboard, Half, Radius);
		const bool bAlong = FMath::Abs(Body.X - Line.Bottom.X) < 45.f;
		if (Player->IsSwimming())
		{
			// Outboard of the ladder by at most an arm's length, between its foot and its head, swimming towards the hull.
			const float Out = (float)((Body.Y - Line.Bottom.Y) * Side);
			if (bAlong && Out > -25.f && Out < 70.f && Body.Z > Line.Bottom.Z - 120.f && Body.Z < Line.Top.Z && Push.Y * Side < -0.55)
			{
				Player->MountLadder(Line, false);
				return;
			}
		}
		else if (Player->GetMovementBaseObject() == Hull && !(bStarboard && Hold == ESaltBoatHold::Moored))
		{
			// At the gap above it, walking out towards it.
			const float Out = (float)((Body.Y - Line.TopExit.Y) * Side);
			if (bAlong && Out > 35.f && FMath::Abs(Body.Z - Line.TopExit.Z) < 40.f && Push.Y * Side > 0.55)
			{
				Player->MountLadder(Line, true);
				return;
			}
		}
	}
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

	if (bScripted)
	{
		// D70: a passage sails her; only how she looks follows.
		UpdateControls(Dt);
		ApplyTransform(Dt);
		UpdateVisuals(Dt);
		UpdateRopes(Dt);
		UpdateLantern(Dt);
		UpdateMastChart();
		UpdateTrail(Dt);
		PushOceanMask(bMasksOcean);
		return;
	}
	UpdateControls(Dt);
	SampleWater(Dt);
	FindContacts();
	// D74: the hardest knock this frame (a contact touching and closing), for the hull damage.
	if (OnHullKnock.IsBound())
	{
		float Fastest = 0.f;
		FVector Where = FVector::ZeroVector;
		for (const FContact& C : Contacts)
		{
			const FVector P = BodyToWorld(C.BodyPoint);
			if (FVector::DotProduct(C.PlanePoint - P, C.Normal) < -0.02)
			{
				continue;
			}
			const float Closing = (float)-FVector::DotProduct(PointVelocity(P), C.Normal);
			if (Closing > Fastest)
			{
				Fastest = Closing;
				Where = C.BodyPoint;
			}
		}
		if (Fastest > 0.f)
		{
			OnHullKnock.Broadcast(Where, Fastest);
		}
	}

	const FVector OldX = X;
	const FQuat OldQ = Q;
	const double OldVz = V.Z;
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
	// A sudden heave (more than 0.5 m/s in one frame) is not the sea: say where it came from (D56 diagnosis).
	if (FMath::Abs(V.Z - OldVz) > 0.5)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_BOAT_KICK dVz=%.2f m/s in %.4f s: Fz=%.0f N (buoyancy %.0f, heave damping %.0f, weight %.0f), contacts=%d immersion=%.2f"),
			V.Z - OldVz, Dt, KickTotal, KickBuoyancy, KickDamping, -Mass * 9.80665, Contacts.Num(), Immersion);
	}
	PreventTunnelling(OldX, OldQ);
	ApplyTransform(Dt);
	UpdateVisuals(Dt);
	UpdateAnchor(Dt);
	UpdateRopes(Dt);
	UpdateLantern(Dt);
	UpdateMastChart();
	UpdateGangway();
	TakeLaddersOnTouch();
	UpdateTrail(Dt);
	PushOceanMask(bMasksOcean);
	UpdateHints();
	UpdateRecovery(Dt);
}

void ASaltBoat::SampleWater(float DeltaSeconds)
{
	const USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>();
	// The sea under each column's middle: world X, Y and the water's height (m).
	TArray<FVector, TInlineAllocator<96>> Sea;
	Sea.SetNumUninitialized(Columns.Num());
	for (int32 I = 0; I < Columns.Num(); ++I)
	{
		const FColumn& C = Columns[I];
		const FVector Mid = BodyToWorld(C.Bottom + FVector(0.0, 0.0, C.Height * 0.5));
		Sea[I] = FVector(Mid.X, Mid.Y, Sub ? Sub->SampleWater(FVector2D(Mid.X * 100.0, Mid.Y * 100.0)).Location.Z * 0.01 : 0.0);
	}
	// D65: the water the hull feels, blended towards a plane: at the height of the columns' water (weighted by their area), with
	// the slope of the sea over WaveFootprint times the hull's size (a wider footprint averages the short waves' slopes out
	// and keeps the long swell's).
	if (WaveFilter > 0.f && Columns.Num() >= 3)
	{
		double Sw = 0.0, Sx = 0.0, Sy = 0.0, Sz = 0.0;
		for (int32 I = 0; I < Columns.Num(); ++I)
		{
			const double Wt = Columns[I].Area;
			Sw += Wt;
			Sx += Wt * Sea[I].X;
			Sy += Wt * Sea[I].Y;
			Sz += Wt * Sea[I].Z;
		}
		const double Mx = Sx / Sw, My = Sy / Sw, Mz = Sz / Sw;
		double Sxx = 0.0, Sxy = 0.0, Syy = 0.0, Sxz = 0.0, Syz = 0.0;
		for (int32 I = 0; I < Columns.Num(); ++I)
		{
			const double Wt = Columns[I].Area;
			const double Dx = Sea[I].X - Mx, Dy = Sea[I].Y - My, Dz = Sea[I].Z - Mz;
			Sxx += Wt * Dx * Dx;
			Sxy += Wt * Dx * Dy;
			Syy += Wt * Dy * Dy;
			Sxz += Wt * Dx * Dz;
			Syz += Wt * Dy * Dz;
		}
		if (WaveFootprint > 1.01f && Sub)
		{
			// The slope over the wider footprint: nine samples on a grid round the hull's middle, along its heading.
			const FVector2D Fwd = FVector2D(Q.GetForwardVector()).GetSafeNormal();
			const FVector2D Right(-Fwd.Y, Fwd.X);
			const FVector2D Mid(Mx, My);
			Sxx = Sxy = Syy = Sxz = Syz = 0.0;
			double Mean = 0.0;
			FVector Wide[9];
			for (int32 K = 0; K < 9; ++K)
			{
				const FVector2D P = Mid + Fwd * ((K % 3 - 1) * SaltHull::HalfLength * WaveFootprint) + Right * ((K / 3 - 1) * SaltHull::HalfBeam * WaveFootprint);
				Wide[K] = FVector(P.X, P.Y, Sub->SampleWater(P * 100.0).Location.Z * 0.01);
				Mean += Wide[K].Z / 9.0;
			}
			for (const FVector& S : Wide)
			{
				const double Dx = S.X - Mx, Dy = S.Y - My, Dz = S.Z - Mean;
				Sxx += Dx * Dx;
				Sxy += Dx * Dy;
				Syy += Dy * Dy;
				Sxz += Dx * Dz;
				Syz += Dy * Dz;
			}
		}
		const double Det = Sxx * Syy - Sxy * Sxy;
		if (Det > 1e-9)
		{
			const double Bx = (Sxz * Syy - Syz * Sxy) / Det;
			const double By = (Syz * Sxx - Sxz * Sxy) / Det;
			for (int32 I = 0; I < Columns.Num(); ++I)
			{
				const double Plane = Mz + Bx * (Sea[I].X - Mx) + By * (Sea[I].Y - My);
				Sea[I].Z = FMath::Lerp(Sea[I].Z, Plane, (double)WaveFilter);
			}
		}
	}
	const double KeepZ = WaterHeightSmoothing > 0.f && DeltaSeconds > 0.f ? FMath::Exp(-DeltaSeconds / WaterHeightSmoothing) : 0.0;
	const double KeepVz = DeltaSeconds > 0.f ? FMath::Exp(-DeltaSeconds / FMath::Max(WaterVelocitySmoothing, 0.001f)) : 1.0;
	for (int32 I = 0; I < Columns.Num(); ++I)
	{
		FColumn& C = Columns[I];
		double Z = Sea[I].Z;
		// Along the column's path: includes the slope the hull moves over (encounter velocity). D56: the sampled surface
		// can jump from one frame to the next where steep storm crests fold (the search for the water under a point lands
		// elsewhere); such a jump is not water moving, and through the heave damping it kicked the whole boat (the view
		// "bugging up and down" in big waves). Bounded to what a sea moves (2.5 m/s) and eased in (WaterVelocitySmoothing).
		if (C.bHasWater && DeltaSeconds > 0.f)
		{
			Z = FMath::Lerp(Z, C.WaterZ, KeepZ);
			const double Raw = FMath::Clamp((Z - C.WaterZ) / DeltaSeconds, -2.5, 2.5);
			C.WaterVz += (Raw - C.WaterVz) * (1.0 - KeepVz);
		}
		else
		{
			C.WaterVz = 0.0;
		}
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
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(SaltHull::HalfLength + 0.8, SaltHull::HalfBeam + 0.8, 3.3) * 100.0);
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
	// D74: the water in her bilge weighs her down.
	FVector F(0.0, 0.0, -(Mass + BilgeWaterKg) * Gravity);
	FVector T = FVector::ZeroVector;
	KickBuoyancy = KickDamping = 0.0;
	auto AddForceAt = [&](const FVector& Force, const FVector& At)
	{
		F += Force;
		T += FVector::CrossProduct(At - X, Force);
	};
	// D56: forces high on the rig (the sail's drive, the wind on hull and rig) act on the centreline, so on an upright
	// boat they do not turn it. When the waves heel it, that point swings out to the side and the same forces twisted
	// the boat round (it broached and circled in big waves). Their twist about the vertical is left out: the waves heel
	// and pitch the boat but do not steer it (owner).
	auto AddForceAtNoTurn = [&](const FVector& Force, const FVector& At)
	{
		F += Force;
		FVector Twist = FVector::CrossProduct(At - X, Force);
		if (CVarRigTurn.GetValueOnGameThread() == 0)
		{
			Twist.Z = 0.0;
		}
		T += Twist;
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
		KickBuoyancy += RhoWater * Gravity * Vol;
		KickDamping += -Damp * VRel;
	}
	Immersion = FMath::Clamp((float)(Submerged / DesignVolume), 0.f, 1.f);
	const double Wet = Immersion;
	// D65: heave damping against the boat's own vertical speed (calms the short, quick heave; the swell still lifts it).
	AddForceAt(FVector(0.0, 0.0, -HeaveDamping * V.Z * Wet), X);
	KickDamping += -HeaveDamping * V.Z * Wet;

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

	// Sail (D65, owner: "always pulls"): the square sail, fixed square, drives the boat along its heading whatever the wind's
	// angle: the whole SailThrust with the apparent wind astern, SailDriveAbeam of it with the wind abeam and never less than
	// SailDriveHeadToWind (head to wind: she is never stuck), on a smooth curve between. The wind's speed shades it a little
	// (WindInfluence: a fresher breeze is a bit faster). Its sideways share heels the boat to leeward as before D65 (the full
	// sail's force on the beam). Both act on the centreline at the sail's centre of effort (heel and pitch, no twist: D56).
	const FVector AirWorld(WindWorld.X, WindWorld.Y, 0.0);
	const FVector CE(YardX, 0.f, SheerZ(MastX) + SailCEHeight);
	const FVector Apparent = Inv.RotateVector(AirWorld - PointVelocity(BodyToWorld(CE)));
	LastApparentWind = FVector2D(Apparent.X, Apparent.Y);
	SailDrive = 0.f;
	SailDriveFactor = 0.f;
	if (Hoist > 0.01f)
	{
		const double AppSpeed = FMath::Max(LastApparentWind.Size(), 0.1);
		// 0 = the apparent wind from dead ahead .. 1 = from dead astern (the air moving forward over the boat).
		const double Astern = FMath::Clamp(0.5 * (1.0 + Apparent.X / AppSpeed), 0.0, 1.0);
		const double Floor = FMath::Clamp((double)SailDriveHeadToWind, 0.0, 0.95);
		const double Beam = FMath::Clamp((double)SailDriveAbeam, Floor + 0.01, 0.99);
		const double Shape = FMath::Loge((Beam - Floor) / (1.0 - Floor)) / FMath::Loge(0.5);
		const double AngleFactor = Floor + (1.0 - Floor) * FMath::Pow(Astern, Shape);
		const double SpeedFactor = FMath::Clamp(AirWorld.Size() / 7.0, 0.3, 1.5);
		const double Shade = FMath::Lerp(1.0, SpeedFactor, (double)WindInfluence) * (1.0 - AnchorHold);
		// D74: a torn sail drives less, and a boat low in the water drags (SailCondition, from the hull damage).
		const double Fx = SailThrust * Hoist * AngleFactor * Shade * SailCondition;
		const double Fy = SailThrust * Hoist * Shade * SailHeel * SailCondition * FMath::Clamp(Apparent.Y / AppSpeed, -1.0, 1.0);
		AddForceAtNoTurn(Q.RotateVector(FVector(Fx, Fy, 0.0)), BodyToWorld(CE));
		SailDrive = (float)Fx;
		SailDriveFactor = (float)AngleFactor;
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
		AddForceAtNoTurn(AirRel * (0.5 * RhoAir * Area * AirSpeed), WindagePoint);
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
	KickTotal = F.Z;
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
	// D59 (owner: "a way to control the sail more, so you can control the speed"): at the helm W lets the sail out and S
	// takes it in, smoothly from nothing to full in TrimTime; the pin rails set it fully or take it in (E). The sail
	// follows its target at the set rate, and the drive goes with how much of it is set.
	if (!bOverride && FMath::Abs(In.Y) > 0.2f)
	{
		SetSailAmount(SailTarget + FMath::Sign(In.Y) * DeltaSeconds / FMath::Max(TrimTime, 0.1f));
	}
	Hoist = FMath::FInterpConstantTo(Hoist, SailTarget, DeltaSeconds, 1.f / FMath::Max(SetTime, 0.1f));

	// D65: the square sail stays square. With the apparent wind well forward of the beam it cannot fill and shivers (Luff);
	// the cloth leans to leeward with the wind's sideways share (SailLean, the material's Lean).
	float LeanTarget = 0.f;
	const double AppSpeed = LastApparentWind.Size();
	if (Hoist > 0.05f && AppSpeed > 0.3)
	{
		const float FromDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)(-LastApparentWind.X / AppSpeed), -1.f, 1.f)));
		Luff = 1.f - FMath::SmoothStep(20.f, 75.f, FromDeg);
		LeanTarget = FMath::Clamp((float)(LastApparentWind.Y / AppSpeed), -1.f, 1.f) * FMath::SmoothStep(0.3f, 3.f, (float)AppSpeed);
	}
	else
	{
		Luff = 0.f;
	}
	SailLean = FMath::FInterpTo(SailLean, LeanTarget, DeltaSeconds, 1.5f);

	// Belly fill: a spring with little damping, so the sail pops full when it catches the wind and breathes with
	// the gusts (the wind speed against its own slow average). D59 (owner: "too little reaction in the sail"): how full
	// follows the apparent wind's strength (a slack belly in light air, a deep one in a fresh breeze, SailPress), the
	// gusts count more and the spring is livelier; a part-set sail fills less.
	const float WindNow = (float)LastTrueWind.Size();
	AvgWind = AvgWind <= 0.f ? WindNow : FMath::Lerp(AvgWind, WindNow, FMath::Min(DeltaSeconds / 8.f, 1.f));
	const float Gust = AvgWind > 0.1f ? FMath::Clamp(WindNow / AvgWind, 0.7f, 1.35f) : 1.f;
	SailPress = FMath::SmoothStep(1.f, 9.f, (float)AppSpeed);
	const float Set = FMath::SmoothStep(0.1f, 0.7f, Hoist);
	const float FillTarget = Set * FMath::Lerp(1.f, 0.3f, Luff) * (0.55f + 0.6f * SailPress) * Gust + (1.f - Set) * 0.12f * Hoist;
	const float Omega = 2.f * PI * 1.6f;
	FillVel += (Omega * Omega * (FillTarget - Fill) - 2.f * 0.2f * Omega * FillVel) * DeltaSeconds;
	Fill += FillVel * DeltaSeconds;
}

void ASaltBoat::UpdateVisuals(float DeltaSeconds)
{
	using namespace SaltHull;
	// D65: the sail material moves the cloth (M_SB_SquareSail): it hangs down from the yard as far as it is set, the rest
	// rolled up under it; the belly fills forward (its cut and, D59, the wind pressing it out deeper as it freshens, never
	// as far as the forestay), leans to leeward, shivers when the wind comes from ahead and always has a little life.
	SailBellyShown = FMath::Min(FMath::Max(Fill, 0.f) * (SailCutBelly + SailBelly * FMath::Lerp(0.35f, 1.f, SailPress)) * (1.f - 0.5f * Luff),
		SailMaxBelly * 100.f);
	for (UMaterialInstanceDynamic* Mid : SailMaterials)
	{
		Mid->SetScalarParameterValue(TEXT("Set"), Hoist);
		Mid->SetScalarParameterValue(TEXT("Fill"), FMath::Max(Fill, 0.f));
		Mid->SetScalarParameterValue(TEXT("Belly"), SailBellyShown);
		Mid->SetScalarParameterValue(TEXT("Lean"), SailLean);
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
	FSaltMooring Near;
	const bool bNearMooring = FindMooring(Near);
	FText WindlassText;
	switch (Hold)
	{
	case ESaltBoatHold::Moored: WindlassText = NSLOCTEXT("Saltbound", "BoatCastOff", "Cast off"); break;
	case ESaltBoatHold::Anchored: WindlassText = NSLOCTEXT("Saltbound", "BoatRaiseAnchor", "Hold to raise the anchor"); break;
	default:
		WindlassText = Speed < 1.5 && bNearMooring ? NSLOCTEXT("Saltbound", "BoatMoor", "Moor the boat")
			: NSLOCTEXT("Saltbound", "BoatDropAnchor", "Drop the anchor");
		break;
	}
	Windlass->PromptText = WindlassText;
	// D55: raising the anchor is a hold (the owner's choice); everything else is a short press.
	Windlass->HoldTime = Hold == ESaltBoatHold::Anchored ? 0.8f : 0.f;
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
	const FString SailState = SailTarget <= 0.f && Hoist < 0.02f ? FString(TEXT("sail up")) : FString::Printf(TEXT("sail %d %%"), FMath::RoundToInt(SailTarget * 100.f));
	// D58: the controls in the key-scrap prompt (W and S the sail, D59; A and D steer), the boat's state on a line under it.
	Helm->Controls = { { { TEXT("W"), TEXT("S") }, NSLOCTEXT("Saltbound", "BoatTrimSail", "Sail") },
		{ { TEXT("A"), TEXT("D") }, NSLOCTEXT("Saltbound", "BoatSteer", "Steer") } };
	const FString Status = FString::Printf(TEXT("%.1f kn  ·  %s  ·  %s%s"), Speed * MsToKnots, *SailState, *WindText,
		Hold == ESaltBoatHold::Moored ? TEXT("  ·  moored") : (Hold == ESaltBoatHold::Anchored ? TEXT("  ·  at anchor") : TEXT("")));
	Helm->ControlsHintText = FText::FromString(Status);
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
	// D62: to the mooring in use (the pier, or a quay of the hub to starboard or port).
	const FVector Reach = Mooring.Pose.GetRotation().RotateVector(FVector(0.0, Mooring.Reach, 0.0));
	const FVector PierZ(0.0, 0.0, Mooring.Pose.GetLocation().Z + Mooring.PierHeight + 4.0);
	const double Side = Mooring.Side();
	auto PierPoint = [&](const FVector& BittBody)
	{
		FVector P = Mooring.Pose.TransformPosition(BittBody * 100.0) + Reach;
		P.Z = PierZ.Z;
		return P;
	};
	MooringPierFore->SetWorldLocation(PierPoint(FVector(4.0, 0.22 * Side, 0.0)));
	MooringPierAft->SetWorldLocation(PierPoint(FVector(-4.15, 0.22 * Side, 0.0)));
	const bool bMoored = Hold == ESaltBoatHold::Moored;
	Fit(MooringLineFore, MooringPierFore, 1.05f, bMoored);
	Fit(MooringLineAft, MooringPierAft, 1.05f, bMoored);
	// Anchor rode from the cathead down to the anchor.
	Fit(AnchorLine, Anchor, 1.08f, AnchorAlpha > 0.01f);
	// D65: the sheets go from the pin rails to the clews (a little slack), the brails hang taut from their blocks down to the
	// foot; their sail ends follow the cloth as it is let fall or rolled up (the material's own sums).
	for (const bool bStarboard : { false, true })
	{
		USceneComponent* Clew = GetClew(bStarboard);
		USceneComponent* Foot = GetBrailFoot(bStarboard);
		UCableComponent* Brail = GetBrail(bStarboard);
		if (!Clew || !Foot || !Brail)
		{
			continue;
		}
		Clew->SetRelativeLocation(SailClewLocal(bStarboard));
		Foot->SetRelativeLocation(SailBrailFootLocal(bStarboard));
		Fit(GetSheet(bStarboard), Clew, 1.03f, true);
		Fit(Brail, Foot, 1.f, true);
		Brail->CableLength = (float)FVector::Dist(Brail->GetComponentLocation(), Foot->GetComponentLocation()) + 1.f;
	}
}

float ASaltBoat::SailCut() const
{
	using namespace SaltHull;
	return FMath::Max(FMath::Clamp(Hoist, 0.f, 1.f) * SailDrop, SailCutMin);
}

FVector ASaltBoat::SailClewLocal(bool bStarboard) const
{
	// M_SB_SquareSail (setup_boat_sail.py): the leeches never belly, so a clew lies on the leech's line; once the sail is
	// rolled up from the foot, the clew is in the roll, whose end's middle lies Rout forward of the cut.
	using namespace SaltHull;
	const float Cut = SailCut();
	const float Rout = FMath::Sqrt(SailRollCore * SailRollCore + SailRollLayer * (SailDrop - Cut) / PI);
	const float Half = FMath::Lerp(SailHalfHead, SailHalfFoot, Cut / SailDrop);
	return FVector(Rout, (bStarboard ? 1.f : -1.f) * Half, SailHeadZ - Cut) * 100.f;
}

FVector ASaltBoat::SailBrailFootLocal(bool bStarboard) const
{
	// M_SB_SquareSail at the cut (the foot when it is set), under the brail's block: the belly there (Across, Hang, Breath)
	// and the lean to leeward, on the cloth's aft face, where the brail goes round under the roll.
	using namespace SaltHull;
	const float Cut = SailCut();
	const float Half = FMath::Lerp(SailHalfHead, SailHalfFoot, Cut / SailDrop);
	const float Y = (bStarboard ? 1.f : -1.f) * BrailAcross * Half;
	const float U = 0.5f + Y / (2.f * Half);
	const float Up = 0.5f + 0.18f * SailLean;
	const float Ua = U < Up ? 0.5f * U / Up : 0.5f + 0.5f * (U - Up) / (1.f - Up);
	const float ZhCm = (SailHeadZ - Cut) * 100.f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;   // the material's Time
	const float Breath = FMath::Sin(Now * 1.6f + ZhCm * 0.012f) * 0.08f * FMath::Clamp(Fill, 0.f, 1.f);
	const float DepthCm = SailBellyShown * FMath::Sqrt(Cut / SailDrop) * FMath::Sin(PI * Ua) * (1.f + Breath);
	return FVector(DepthCm * FMath::Sin(2.4f) - 2.f, Y * 100.f + SailLean * 0.3f * DepthCm * FMath::Sin(2.83f), ZhCm);
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
	const float Side = Mooring.Side();
	FVector Pier = Mooring.Pose.TransformPosition(FVector(LadderX * 100.f, Mooring.Reach + 15.f * Side, 0.f));
	Pier.Z = Mooring.Pose.GetLocation().Z + Mooring.PierHeight;
	const FVector OnBoat = Hull->GetComponentTransform().TransformPosition(FVector(LadderX, Side * (HalfBeamAtSheer(LadderX) - 0.2f), SheerZ(LadderX) + 0.03f) * 100.0);
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
		TEXT("rudder=%.1f drive=%.0fN driveShare=%.2f lean=%.2f fill=%.2f sail=%.2f hold=%s immersion=%.2f waterline=%.1fcm contacts=%d depth=%.1fcm ")
		TEXT("twa=%.1f tws=%.2f aws=%.2f mass=%.0f"),
		L.X, L.Y, L.Z, Yaw, CourseYaw, GetSpeedKnots(), Vb.X, Vb.Y, FMath::RadiansToDegrees(W.Z), GetHeelDeg(), GetPitchDeg(),
		RudderAngle, SailDrive, SailDriveFactor, SailLean, Fill, Hoist, HoldName, Immersion, GetWaterlineOffset(), Contacts.Num(), GetContactDepth(),
		FRotator::NormalizeAxis(FromYaw - Yaw), LastTrueWind.Size(), LastApparentWind.Size(), Mass);
}

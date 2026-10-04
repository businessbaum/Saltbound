// Saltbound - the sailing boat: a kinematic rigid body moved by our own integrator (D7, D36).

#pragma once

#include "CoreMinimal.h"
#include "Core/SaltKinematicBase.h"
#include "GameFramework/Actor.h"
#include "SaltBoat.generated.h"

class UCableComponent;
class UMaterialParameterCollection;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USaltInteractableComponent;
class USaltStationComponent;
class ASaltPlayerCharacter;

/** What holds the boat in place. */
UENUM(BlueprintType)
enum class ESaltBoatHold : uint8
{
	Free,      // sailing or drifting
	Moored,    // bow and stern lines to the mooring (its start position)
	Anchored   // one rode from the bow; the boat swings to the wind
};

/**
 * The starter boat (greybox until the art decisions, D13). Kinematic like the M2 test deck: the transform is set
 * by code in TG_PrePhysics and the player walks on it with CharacterMovement's based movement (D7).
 *
 * Physics (SI units inside, fixed substeps):
 * - Buoyancy: about 70 vertical hull columns from the hull shape (SaltHullShape.h), each pushed up by the water it
 *   displaces at the CPU wave surface (USaltOceanSubsystem), damped against the water's vertical motion. Heave,
 *   pitch, roll and the righting moment (heel) follow from the shape; mass = displacement at the design waterline.
 * - Hull: forward resistance rising steeply past hull speed; the keel as a low-aspect wing (lateral resistance,
 *   leeway); crossflow drag along the hull; yaw damping.
 * - Rudder: a foil at the stern (no steerage without speed, plus a little "sculling" at rest).
 * - Sail (simple, D41): set, it drives the boat forward (the wind only shades the force a little and heels the boat
 *   to leeward); taken in, the boat slows down. The boom trims itself to the wind. Later engines will drive the
 *   same way.
 * - Lines: mooring (bow and stern) or anchor (bow), horizontal springs with slack.
 * - Collision: capsules along the hull (two rings and the keel) against static geometry (pier, rocks, seabed):
 *   contact planes found once per frame push back as stiff damped springs with friction inside the substeps, and
 *   a sweep stops the boat from tunnelling through thin objects.
 * - Recovery: a capsized boat returns upright to its last safe position; sb.Boat.Reset returns to the mooring.
 *
 * Controls (D41): the wheel only steers (A / D). The sail is let down (set) or raised (taken in) at the ropes by
 * the shrouds, either side; the windlass on the foredeck casts off, moors (near the pier) or drops and raises the
 * anchor. Sail, sheet, anchor rode and mooring lines are drawn as physics ropes.
 */
UCLASS()
class SALTBOUND_API ASaltBoat : public AActor, public ISaltKinematicBase
{
	GENERATED_BODY()

public:
	ASaltBoat();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	//~ ISaltKinematicBase
	virtual FVector GetKinematicAngularVelocity() const override { return AngVel; }
	virtual FVector GetKinematicPivot() const override { return GetActorLocation(); }

	// Controls -----------------------------------------------------------------
	/** Set the sail (let it down: the boat sails) or take it in (raised to the gaff: the boat slows down). */
	void SetSailSet(bool bSet);
	void ToggleSail() { SetSailSet(!bSailSet); }
	/** Cast off when held; otherwise moor (near the mooring) or drop the anchor. */
	void ToggleHold();
	void SetHold(ESaltBoatHold NewHold);
	/** Development and tests: fixed rudder (-1..1, + = starboard), ignoring the helm. */
	void SetControlOverride(bool bEnable, float Rudder01 = 0.f);
	/** Development and tests (with the override): the rudder holds a heading (degrees). */
	void SetAutopilot(bool bEnable, float HeadingYaw = 0.f) { bAutopilot = bEnable; AutopilotYaw = HeadingYaw; }
	/** Development and tests: add velocity (m/s, world) and angular velocity (rad/s, world). */
	void DebugKick(const FVector& Linear, const FVector& Angular) { V += Linear; W += Angular; }
	/** Back to the mooring (start pose), sail lowered, moored. */
	void ResetToStart();
	/** Upright at the last safe position, at rest. */
	void Recover();
	/** Development and tests: place the boat (origin, cm; heading in degrees), optionally with a forward speed (cm/s). */
	void PlaceAt(const FVector& Location, float Yaw, float ForwardSpeed = 0.f);

	// State --------------------------------------------------------------------
	bool IsSailSet() const { return bSailSet; }
	/** How far the sail is set (0 taken in .. 1 set), animated. */
	float GetSailAmount() const { return Hoist; }
	ESaltBoatHold GetHold() const { return Hold; }
	float GetSpeedKnots() const;
	/** Velocity of the origin, cm/s. */
	FVector GetVelocity() const override;
	float GetHeelDeg() const;
	float GetPitchDeg() const;
	float GetYawDeg() const { return (float)Q.Rotator().Yaw; }
	/** Direction of travel over the ground (degrees), the heading when at rest. */
	float GetCourseDeg() const { return V.Size2D() > 0.05 ? FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) : GetYawDeg(); }
	float GetYawRateDeg() const { return FMath::RadiansToDegrees((float)W.Z); }
	float GetMass() const { return Mass; }
	float GetRudderAngleDeg() const { return RudderAngle; }
	float GetBoomAngleDeg() const { return BoomAngle; }
	/** Deepest penetration of the collision capsules into static geometry this frame (cm). */
	float GetContactDepth() const { return MaxContactDepth * 100.f; }
	int32 GetContactCount() const { return Contacts.Num(); }
	/** Height of the origin above the water surface at the origin, cm (0 = floating at the design waterline). */
	float GetWaterlineOffset() const;
	USaltStationComponent* GetHelm() const { return Helm; }
	USaltInteractableComponent* GetSailRope(bool bStarboard) const { return bStarboard ? SailRopeStarboard : SailRopePort; }
	USaltInteractableComponent* GetWindlass() const { return Windlass; }
	/** One line for logs and tests (SB_BOAT). */
	FString Describe() const;

	// Components ---------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Hull;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> BoomPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Boom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Sail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> GaffPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Gaff;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> RudderPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Rudder;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Wheel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltStationComponent> Helm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> SailRopePort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> SailRopeStarboard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> Windlass;
	/** The anchor: hangs at the cathead, goes down to the seabed when dropped. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Anchor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> Cathead;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> AnchorLine;
	/** Mainsheet from the boom to a block on deck (slack when the sail is taken in). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> BoomSheetPoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> SheetBlock;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> MainSheet;
	/** Mooring lines from the bitts to the pier while moored (their pier ends stay fixed in the world). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> MooringPierFore;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> MooringPierAft;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> MooringLineFore;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> MooringLineAft;

	// Tunables -------------------------------------------------------------------
	/** Centre of mass height above the design waterline (lower = stiffer, less heel). */
	UPROPERTY(EditAnywhere, Category = "Boat|Mass", meta = (Units = "m")) float CentreOfMassHeight = 0.15f;
	/** Radii of gyration for roll, pitch, yaw. */
	UPROPERTY(EditAnywhere, Category = "Boat|Mass", meta = (Units = "m")) FVector Gyration = FVector(1.35, 2.6, 2.7);
	/** Added mass of the water moving with the hull: heave, roll, pitch (fractions of the mass / inertia). */
	UPROPERTY(EditAnywhere, Category = "Boat|Mass") FVector AddedMass = FVector(0.6, 0.25, 0.6);

	/** Vertical damping of the hull columns (N s/m per m2 of column area). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float VerticalDamping = 1400.f;
	/** Forward resistance at hull speed (N), and hull speed (m/s; about 7.3 kn for 9 m on the waterline). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float HullSpeedResistance = 1700.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro", meta = (Units = "m/s")) float HullSpeed = 3.75f;
	/** Keel as a wing: lift slope (per radian) and the angle where it stalls. */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float KeelLiftSlope = 1.3f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro", meta = (Units = "deg")) float KeelStallDeg = 14.f;
	/** Crossflow drag coefficient of the hull's lateral plane. */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float CrossflowDrag = 0.9f;
	/** Yaw damping: constant part (N m s) and part per m/s of speed. */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float YawDamping = 9000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float YawDampingPerSpeed = 16000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float RollDamping = 25000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float PitchDamping = 20000.f;

	/** Rudder blade area (m2). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RudderArea = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder", meta = (Units = "deg")) float MaxRudderAngle = 35.f;
	/** How fast the helm turns the rudder, and how fast it returns to the centre when let go (deg/s). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RudderRate = 50.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RudderReturnRate = 30.f;
	/** Yaw torque at full rudder when at rest ("sculling"), fading out by 1.5 m/s (N m). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RestTurnTorque = 1500.f;
	/** Wheel turns per rudder turn (visual). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float WheelRatio = 5.f;

	/** Forward force of the set sail in a 7 m/s breeze (N): about 7.5 kn at full sail, reached in about 20 s. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float SailThrust = 7800.f;
	/** Extra forward drag per m/s (N s/m), so the boat slows down within about 15 s once the sail is taken in
	 *  (simple, arcade-friendly handling; engines will drive the same way). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float CoastDrag = 1500.f;
	/** Sideways share of the sail force (heels the boat to leeward). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float SailHeel = 0.22f;
	/** How strongly the wind shades the drive: 0 = always the same force, 1 = force with wind speed and angle. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float WindInfluence = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "deg/s")) float BoomSwingRate = 70.f;
	/** Time to let the sail down or raise it. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "s")) float SetTime = 2.5f;
	/** Hull and rig drag in the wind (sail lowered): side and frontal area (m2). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") FVector2D WindageArea = FVector2D(2.5, 7.0);

	/** Contact springs against static geometry: stiffness (N/m), damping (N s/m), friction. */
	UPROPERTY(EditAnywhere, Category = "Boat|Collision") float ContactStiffness = 3.0e6f;
	UPROPERTY(EditAnywhere, Category = "Boat|Collision") float ContactDamping = 1.8e5f;
	UPROPERTY(EditAnywhere, Category = "Boat|Collision") float ContactFriction = 0.35f;

	/** Mooring and anchor lines: stiffness (N/m), damping (N s/m), anchor rode slack (m). */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines") float LineStiffness = 6000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines") float LineDamping = 6000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "m")) float AnchorRode = 14.f;
	/** Mooring is possible within this distance of the start position. */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "m")) float MooringRadius = 8.f;
	/** Where the mooring lines reach the pier: this far to starboard (+) or port (-) of the bitts at the start. */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "cm")) float MooringReach = 240.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "s")) float AnchorDropTime = 3.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "s")) float AnchorRaiseTime = 4.f;

	/** Initial state when play starts. */
	UPROPERTY(EditAnywhere, Category = "Boat") ESaltBoatHold StartHold = ESaltBoatHold::Moored;

	/** The sea is cut out inside this boat's hull (MPC_Ocean Boat*, D42): no waves inside the boat or on its deck. */
	UPROPERTY(EditAnywhere, Category = "Boat") bool bMasksOcean = true;

private:
	struct FColumn
	{
		FVector Bottom = FVector::ZeroVector;   // body, m
		float Height = 0.f;                     // m, along body Z
		float Area = 0.f;                       // m2
		double WaterZ = 0.0;                    // world, m (sampled once per frame)
		double WaterVz = 0.0;                   // m/s
		bool bHasWater = false;
	};
	struct FSegment
	{
		FVector Point = FVector::ZeroVector;    // body, m: centre of the lateral area
		float Area = 0.f;                       // m2 lateral area
	};
	struct FCapsule
	{
		FVector A = FVector::ZeroVector;        // body, m
		FVector B = FVector::ZeroVector;
		float Radius = 0.f;
	};
	struct FContact
	{
		FVector BodyPoint = FVector::ZeroVector; // body, m
		FVector PlanePoint = FVector::ZeroVector; // world, m
		FVector Normal = FVector::UpVector;
	};
	struct FLine
	{
		FVector BodyPoint = FVector::ZeroVector;
		FVector Fixed = FVector::ZeroVector;    // world, m
		float Slack = 0.f;
	};

	void ResolveContent();
	void BuildHullModel();
	void SampleWater(float DeltaSeconds);
	void FindContacts();
	void Step(float H, const FVector2D& WindWorld);
	void PreventTunnelling(const FVector& OldX, const FQuat& OldQ);
	void ApplyTransform(float DeltaSeconds);
	void UpdateControls(float DeltaSeconds);
	void UpdateVisuals(float DeltaSeconds);
	void UpdateHints();
	void UpdateRecovery(float DeltaSeconds);
	void SetPose(const FVector& OriginWorldM, const FQuat& Rotation);
	void ClearMotion();
	void MoorAtStart();

	void PushOceanMask(bool bEnable);
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> OceanCollection;

	UFUNCTION() void HandleSailRope(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	UFUNCTION() void HandleWindlass(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	void UpdateRopes(float DeltaSeconds);
	void UpdateAnchor(float DeltaSeconds);
	void SetupSailMaterials();

	FVector BodyToWorld(const FVector& Body) const { return X + Q.RotateVector(Body - CoM); }
	FVector PointVelocity(const FVector& World) const { return V + FVector::CrossProduct(W, World - X); }

	// Hull model (body space, SI).
	TArray<FColumn> Columns;
	TArray<FSegment> Segments;
	TArray<FCapsule> Capsules;
	FVector CoM = FVector::ZeroVector;
	float Mass = 1.f;
	float DesignVolume = 1.f;
	FVector KeelPoint = FVector::ZeroVector;
	float KeelArea = 0.f;
	/** Sail centre of effort: distance aft along the boom and height above it (set). */
	float SailCEAft = 2.f;
	float SailCEUp = 2.5f;

	// Rigid body state (world, SI): centre of mass, orientation, velocities.
	FVector X = FVector::ZeroVector;
	FQuat Q = FQuat::Identity;
	FVector V = FVector::ZeroVector;
	FVector W = FVector::ZeroVector;
	float Immersion = 1.f;
	FVector AngVel = FVector::ZeroVector;    // reported to CharacterMovement (rad/s)

	// Controls and rig.
	float RudderAngle = 0.f;                 // deg, + = bear away to starboard
	bool bSailSet = false;
	float Hoist = 0.f;                       // 0 taken in .. 1 set
	float BoomAngle = 0.f;                   // deg, + = boom to starboard
	float SailSide = 1.f;                    // leeward side of the belly (visual)
	float SailSideVisual = 1.f;
	float Fill = 0.f;                        // belly fill (spring: pops when the sail catches the wind)
	float FillVel = 0.f;
	float Luff = 0.f;                        // 0..1, wind from ahead: the sail shivers
	float AvgWind = 0.f;                     // slow average of the wind speed (gusts against it)
	float SailDrive = 0.f;                   // N, last forward sail force (logs)
	bool bOverride = false;
	float OverrideRudder = 0.f;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> SailMaterials;
	// Anchor: 0 stowed at the cathead .. 1 on the seabed.
	float AnchorAlpha = 0.f;
	FVector AnchorGround = FVector::ZeroVector;   // world, cm
	bool bAutopilot = false;
	float AutopilotYaw = 0.f;
	FVector2D LastApparentWind = FVector2D::ZeroVector;   // body XY, m/s
	FVector2D LastTrueWind = FVector2D::ZeroVector;       // world XY, m/s

	ESaltBoatHold Hold = ESaltBoatHold::Free;
	TArray<FLine> Lines;
	TArray<FContact> Contacts;
	float MaxContactDepth = 0.f;
	/** Static geometry within reach this frame (broad phase); otherwise no contact or sweep queries run. */
	bool bNearStatic = false;

	FTransform StartTransform;
	struct FSafePose { FVector Origin; float Yaw; };
	TArray<FSafePose> SafePoses;
	float SafePoseTimer = 0.f;
	float CapsizeTimer = 0.f;
	double Time = 0.0;
	bool bInitialised = false;
};

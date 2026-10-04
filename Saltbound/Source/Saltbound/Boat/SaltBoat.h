// Saltbound - the sailing boat: a kinematic rigid body moved by our own integrator (D7, D36).

#pragma once

#include "CoreMinimal.h"
#include "Core/SaltKinematicBase.h"
#include "GameFramework/Actor.h"
#include "SaltBoat.generated.h"

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
 * - Sail: a gaff main as a foil in the apparent wind; the boom swings free up to the sheet's limit (luffing head to
 *   wind gives the no-go zone, gybes swing the boom across). The heeling moment comes from the sail's height.
 * - Lines: mooring (bow and stern) or anchor (bow), horizontal springs with slack.
 * - Collision: capsules along the hull (two rings and the keel) against static geometry (pier, rocks, seabed):
 *   contact planes found once per frame push back as stiff damped springs with friction inside the substeps, and
 *   a sweep stops the boat from tunnelling through thin objects.
 * - Recovery: a capsized boat returns upright to its last safe position; sb.Boat.Reset returns to the mooring.
 *
 * Helm: A / D steer, W / S sheet in / out, Space raises or lowers the sail, C casts off / moors / anchors.
 */
UCLASS()
class SALTBOUND_API ASaltBoat : public AActor, public ISaltKinematicBase
{
	GENERATED_BODY()

public:
	ASaltBoat();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//~ ISaltKinematicBase
	virtual FVector GetKinematicAngularVelocity() const override { return AngVel; }
	virtual FVector GetKinematicPivot() const override { return GetActorLocation(); }

	// Controls -----------------------------------------------------------------
	void SetSailRaised(bool bRaised);
	void ToggleSail() { SetSailRaised(!bSailRaised); }
	/** Cast off when held; otherwise moor (near the mooring) or anchor (when slow). */
	void ToggleHold();
	void SetHold(ESaltBoatHold NewHold);
	/** Development and tests: fixed rudder (-1..1, + = starboard) and sheet (0 hauled in .. 1 eased), ignoring the helm. */
	void SetControlOverride(bool bEnable, float Rudder01 = 0.f, float Sheet01 = 0.5f);
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
	bool IsSailRaised() const { return bSailRaised; }
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
	float GetSheet() const { return Sheet; }
	float GetBoomAngleDeg() const { return BoomAngle; }
	/** Deepest penetration of the collision capsules into static geometry this frame (cm). */
	float GetContactDepth() const { return MaxContactDepth * 100.f; }
	int32 GetContactCount() const { return Contacts.Num(); }
	/** Height of the origin above the water surface at the origin, cm (0 = floating at the design waterline). */
	float GetWaterlineOffset() const;
	USaltStationComponent* GetHelm() const { return Helm; }
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> Halyard;

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
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float RollDamping = 6000.f;
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

	/** Sail area (m2). The greybox draws only the gaff main (27 m2); 40 m2 stands for the whole rig of a boat this
	 *  heavy (main and a jib), which the final art will carry. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float SailArea = 40.f;
	/** Boom angle limit with the sheet hauled in and fully eased. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "deg")) float SheetMinAngle = 7.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "deg")) float SheetMaxAngle = 85.f;
	/** Sheet change per second at full W / S input (0..1 of the range). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float SheetRate = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "deg/s")) float BoomSwingRate = 110.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "s")) float HoistTime = 3.f;
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

	/** Initial state when play starts. */
	UPROPERTY(EditAnywhere, Category = "Boat") ESaltBoatHold StartHold = ESaltBoatHold::Moored;

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

	UFUNCTION() void HandleHelmAction(USaltStationComponent* Station, int32 Action);
	UFUNCTION() void HandleHalyard(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);

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
	/** Sail centre of effort: distance aft along the boom and height above it (fully hoisted). */
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
	float Sheet = 0.5f;
	bool bSailRaised = false;
	float Hoist = 0.f;
	float BoomAngle = 0.f;                   // deg, + = boom to starboard
	float SailAlpha = 0.f;                   // deg, angle of attack (visual luffing)
	float SailSide = 1.f;                    // leeward side of the camber (visual)
	float SailSideVisual = 1.f;
	bool bOverride = false;
	float OverrideRudder = 0.f;
	float OverrideSheet = 0.5f;
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

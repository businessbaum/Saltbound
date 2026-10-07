// Saltbound - the sailing boat: a kinematic rigid body moved by our own integrator (D7, D36).

#pragma once

#include "CoreMinimal.h"
#include "Boat/SaltMooringPoint.h"
#include "Core/SaltKinematicBase.h"
#include "GameFramework/Actor.h"
#include "SaltBoat.generated.h"

class UCableComponent;
struct FSaltLadderLine;
class UPointLightComponent;
class USaltSwingComponent;
class UMaterialParameterCollection;
class UMaterialInstanceDynamic;
class USaltInventoryComponent;
class UStaticMeshComponent;
class USaltInteractableComponent;
class USaltStationComponent;
class USaltHullDamageComponent;
class ASaltPlayerCharacter;

/** What holds the boat in place. */
UENUM(BlueprintType)
enum class ESaltSteering : uint8
{
	SteeringOar,   // a big oar on the starboard quarter, turned by a handle (the first boat, D45)
	Wheel          // sternpost rudder and ship's wheel (D36-D43, kept for later boats)
};

UENUM(BlueprintType)
enum class ESaltBoatHold : uint8
{
	Free,      // sailing or drifting
	Moored,    // bow and stern lines to a mooring (its start position, the pier, or a mooring point: D62)
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
 * - Sail (D65, "always pulls"): a square sail on a yard fixed square to the boat. Set, it drives the boat along its
 *   heading whatever the wind's angle: strongest with the wind astern, weaker with it abeam, never stuck head to wind;
 *   it heels the boat to leeward; taken in, the boat slows down. Later engines will drive the same way.
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
	/** Set the sail (let it fall: the boat sails) or take it in (rolled up under the yard: the boat slows down). */
	void SetSailSet(bool bSet);
	void ToggleSail() { SetSailSet(!bSailSet); }
	/** FH1 (D77): the prologue keeps her ripped sail furled until it has been mended once; Why is what the rope says then. */
	void SetSailLocked(bool bLocked, const FText& Why = FText()) { bSailLocked = bLocked; SailLockedText = Why; }
	bool IsSailLocked() const { return bSailLocked; }
	/** D59: how much sail to set, 0 (taken in) .. 1 (full): W / S at the helm move it; the sail follows at its set rate. */
	void SetSailAmount(float Amount);
	float GetSailTarget() const { return SailTarget; }
	/** Cast off when held; otherwise moor (near the mooring) or drop the anchor. */
	void ToggleHold();
	void SetHold(ESaltBoatHold NewHold);
	/** Development and tests: fixed rudder (-1..1, + = starboard), ignoring the helm. */
	void SetControlOverride(bool bEnable, float Rudder01 = 0.f);
	/** D70: a cutscene sails her (the hub's harbour passages): her own simulation rests and SetScriptedPose places her each
	 *  frame (her origin, cm, on the water there; her heading), her velocity following from the moves (wake, ropes, the player
	 *  standing on her). Enabling it casts her off. */
	void SetScripted(bool bEnable);
	bool IsScripted() const { return bScripted; }
	void SetScriptedPose(const FVector& LocationCm, float Yaw, float DeltaSeconds);
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
	/** A saved state (D46): moored boats lie at their mooring (D62: InMooring, its home when invalid), anchored ones with the
	 *  anchor down and holding. */
	void RestoreState(const FVector& Location, float Yaw, ESaltBoatHold InHold, bool bSail, const FSaltMooring& InMooring = FSaltMooring());
	/** Moored at that mooring now: placed there at rest, lines out, the sail taken in. */
	void MoorAt(const FSaltMooring& InMooring);
	/** The mooring it lies at (or last lay at). Its home is its start position (the pier). */
	const FSaltMooring& GetMooring() const { return Mooring; }
	const FSaltMooring& GetHomeMooring() const { return Home; }
	/** The nearest mooring within MooringRadius: its home or a USaltMooringPointComponent's in the world. */
	bool FindMooring(FSaltMooring& Out, double* OutDistanceM = nullptr) const;

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
	/** D65: the share of the full drive the set sail gives at the apparent wind's angle now (1 astern .. the floor ahead). */
	float GetSailDriveFactor() const { return SailDriveFactor; }
	/** The wind where she is (world XY, m/s). */
	FVector2D GetTrueWind() const { return LastTrueWind; }

	// D74: the ship repair system (Boat/SaltHullDamage). A hard knock: where on the hull (body metres) and how fast it closed (m/s).
	DECLARE_MULTICAST_DELEGATE_TwoParams(FSaltHullKnock, const FVector& /*BodyPointM*/, float /*ClosingSpeed*/);
	FSaltHullKnock OnHullKnock;
	/** Water in her bilge (kg): it weighs her down. Set by the hull damage. */
	float BilgeWaterKg = 0.f;
	/** What is left of the sail's drive (a torn sail, a boat low in the water), 0..1. Set by the hull damage. */
	float SailCondition = 1.f;
	USaltHullDamageComponent* GetHullDamage() const { return HullDamage; }
	/** Deepest penetration of the collision capsules into static geometry this frame (cm). */
	float GetContactDepth() const { return MaxContactDepth * 100.f; }
	int32 GetContactCount() const { return Contacts.Num(); }
	/** Height of the origin above the water surface at the origin, cm (0 = floating at the design waterline). */
	float GetWaterlineOffset() const;
	USaltStationComponent* GetHelm() const { return Helm; }
	USaltInteractableComponent* GetSailRope(bool bStarboard) const { return bStarboard ? SailRopeStarboard : SailRopePort; }
	USaltInteractableComponent* GetWindlass() const { return Windlass; }
	USaltInteractableComponent* GetLadder(bool bStarboard) const { return bStarboard ? LadderStarboard : LadderPort; }
	USaltInteractableComponent* GetStoreLid(bool bCrate) const { return bCrate ? CrateLid : BarrelLid; }
	USaltInventoryComponent* GetStore(bool bCrate) const { return bCrate ? CrateStore : BarrelStore; }
	/** The ladder as the player holds it (D54): the capsule centre's line from the lowest rung to the top, in the hull
	 *  component's space (cm), facing the hull, and the step on to the deck at the top. */
	FSaltLadderLine GetLadderLine(bool bStarboard, float CapsuleHalfHeight, float CapsuleRadius) const;
	/** 0 = anchor up .. 1 = anchor holding the boat (smooth stop, no turning). */
	float GetAnchorHold() const { return AnchorHold; }
	/** How far (m) the bow wave and Kelvin wake reach behind the stern: the boat's recent straight course (D48). */
	float GetWakeExtent() const;
	/** One line for logs and tests (SB_BOAT). */
	FString Describe() const;

	/** The lantern on the mast (D53): lit at dusk and in the evening by the lighting rig, or by hand. */
	void SetLanternLit(bool bLit);
	bool IsLanternLit() const { return bLanternLit; }
	/** A pull on the bell rope: the clapper swings over and strikes the bell. */
	void RingBell();
	USaltSwingComponent* GetBellSwing() const { return BellSwing; }
	USaltSwingComponent* GetClapperSwing() const { return ClapperSwing; }
	USaltSwingComponent* GetLanternSwing() const { return LanternSwing; }
	USaltInteractableComponent* GetBellRope() const { return BellRope; }
	USaltInteractableComponent* GetLanternUse() const { return LanternUse; }
	/** The chart on the mast (D57): where the boat is on the region chart (its window's centre, 0..1) and the E to look. */
	FVector2D GetChartCentre() const;
	USaltInteractableComponent* GetMastChartUse() const { return MastChartUse; }
	/** The value the chart's material shows (tests). */
	float GetMastChartParameter(FName Name) const;
	/** D58: the chart sheet's face in the hull's space (cm): its middle, X looking into it, Y to the viewer's right, Z up. */
	FTransform GetChartFaceLocal() const;
	/** D58: where the eyes go to look at the chart closely (hull space): straight in front of the sheet and so far back that
	 *  it fills 78 % of the view's height at this horizontal field of view (degrees) and aspect (width / height). */
	FTransform GetChartViewLocal(float FieldOfView, float Aspect) const;
	UPointLightComponent* GetLanternLight() const { return LanternLight; }
	/** D65: the sheets (from the pin rails to the clews) and the brails (from their blocks under the yard to the foot), and
	 *  the points on the sail their ends follow. */
	UCableComponent* GetSheet(bool bStarboard) const { return bStarboard ? SheetStarboard : SheetPort; }
	USceneComponent* GetClew(bool bStarboard) const { return bStarboard ? ClewStarboard : ClewPort; }
	UCableComponent* GetBrail(bool bStarboard) const { return bStarboard ? BrailStarboard : BrailPort; }
	USceneComponent* GetBrailFoot(bool bStarboard) const { return bStarboard ? BrailFootStarboard : BrailFootPort; }

	// Components ---------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Hull;
	/** D74: holes, water aboard, the sail's tears, and the means to mend them. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltHullDamageComponent> HullDamage;
	/** D65: the yard, slung across the mast's front and fixed square to the boat; the square sail hangs from it (both are
	 *  built in the yard's frame, SaltHullShape.h YardX / YardHeight). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> YardPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Yard;
	/** D65: a new name (component and property): the levels' saved boats still point their old "Sail" at the gaff sail. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> SquareSail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> RudderPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Rudder;
	/** The steering oar (D45): its pivot sits in the boss, raked aft; the oar turns about its shaft. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> OarPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> SteeringOar;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Wheel;
	/** Wind pennant at the masthead: swings to the apparent wind and streams out with its strength (D44). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> PennantPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Pennant;
	/** The ensign on its staff at the stern (D52): turns and flutters with the pennant. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> EnsignPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Ensign;
	/** Stores (D52): the lashed barrel and crate open as 10-slot stores. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> BarrelLid;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> CrateLid;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInventoryComponent> BarrelStore;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInventoryComponent> CrateStore;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltStationComponent> Helm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> SailRopePort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> SailRopeStarboard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> Windlass;
	/** The windlass drum (with its cranks) and the rope wound on it: they turn while the anchor goes down or up. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> WindlassDrum;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> WindlassRope;
	/** Boarding ladders over both sides at the gap: climb up from the water or down from the deck. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> LadderPort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> LadderStarboard;
	/** Gangway from the pier to the gap while moored; part of the boat, so it never cuts through it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Gangway;
	/** The anchor: hangs at the cathead, goes down to the seabed when dropped. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Anchor;
	/** Where the anchor rope leaves the hull (a hawse hole in the starboard bow, D52). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> Hawse;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> AnchorLine;
	/** D65: the sheets, from the pin rails' forward pins to the sail's clews, and the brails, from their blocks under the
	 *  yard down to the foot (hauled, they roll the sail up): their sail ends follow the cloth (UpdateRopes). The yard's
	 *  halyard, lifts and braces and the brails' falls never move: they are part of the hull. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> ClewPort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> ClewStarboard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> SheetPort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> SheetStarboard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> BrailFootPort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> BrailFootStarboard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> BrailPort;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCableComponent> BrailStarboard;
	/** The bell (D53): it hangs in the ring under the bell arm and swings with the boat; its clapper swings inside it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltSwingComponent> BellSwing;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Bell;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltSwingComponent> ClapperSwing;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> BellClapper;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> BellRope;
	/** The lantern (D53): it hangs on the hook aft of the mast and swings; light it or douse it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltSwingComponent> LanternSwing;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Lantern;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPointLightComponent> LanternLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> LanternUse;
	/** D57: the chart pinned to a board on the mast (aft and to starboard, chest height), live; E to look at it closely. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> MastChart;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USaltInteractableComponent> MastChartUse;
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
	/** Added mass of the water moving with the hull: heave, roll, pitch (fractions of the mass / inertia). D65: more (was 0.6,
	 *  0.25, 0.6), with the damping below: calmer in the waves (sb.Dev.BoatMotion). */
	UPROPERTY(EditAnywhere, Category = "Boat|Mass") FVector AddedMass = FVector(1.0, 0.5, 1.0);

	/** Vertical damping of the hull columns (N s/m per m2 of column area; D65: was 1400). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float VerticalDamping = 2400.f;
	/** The last step's vertical forces (N), for the sudden-heave diagnosis (D56). */
	double KickBuoyancy = 0.0, KickDamping = 0.0, KickTotal = 0.0;
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
	/** Roll and pitch damping (N m s; D65: was 25 000 and 20 000; more roll damping and she barely rolls back after a kick). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float RollDamping = 50000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float PitchDamping = 120000.f;
	/** D65 (owner: "make the starter ship calmer in the waves"): heave damping against the boat's own vertical speed (N s/m),
	 *  beside the hull columns' damping against the water's (VerticalDamping). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float HeaveDamping = 30000.f;
	/** D65: how far the water the hull columns feel is blended towards a plane (0 = the sea as it is, 1 = only the plane): its
	 *  height is the sea's under the hull, its slope the sea's over WaveFootprint times the hull's length and beam (nine samples).
	 *  The long swell's slope is the same over a wider area, a short wind sea's averages out: she rides the swell and rocks
	 *  less on the short waves. */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro", meta = (ClampMin = "0", ClampMax = "1")) float WaveFilter = 1.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro", meta = (ClampMin = "1", ClampMax = "4")) float WaveFootprint = 1.6f;
	/** D65: how slowly the water the hull feels follows the sea (time constants, s): its height (0 = at once) and its vertical
	 *  speed, which the columns' damping works against (D56: 0.06 s). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro", meta = (Units = "s")) float WaterHeightSmoothing = 0.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro", meta = (Units = "s")) float WaterVelocitySmoothing = 0.06f;

	/** Rudder blade area (m2). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RudderArea = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder", meta = (Units = "deg")) float MaxRudderAngle = 35.f;
	/** How fast the helm turns the rudder, and how fast it returns to the centre when let go (deg/s). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RudderRate = 50.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RudderReturnRate = 30.f;
	/** Yaw torque at full rudder when at rest ("sculling"), fading out by 1.5 m/s (N m). */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float RestTurnTorque = 1500.f;
	/** Wheel turns per rudder turn (visual): full rudder is one full turn of the wheel either way. */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") float WheelRatio = 360.f / 35.f;
	/** How the boat is steered: the steering oar (first boat) or the wheel and sternpost rudder. */
	UPROPERTY(EditAnywhere, Category = "Boat|Rudder") ESaltSteering Steering = ESaltSteering::SteeringOar;

	/** Forward force of the full sail in a 7 m/s breeze with the wind astern (N; D65: the most it gives). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float SailThrust = 7800.f;
	/** D65 (owner: "always pulls"): the share of SailThrust the square sail gives with the apparent wind abeam, and head to wind
	 *  (the floor: she is never stuck). In between it follows a smooth curve, the whole thrust with the wind astern. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (ClampMin = "0", ClampMax = "1")) float SailDriveAbeam = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (ClampMin = "0", ClampMax = "1")) float SailDriveHeadToWind = 0.28f;
	/** Extra forward drag per m/s (N s/m), so the boat slows down within about 15 s once the sail is taken in
	 *  (simple, arcade-friendly handling; engines will drive the same way). */
	UPROPERTY(EditAnywhere, Category = "Boat|Hydro") float CoastDrag = 1500.f;
	/** Sideways share of the sail force (heels the boat to leeward). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float SailHeel = 0.22f;
	/** How strongly the wind's speed shades the drive: 0 = the same force in any breeze, 1 = force with the wind's speed. */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail") float WindInfluence = 0.35f;
	/** Time to let the sail fall or roll it up. */
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
	/** Mooring is possible within this distance of a mooring (the start position, or a mooring point: D62). */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "m")) float MooringRadius = 8.f;
	/** Where the mooring lines reach the pier: this far to starboard (+) or port (-) of the bitts at the start. */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "cm")) float MooringReach = 240.f;
	/** How high the pier (or quay) stands over the water at the start: the gangway rests on it, the lines are made fast a
	 *  little higher. The cove's pier is 111 cm; the hub's quays 150 cm (D61, set on the boat in the hub's blockout map). */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "cm")) float MooringPierHeight = 111.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "s")) float AnchorDropTime = 3.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines", meta = (Units = "s")) float AnchorRaiseTime = 4.f;
	/** Anchored (D43): the boat comes to a smooth stop and holds its place and heading, whatever the sail does.
	 *  Brake (N s/m, N m s) and hold springs (N/m, N m/rad). */
	UPROPERTY(EditAnywhere, Category = "Boat|Lines") float AnchorBrake = 6000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines") float AnchorYawBrake = 90000.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines") float AnchorHoldSpring = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Lines") float AnchorYawSpring = 60000.f;

	/** Initial state when play starts. */
	UPROPERTY(EditAnywhere, Category = "Boat") ESaltBoatHold StartHold = ESaltBoatHold::Moored;
	/** D59: the boat's row in the stats table (Core/SaltStats): speed, sail and steering numbers and its stores, read when
	 *  play starts (they override the tunables above). */
	UPROPERTY(EditAnywhere, Category = "Boat") FName BoatClass = TEXT("DoubleEnder");
	/** Seconds for W / S at the helm to let the sail out from nothing to full (D59). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "s")) float TrimTime = 3.f;
	/** How deep the sail bellies beyond its cut shape in a fresh breeze (cm, D59), and its cut shape's own belly when it fills
	 *  (cm, D65: the square sail hangs flat when slack). */
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "cm")) float SailBelly = 70.f;
	UPROPERTY(EditAnywhere, Category = "Boat|Sail", meta = (Units = "cm")) float SailCutBelly = 30.f;

	/** The sea is cut out inside this boat's hull (MPC_Ocean Boat*, D42): no waves inside the boat or on its deck. */
	UPROPERTY(EditAnywhere, Category = "Boat") bool bMasksOcean = true;
	/** Height of the bow wave and wake the sea shows (D44); 0 = none. */
	UPROPERTY(EditAnywhere, Category = "Boat", meta = (ClampMin = "0")) float WakeStrength = 1.f;

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

	void ApplyStats();
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
	UFUNCTION() void HandleLadder(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	UFUNCTION() void HandleStore(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	UFUNCTION() void HandleBell(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	UFUNCTION() void HandleLantern(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	UFUNCTION() void HandleChart(USaltInteractableComponent* Interactable, ASaltPlayerCharacter* Player);
	void UpdateMastChart();
	/** The lantern's light and glass follow its state, with a little flicker while it burns. */
	void UpdateLantern(float DeltaSeconds);
	void UpdateGangway();
	/** Shows the steering oar or the wheel and rudder, and puts the helm station at its handle or wheel. */
	void ApplySteeringLayout();
	/** Centre of the steering foil (oar blade or rudder blade), body m. */
	FVector RudderFoilPoint() const;
	void UpdateTrail(float DeltaSeconds);
	void ClearTrail();
	static float TrailStrengthAt(float SpeedMs);
	/** D65: how far the sail hangs down from its head now (m): what is set, at least SailCutMin; below it the cloth is rolled. */
	float SailCut() const;
	/** D65: where M_SB_SquareSail puts a clew (the roll's end once it is rolled) and the brail's end on the foot (the cloth's
	 *  aft face at the cut), in the yard's frame (cm). */
	FVector SailClewLocal(bool bStarboard) const;
	FVector SailBrailFootLocal(bool bStarboard) const;
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
	/** D65: the square sail's centre of effort above the deck at the mast (its centroid, set). */
	float SailCEHeight = 5.4f;

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
	bool bSailLocked = false;                // FH1 (D77)
	FText SailLockedText;
	float SailTarget = 0.f;                  // D59: how much sail is wanted (0..1)
	float Hoist = 0.f;                       // 0 taken in .. 1 set
	float SailLean = 0.f;                    // D65: the cloth leans to leeward (-1 port .. 1 starboard, visual)
	float SailBellyShown = 0.f;              // D65: the belly the material shows now (cm)
	float SailDriveFactor = 0.f;             // D65: share of the full drive at the apparent wind's angle (last step)
	float PennantYaw = 180.f;                  // deg in the hull's frame (streaming aft)
	float PennantWind = 0.f;                   // 0 = hanging .. 1 = streaming out
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PennantMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EnsignMaterials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LanternGlassMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MastChartMaterial;
	bool bLanternLit = false;
	float LanternFlicker = 0.f;
	float Fill = 0.f;                        // belly fill (spring: pops when the sail catches the wind)
	float FillVel = 0.f;
	float Luff = 0.f;                        // 0..1, wind from ahead: the sail shivers
	float SailPress = 0.f;                   // D59: 0..1, how hard the apparent wind presses the sail
	float AvgWind = 0.f;                     // slow average of the wind speed (gusts against it)
	float SailDrive = 0.f;                   // N, last forward sail force (logs)
	bool bOverride = false;
	float OverrideRudder = 0.f;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> SailMaterials;
	// Anchor: 0 stowed at the cathead .. 1 on the seabed; the hold ramps in as it bites.
	float AnchorAlpha = 0.f;
	float AnchorHold = 0.f;
	FVector HoldPoint = FVector::ZeroVector;   // world, m (origin)
	float HoldYaw = 0.f;
	bool bHoldSet = false;                     // the boat has come to rest: hold this place and heading
	float WindlassAngle = 0.f;
	float AnchorDepth = 500.f;                 // cm of rode paid out at the bottom
	// Trail (D45): the stern's path, one sample a second, newest first (world cm), and the speed there (0..1).
	TArray<FVector2D> TrailPoints;
	TArray<float> TrailStrength;
	float TrailClock = 0.f;
	float WakeExtentShown = 0.f;               // eased and capped GetWakeExtent() for the sea (m)
	FVector AnchorGround = FVector::ZeroVector;   // world, cm
	bool bAutopilot = false;
	float AutopilotYaw = 0.f;
	FVector2D LastApparentWind = FVector2D::ZeroVector;   // body XY, m/s
	FVector2D LastTrueWind = FVector2D::ZeroVector;       // world XY, m/s

	ESaltBoatHold Hold = ESaltBoatHold::Free;
	/** D70: a passage sails her (SetScripted); she leans a little into its turns. */
	bool bScripted = false;
	float ScriptedHeel = 0.f;
	TArray<FLine> Lines;
	TArray<FContact> Contacts;
	float MaxContactDepth = 0.f;
	/** Static geometry within reach this frame (broad phase); otherwise no contact or sweep queries run. */
	bool bNearStatic = false;

	FTransform StartTransform;
	/** D62: the home mooring (the start position with MooringReach / MooringPierHeight) and the one in use. */
	FSaltMooring Home;
	FSaltMooring Mooring;
	void UpdateHomeMooring();
	struct FSafePose { FVector Origin; float Yaw; };
	TArray<FSafePose> SafePoses;
	float SafePoseTimer = 0.f;
	float CapsizeTimer = 0.f;
	double Time = 0.0;
	bool bInitialised = false;
};

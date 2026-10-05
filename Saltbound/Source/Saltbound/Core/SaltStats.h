// Saltbound - the stats of the things in the game, one table per kind (D59, owner: "create lists for stats for each
// object like the ships"): boats (with their stores) and the player. Change a number here and the game follows: the
// boat reads its row when play starts, the player when it is made. Items and tools have their own tables (SaltItems,
// SaltTools). Docs/Stats.md lists every table with what the numbers do and what was measured.

#pragma once

#include "CoreMinimal.h"

/** A kind of boat. */
struct FSaltBoatStats
{
	FName Id;
	FText Name;

	// Speed ------------------------------------------------------------------------------------------------------
	/** Forward force of the full sail in a 7 m/s breeze with the wind astern (N). Top speed is where it meets the resistance
	 *  below. */
	float SailThrust = 7800.f;
	/** D65 ("always pulls"): the share of SailThrust the sail gives with the apparent wind abeam and head to wind (it never
	 *  stops pulling); a smooth curve between, the whole of it with the wind astern. */
	float SailDriveAbeam = 0.6f;
	float SailDriveHeadToWind = 0.28f;
	/** Hull speed (m/s) and the resistance there (N): past it the resistance climbs steeply (a wall near hull speed). */
	float HullSpeed = 3.75f;
	float HullResistance = 1700.f;
	/** Extra drag per m/s (N s/m): how fast the boat slows when the sail comes in. */
	float CoastDrag = 1500.f;

	// The sail ---------------------------------------------------------------------------------------------------
	/** Seconds to set the sail fully or take it in (E at the pin rails). */
	float SetTime = 2.5f;
	/** Seconds for W / S at the helm to let the sail out from nothing to full (or take it in). */
	float TrimTime = 3.f;
	/** How deep the sail bellies in a fresh breeze beyond its cut shape (cm, at its deepest). */
	float SailBelly = 70.f;

	// Steering ---------------------------------------------------------------------------------------------------
	float MaxRudderAngle = 35.f;   // degrees
	float RudderRate = 50.f;       // degrees a second the helm turns the rudder
	float RestTurnTorque = 1500.f; // N m: turning on the spot with the oar ("sculling")

	// Stores -----------------------------------------------------------------------------------------------------
	int32 BarrelSlots = 10;
	int32 CrateSlots = 10;
};

/** The player. */
struct FSaltPlayerStats
{
	// Walking (cm/s, cm/s2) --------------------------------------------------------------------------------------
	float WalkSpeed = 250.f;
	float SprintSpeed = 420.f;
	/** How fast the body gets up to speed and stops (higher = snappier). */
	float Acceleration = 2800.f;
	float Braking = 2600.f;
	float GroundFriction = 10.f;
	/** Highest step the feet walk up without jumping (cm). */
	float MaxStepHeight = 25.f;

	// Jumping ------------------------------------------------------------------------------------------------------
	/** Take-off speed (cm/s) and gravity's scale: together a 60 cm jump, quick up and down. */
	float JumpSpeed = 420.f;
	float GravityScale = 1.5f;
	/** Steering in the air (0 = none, 1 = as on the ground). */
	float AirControl = 0.35f;

	// Swimming (cm/s) ----------------------------------------------------------------------------------------------
	float SwimSpeed = 300.f;
	float SwimSprintSpeed = 460.f;

	// Ladders (cm/s; s) --------------------------------------------------------------------------------------------
	/** Climbing speed, the same up and down, eased in and out. */
	float LadderSpeed = 190.f;
	/** Speed of the move on to the ladder (cm/s), and over its top on to the deck (s). */
	float LadderMountSpeed = 320.f;
	float LadderTopTime = 0.35f;

	// The eyes ---------------------------------------------------------------------------------------------------
	float EyeHeight = 120.f;          // cm above the feet
	float InteractionRange = 220.f;   // cm
};

namespace SaltStats
{
	/** The boats; the first is the boat the player starts with. */
	SALTBOUND_API const TArray<FSaltBoatStats>& Boats();
	/** A boat's row (the first one when there is no such id). */
	SALTBOUND_API const FSaltBoatStats& Boat(FName Id);
	SALTBOUND_API const FSaltPlayerStats& Player();
}

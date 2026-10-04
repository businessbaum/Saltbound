// Saltbound - hull shape of the starter boat (greybox, D36).
//
// The same formulas build the mesh in Art/Blender/Scripts/build_boat.py: keep both in sync. Units are metres in
// body space: +X bow, +Y starboard, +Z up, origin midship on the design waterline.
//
// A beamy double-ender (D13): 10 m on deck, 3.4 m beam, 0.75 m freeboard rising to 1.05 m at the ends. The
// canoe body is 0.65 m deep; a long keel runs from the forefoot to a 1.45 m deep heel at the sternpost.

#pragma once

#include "CoreMinimal.h"

namespace SaltHull
{
	constexpr float HalfLength = 5.0f;        // stem and stern at the sheer
	constexpr float HalfBeam = 1.7f;          // at the sheer, midship
	constexpr float Freeboard = 0.75f;        // sheer height midship
	constexpr float SheerRise = 0.3f;         // extra sheer height at the ends
	constexpr float Draft = 0.65f;            // canoe body, midship centreline
	constexpr float BilgeExponent = 0.8f;     // section fullness (lower = fuller bilge)
	constexpr float EndsStartX = 3.0f;        // stem and stern profiles start rising here
	constexpr float KeelAftX = -4.3f;         // sternpost
	constexpr float KeelAftZ = -1.45f;        // heel of the keel
	constexpr float KeelForeX = 3.6f;         // the keel fairs into the forefoot here
	constexpr float KeelHalfThickness = 0.14f;
	constexpr float BulwarkHeight = 0.45f;
	constexpr float BulwarkThickness = 0.07f;
	/** Boarding gaps in the bulwark on both sides, midship (|X| below this). */
	constexpr float GateHalfWidth = 0.5f;

	/** Rig (the gaff mainsail is the only sail): mast, boom and gaff, heights above the deck at the mast. */
	constexpr float MastX = 1.2f;
	constexpr float MastHeight = 10.5f;
	constexpr float PennantHeight = 0.42f;    // the wind pennant's swivel above the mast top

	/** D52: the anchor rope leaves through a hawse hole in the starboard bow planking (the anchor itself is never seen),
	 *  and the ensign flies from a staff on the stern deck (build_boat.py HAWSE_HULL, ENSIGN_X / ENSIGN_H). */
	constexpr float HawseX = 4.2f;
	constexpr float HawseZ = 0.66f;
	constexpr float EnsignX = -4.62f;
	constexpr float EnsignHeight = 1.45f;     // staff above the deck
	constexpr float BoomHeight = 2.05f;
	constexpr float BoomLength = 5.0f;
	constexpr float ThroatHeight = 6.8f;
	constexpr float GaffLength = 4.6f;
	constexpr float GaffAngleDeg = 35.f;

	/** Wheel steering (D36-D43, ESaltSteering::Wheel, kept for later boats): the wheel on its pedestal and the
	 *  rudder stock (raked, from its heel up to the deck). The first boat steers with an oar (below). */
	constexpr float WheelX = -3.2f;
	constexpr float WheelHeight = 0.68f;      // hub above the deck
	constexpr float HelmsmanX = -3.85f;
	constexpr float RudderHeelX = -4.38f;
	constexpr float RudderHeelZ = -1.45f;
	constexpr float RudderHeadX = -4.98f;
	constexpr float RudderHeadZ = 1.0f;
	constexpr float RudderChord = 0.6f;
	constexpr float RudderBladeTopZ = 0.0f;   // the wide blade ends at the waterline; a narrow head above

	/** Windlass on the foredeck (drum axis height above the deck) and the boarding ladders over both sides at the gap. */
	constexpr float DeckCamber = 0.04f;       // deck crown on the centreline
	constexpr float WindlassX = 3.55f;
	constexpr float WindlassDrumHeight = 0.34f;
	constexpr float WindlassDrumRadius = 0.14f;
	constexpr float LadderX = 0.0f;
	constexpr float LadderOut = 0.07f;        // ladder plane outside the sheer
	constexpr float LadderBottomZ = -0.75f;

	inline float SheerZ(float X)
	{
		const float T = X / HalfLength;
		return Freeboard + SheerRise * T * T;
	}

	inline float HalfBeamAtSheer(float X)
	{
		const float T = FMath::Min(FMath::Abs(X) / HalfLength, 1.f);
		return HalfBeam * FMath::Pow(FMath::Max(1.f - FMath::Pow(T, 2.2f), 0.f), 0.55f);
	}

	/** Centreline bottom of the canoe body: flat midship, rising to the sheer at the stem and stern. */
	inline float BottomZ(float X)
	{
		const float S = FMath::Clamp((FMath::Abs(X) - EndsStartX) / (HalfLength - EndsStartX), 0.f, 1.f);
		return -Draft + (Draft + SheerZ(HalfLength)) * S * S * S;
	}

	/** Hull surface height at lateral offset Y (|Y| up to the sheer half beam). Sections are quarter-ellipse-like:
	 *  Y = Bs * sin(t)^BilgeExponent, Z = Zs - (Zs - Zb) * cos(t), t from 0 (keel) to 90 deg (sheer). */
	inline float SectionZ(float X, float Y)
	{
		const float Bs = HalfBeamAtSheer(X);
		const float Zs = SheerZ(X);
		const float Zb = BottomZ(X);
		if (Bs <= UE_KINDA_SMALL_NUMBER)
		{
			return Zs;
		}
		const float SinT = FMath::Pow(FMath::Clamp(FMath::Abs(Y) / Bs, 0.f, 1.f), 1.f / BilgeExponent);
		return Zs - (Zs - Zb) * FMath::Sqrt(FMath::Max(1.f - SinT * SinT, 0.f));
	}

	/** Half breadth of the hull at height Z (0 below the bottom). */
	inline float HalfBreadthAt(float X, float Z)
	{
		const float Zs = SheerZ(X);
		const float Zb = BottomZ(X);
		if (Z <= Zb || Zs - Zb <= UE_KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		const float CosT = FMath::Clamp((Zs - Z) / (Zs - Zb), 0.f, 1.f);
		return HalfBeamAtSheer(X) * FMath::Pow(FMath::Sqrt(1.f - CosT * CosT), BilgeExponent);
	}

	/** Keel bottom for X in [KeelAftX, KeelForeX]: a straight line from the heel to the forefoot. */
	inline float KeelZ(float X)
	{
		const float A = FMath::Clamp((X - KeelAftX) / (KeelForeX - KeelAftX), 0.f, 1.f);
		return FMath::Lerp(KeelAftZ, BottomZ(KeelForeX), A);
	}

	/** Lowest point of the hull at X on the centreline (keel where there is one). */
	inline float DeepestZ(float X)
	{
		return (X >= KeelAftX && X <= KeelForeX) ? FMath::Min(KeelZ(X), BottomZ(X)) : BottomZ(X);
	}

	/** Steering oar (D45): a big oar hung on the starboard quarter, held in a boss on the sheer, raked aft and turned
	 *  about its shaft by a handle across the boat. Distances along the shaft are measured from the boss (+ up). */
	constexpr float OarBossX = -3.35f;
	constexpr float OarBossOut = 0.14f;       // shaft outside the sheer
	constexpr float OarBossHeight = 0.1f;     // above the sheer
	constexpr float OarRakeDeg = 16.5f;       // the lower end aft
	constexpr float OarTip = -2.05f;
	constexpr float OarBladeTop = -1.15f;
	constexpr float OarBladeChord = 0.5f;     // aft of the shaft
	constexpr float OarHead = 0.72f;          // the handle sits here
	constexpr float OarHandleLength = 0.85f;  // inboard, across the boat
	constexpr float OarHelmsmanX = -3.75f;    // behind the handle, clear of the aft bitts
	constexpr float OarHelmsmanY = 0.55f;

	inline FVector OarBoss()
	{
		return FVector(OarBossX, HalfBeamAtSheer(OarBossX) + OarBossOut, SheerZ(OarBossX) + OarBossHeight);
	}

	/** Point on the oar's shaft, Along metres from the boss. */
	inline FVector OarAlong(float Along)
	{
		const float Rake = FMath::DegreesToRadians(OarRakeDeg);
		return OarBoss() + FVector(FMath::Sin(Rake), 0.f, FMath::Cos(Rake)) * Along;
	}

	/** Direction from the shaft to the blade's trailing edge (aft, tilted with the rake). */
	inline FVector OarAft()
	{
		const float Rake = FMath::DegreesToRadians(OarRakeDeg);
		return FVector(-FMath::Cos(Rake), 0.f, FMath::Sin(Rake));
	}

	/** D53, the mast and what hangs on it (build_boat.py MAST_HEAD_OFFSET and its SB_MAST / SB_GAFF prints): the mast's
	 *  head is worked aft and to port, so the pennant's swivel sits there; the throat and peak halyards' blocks on the
	 *  mast (where their ropes leave them); the bell's and the lantern's pivots; on the gaff (its own frame, m), the throat
	 *  halyard's eye and the top of the peak halyard's span block. */
	constexpr float MastHeadAft = 0.07f;
	constexpr float MastHeadPort = 0.02f;
	inline FVector ThroatBlock() { return FVector(MastX - 0.134f, -0.113f, SheerZ(MastX) + 7.275f); }
	inline FVector PeakBlock() { return FVector(MastX - 0.189f, 0.083f, SheerZ(MastX) + 9.895f); }
	inline FVector BellPivot() { return FVector(1.686f, 0.f, SheerZ(MastX) + 1.494f); }
	inline FVector LanternPivot() { return FVector(0.937f, 0.f, SheerZ(MastX) + 1.704f); }
	inline FVector GaffThroatEye() { return FVector(-0.271f, 0.f, 0.117f); }
	inline FVector GaffSpanBlock() { return FVector(-2.676f, 0.f, 0.503f); }
	/** The halyards' falls are made fast on pin rails inside the bulwark at the shrouds (the sail is let go there). */
	constexpr float PinRailX = MastX + 0.35f;
	inline float PinRailY() { return HalfBeamAtSheer(PinRailX) - 0.165f; }
}

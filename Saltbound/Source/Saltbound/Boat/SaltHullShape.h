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
	constexpr float BoomHeight = 2.05f;
	constexpr float BoomLength = 5.0f;
	constexpr float ThroatHeight = 6.8f;
	constexpr float GaffLength = 4.6f;
	constexpr float GaffAngleDeg = 35.f;

	/** Helm: the wheel on its pedestal, and the rudder stock (raked, from its heel up to the deck). */
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
}

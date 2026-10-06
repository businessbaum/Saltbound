// Saltbound - default content used by native actors.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"

/**
 * Native classes never load /Game content in their constructors. Class default objects are built while
 * the engine starts up, and everything loaded then joins the root set for good: the content scripts can
 * no longer rebuild such assets (deleting a rooted material expression asserts). Defaults are resolved
 * lazily in OnConstruction instead. The placed instance then stores a hard reference, so cooking finds it.
 */
namespace SaltContent
{
	inline const TCHAR* OceanParameterCollection = TEXT("/Game/Saltbound/Water/MPC_Ocean.MPC_Ocean");
	inline const TCHAR* OceanMaterial = TEXT("/Game/Saltbound/Water/MI_Ocean.MI_Ocean");
	inline const TCHAR* OceanTile = TEXT("/Game/Saltbound/Water/SM_OceanTile32.SM_OceanTile32");
	inline const TCHAR* OceanQuarterTile = TEXT("/Game/Saltbound/Water/SM_OceanTile16.SM_OceanTile16");
	/** The sea seen from below and the underwater post-process pass (setup_ocean.py). */
	inline const TCHAR* OceanUnderMaterial = TEXT("/Game/Saltbound/Water/MI_OceanUnder.MI_OceanUnder");
	inline const TCHAR* UnderwaterPostMaterial = TEXT("/Game/Saltbound/Water/MI_PP_Underwater.MI_PP_Underwater");
	inline const TCHAR* UnderwaterParticleMaterial = TEXT("/Game/Saltbound/Water/MI_UnderwaterParticles.MI_UnderwaterParticles");
	inline const TCHAR* UnderwaterParticleMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	inline const TCHAR* LanternMesh = TEXT("/Game/Saltbound/Props/SM_Lantern.SM_Lantern");
	inline const TCHAR* CloudMaterial = TEXT("/Game/Saltbound/Environment/Sky/MI_Clouds_Stylized.MI_Clouds_Stylized");
	/** D72: the sky dome (the atmosphere, the sun's and the moon's discs, the stars) and its parameters (setup_night_sky.py). */
	inline const TCHAR* SkyDomeMesh = TEXT("/Game/Saltbound/Environment/Sky/SM_SkyDome.SM_SkyDome");
	inline const TCHAR* SkyMaterial = TEXT("/Game/Saltbound/Environment/Sky/M_SB_Sky.M_SB_Sky");
	inline const TCHAR* SkyCollection = TEXT("/Game/Saltbound/Environment/Sky/MPC_Sky.MPC_Sky");
	/** Rope material for the boat's physics ropes (cable components). */
	inline const TCHAR* RopeMaterial = TEXT("/Game/Saltbound/Materials/Instances/MI_Rope_Hemp.MI_Rope_Hemp");
	/** D76: the skinned seagull (build_birds.py); ASaltBirds poses its bones. */
	inline const TCHAR* GullMesh = TEXT("/Game/Saltbound/Environment/Birds/SK_Gull.SK_Gull");
	/** Seagull parts (Art/Blender/Scripts/build_birds.py): Body, WingL, WingR. */
	inline FString BirdMesh(const TCHAR* Part)
	{
		return FString::Printf(TEXT("/Game/Saltbound/Environment/Birds/SM_Gull_%s.SM_Gull_%s"), Part, Part);
	}
	/** Starter boat parts (Art/Blender/Scripts/build_boat.py): /Game/Saltbound/Boat/SM_Boat_<Part>. */
	inline FString BoatMesh(const TCHAR* Part)
	{
		return FString::Printf(TEXT("/Game/Saltbound/Boat/SM_Boat_%s.SM_Boat_%s"), Part, Part);
	}
	/** The hub ship's art meshes (Art/Blender/Scripts/build_hub_*.py): /Game/Saltbound/Hub/Art/SM_Hub_<Part>. */
	inline FString HubMesh(const TCHAR* Part)
	{
		return FString::Printf(TEXT("/Game/Saltbound/Hub/Art/SM_Hub_%s.SM_Hub_%s"), Part, Part);
	}
	/** FFT ocean render targets (setup_ocean.py): /Game/Saltbound/Water/FFT/RT_Ocean<Kind><Cascade>. */
	inline FString OceanFFTTarget(const TCHAR* Kind, int32 Cascade)
	{
		return FString::Printf(TEXT("/Game/Saltbound/Water/FFT/RT_Ocean%s%d.RT_Ocean%s%d"), Kind, Cascade, Kind, Cascade);
	}

	/** Loads Path into Slot if Slot is empty. */
	template <typename T, typename PtrType>
	void ResolveDefault(PtrType& Slot, const FString& Path)
	{
		if (!Slot)
		{
			Slot = LoadObject<T>(nullptr, *Path);
		}
	}

	template <typename T, typename PtrType>
	void ResolveDefault(PtrType& Slot, const TCHAR* Path)
	{
		if (!Slot)
		{
			Slot = LoadObject<T>(nullptr, Path);
		}
	}
}

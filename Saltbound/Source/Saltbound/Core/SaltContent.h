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
	inline const TCHAR* LanternMesh = TEXT("/Game/Saltbound/Props/SM_Lantern.SM_Lantern");
	inline const TCHAR* CloudMaterial = TEXT("/Game/Saltbound/Environment/Sky/MI_Clouds_Stylized.MI_Clouds_Stylized");
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

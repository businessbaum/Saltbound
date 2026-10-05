// Saltbound - the tools on the tool wheel (D55, UI_Design.md section 4).

#pragma once

#include "CoreMinimal.h"

/** What taking a tool does. */
enum class ESaltToolUse : uint8
{
	Held,       // in the hand: compass, spyglass, lantern
	Satchel,    // opens the satchel
	PutAway,    // empty hands
};

/** One entry of the wheel, in its order (keys 1 to 5, clockwise from the top). No chart: the chart is on the mast (D57). */
struct FSaltToolDef
{
	FName Id;
	FText Name;
	FText Hint;
	/** Its medallion picture under /Game/Saltbound/UI/Textures (Tools/textures/ui_parts.js). */
	const TCHAR* Picture = nullptr;
	ESaltToolUse Use = ESaltToolUse::Held;
	/** Always there (the satchel, empty hands); the others must be owned (the player starts with the compass,
	 *  the spyglass and the lantern, owner). */
	bool bAlwaysOwned = false;
};

namespace SaltTools
{
	SALTBOUND_API const TArray<FSaltToolDef>& All();
	SALTBOUND_API int32 IndexOf(FName Id);
}

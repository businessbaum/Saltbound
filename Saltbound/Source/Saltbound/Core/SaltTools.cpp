// Saltbound - the tools on the tool wheel (D55).

#include "Core/SaltTools.h"

const TArray<FSaltToolDef>& SaltTools::All()
{
	static const TArray<FSaltToolDef> Tools = {
		{ TEXT("Compass"), NSLOCTEXT("Saltbound", "ToolCompass", "Compass"), NSLOCTEXT("Saltbound", "ToolCompassHint", "Read your heading"),
			TEXT("T_UI_Tool_Compass"), ESaltToolUse::Held, false },
		{ TEXT("Spyglass"), NSLOCTEXT("Saltbound", "ToolSpyglass", "Spyglass"), NSLOCTEXT("Saltbound", "ToolSpyglassHint", "Look far ahead"),
			TEXT("T_UI_Tool_Spyglass"), ESaltToolUse::Held, false },
		{ TEXT("Lantern"), NSLOCTEXT("Saltbound", "ToolLantern", "Lantern"), NSLOCTEXT("Saltbound", "ToolLanternHint", "Light for the night"),
			TEXT("T_UI_Tool_Lantern"), ESaltToolUse::Held, false },
		{ TEXT("Satchel"), NSLOCTEXT("Saltbound", "ToolSatchel", "Satchel"), NSLOCTEXT("Saltbound", "ToolSatchelHint", "Open the satchel"),
			TEXT("T_UI_Tool_Satchel"), ESaltToolUse::Satchel, true },
		{ TEXT("EmptyHands"), NSLOCTEXT("Saltbound", "ToolHands", "Empty hands"), NSLOCTEXT("Saltbound", "ToolHandsHint", "Put everything away"),
			TEXT("T_UI_Tool_Hands"), ESaltToolUse::PutAway, true },
	};
	return Tools;
}

int32 SaltTools::IndexOf(FName Id)
{
	return All().IndexOfByPredicate([Id](const FSaltToolDef& T) { return T.Id == Id; });
}

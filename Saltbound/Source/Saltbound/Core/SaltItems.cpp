// Saltbound - items the player can carry (D50), defined in code like the input and the UI.

#include "Core/SaltItems.h"

namespace SaltItems
{
	const TArray<FSaltItemDef>& All()
	{
		static const TArray<FSaltItemDef> Items = {
			{ TEXT("Driftwood"), NSLOCTEXT("SaltItems", "DriftwoodName", "Driftwood"),
				NSLOCTEXT("SaltItems", "DriftwoodDesc", "Sun-bleached wood washed up on the shore. Light, dry and good for small repairs."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_Driftwood.T_Icon_Driftwood"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_Driftwood.SM_Pickup_Driftwood"), 20 },
			{ TEXT("Seashell"), NSLOCTEXT("SaltItems", "SeashellName", "Seashell"),
				NSLOCTEXT("SaltItems", "SeashellDesc", "A ribbed shell, pink inside. The beaches of these waters are full of them."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_Seashell.T_Icon_Seashell"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_Seashell.SM_Pickup_Seashell"), 30 },
			{ TEXT("RopeCoil"), NSLOCTEXT("SaltItems", "RopeCoilName", "Coil of rope"),
				NSLOCTEXT("SaltItems", "RopeCoilDesc", "A few fathoms of hemp rope. Every boat needs more of it than it has."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_RopeCoil.T_Icon_RopeCoil"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_RopeCoil.SM_Pickup_RopeCoil"), 10 },
			{ TEXT("GlassFloat"), NSLOCTEXT("SaltItems", "GlassFloatName", "Glass float"),
				NSLOCTEXT("SaltItems", "GlassFloatDesc", "A green glass fishing float in a rope net, drifted from who knows where."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_GlassFloat.T_Icon_GlassFloat"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_GlassFloat.SM_Pickup_GlassFloat"), 10 },
			{ TEXT("AncientShard"), NSLOCTEXT("SaltItems", "AncientShardName", "Ancient shard"),
				NSLOCTEXT("SaltItems", "AncientShardDesc", "A sliver of carved stone that glows blue-green, like the band on the old tower. Someone shaped it, long ago."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_AncientShard.T_Icon_AncientShard"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_AncientShard.SM_Pickup_AncientShard"), 5,
				NSLOCTEXT("SaltItems", "AncientShardUse", "Hold it up to the light") },
			// D73 (M6 step 2): the first materials for repairs and upgrades, dug up, washed up and traded.
			{ TEXT("Canvas"), NSLOCTEXT("SaltItems", "CanvasName", "Sailcloth"),
				NSLOCTEXT("SaltItems", "CanvasDesc", "A folded length of heavy canvas, stiff with salt. Enough to patch a sail, or two."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_Canvas.T_Icon_Canvas"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_Canvas.SM_Pickup_Canvas"), 10 },
			{ TEXT("IronNails"), NSLOCTEXT("SaltItems", "IronNailsName", "Iron nails"),
				NSLOCTEXT("SaltItems", "IronNailsDesc", "Square iron nails in a twist of oiled cloth. No plank stays where it is put without them."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_IronNails.T_Icon_IronNails"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_IronNails.SM_Pickup_IronNails"), 40 },
			{ TEXT("Tar"), NSLOCTEXT("SaltItems", "TarName", "Keg of tar"),
				NSLOCTEXT("SaltItems", "TarDesc", "Pine tar, black and sticky. It keeps the sea out of the seams."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_Tar.T_Icon_Tar"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_Tar.SM_Pickup_Tar"), 5 },
			{ TEXT("PaleFloat"), NSLOCTEXT("SaltItems", "PaleFloatName", "Pale float"),
				NSLOCTEXT("SaltItems", "PaleFloatDesc", "A hollow, perfectly round float of something that is neither glass nor wood. A seam runs round it, as if it came out of a mould, and a number is pressed into it: 17."),
				TEXT("/Game/Saltbound/UI/Textures/T_Icon_PaleFloat.T_Icon_PaleFloat"),
				TEXT("/Game/Saltbound/Props/Pickups/SM_Pickup_PaleFloat.SM_Pickup_PaleFloat"), 5 },
		};
		return Items;
	}

	const FSaltItemDef* Find(FName Id)
	{
		return All().FindByPredicate([Id](const FSaltItemDef& D) { return D.Id == Id; });
	}
}

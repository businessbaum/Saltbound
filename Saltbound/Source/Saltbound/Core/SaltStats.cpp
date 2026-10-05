// Saltbound - the stats tables (D59). See SaltStats.h and Docs/Stats.md.

#include "Core/SaltStats.h"

namespace SaltStats
{
	const TArray<FSaltBoatStats>& Boats()
	{
		static const TArray<FSaltBoatStats> Table = []()
		{
			TArray<FSaltBoatStats> T;
			// The first boat: a 10 m double-ender with a square sail (D65; gaff-rigged before), 10.9 t, steered with an oar.
			FSaltBoatStats Starter;
			Starter.Id = TEXT("DoubleEnder");
			Starter.Name = NSLOCTEXT("SaltStats", "DoubleEnder", "Double-ender");
			Starter.SailThrust = 10000.f;   // D59 (owner: "a bit faster"); D65: with the wind astern, about 8.4 kn as the best before
			Starter.SailDriveAbeam = 0.6f;          // D65 ("always pulls"): 0.6 of it with the apparent wind abeam
			Starter.SailDriveHeadToWind = 0.28f;    // and 0.28 head to wind: never stuck
			Starter.HullSpeed = 3.75f;
			Starter.HullResistance = 1700.f;
			Starter.CoastDrag = 1500.f;
			Starter.SetTime = 2.5f;
			Starter.TrimTime = 3.f;
			Starter.SailBelly = 70.f;
			Starter.MaxRudderAngle = 35.f;
			Starter.RudderRate = 50.f;
			Starter.RestTurnTorque = 1500.f;
			Starter.BarrelSlots = 10;
			Starter.CrateSlots = 10;
			T.Add(Starter);
			return T;
		}();
		return Table;
	}

	const FSaltBoatStats& Boat(FName Id)
	{
		const TArray<FSaltBoatStats>& All = Boats();
		const FSaltBoatStats* Found = All.FindByPredicate([Id](const FSaltBoatStats& S) { return S.Id == Id; });
		return Found ? *Found : All[0];
	}

	const FSaltPlayerStats& Player()
	{
		static const FSaltPlayerStats Stats;
		return Stats;
	}
}

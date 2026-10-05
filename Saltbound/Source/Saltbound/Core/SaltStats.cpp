// Saltbound - the stats tables (D59). See SaltStats.h and Docs/Stats.md.

#include "Core/SaltStats.h"

namespace SaltStats
{
	const TArray<FSaltBoatStats>& Boats()
	{
		static const TArray<FSaltBoatStats> Table = []()
		{
			TArray<FSaltBoatStats> T;
			// The first boat: a 10 m gaff-rigged double-ender, 10.9 t, steered with an oar.
			FSaltBoatStats Starter;
			Starter.Id = TEXT("DoubleEnder");
			Starter.Name = NSLOCTEXT("SaltStats", "DoubleEnder", "Double-ender");
			Starter.SailThrust = 10000.f;   // D59 (owner: "a bit faster"): about 8.4 kn on a reach (was 7800 N, 7.4 kn)
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

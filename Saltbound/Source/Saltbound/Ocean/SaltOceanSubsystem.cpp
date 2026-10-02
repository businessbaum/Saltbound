// Saltbound - world access point for water queries.

#include "Ocean/SaltOceanSubsystem.h"

#include "EngineUtils.h"
#include "Ocean/SaltOcean.h"

void USaltOceanSubsystem::RegisterOcean(ASaltOcean* Ocean)
{
	ActiveOcean = Ocean;
}

void USaltOceanSubsystem::UnregisterOcean(ASaltOcean* Ocean)
{
	if (ActiveOcean.Get() == Ocean)
	{
		ActiveOcean.Reset();
	}
}

ASaltOcean* USaltOceanSubsystem::GetOcean() const
{
	if (!ActiveOcean.IsValid())
	{
		// Editor worlds and early queries: find one lazily.
		for (TActorIterator<ASaltOcean> It(GetWorld()); It; ++It)
		{
			const_cast<USaltOceanSubsystem*>(this)->ActiveOcean = *It;
			break;
		}
	}
	return ActiveOcean.Get();
}

FSaltWaterSample USaltOceanSubsystem::SampleWater(FVector2D WorldXY) const
{
	if (const ASaltOcean* Ocean = GetOcean())
	{
		return Ocean->SampleSurface(WorldXY);
	}
	FSaltWaterSample Flat;
	Flat.Location = FVector(WorldXY.X, WorldXY.Y, 0.0);
	return Flat;
}

// Saltbound - the named places of the world (D55).

#include "Core/SaltPlaces.h"

#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Saltbound.h"

const TArray<FSaltPlace>& SaltPlaces::All()
{
	// Keep in sync with the world (Tools/ue_scripts/create_visual_test_map.py), the charts (Tools/textures/gen_ui.py) and
	// the landfall banners (LANDFALL in Tools/textures/ui_parts.js).
	static const TArray<FSaltPlace> Places = {
		{ TEXT("SaltboundCove"), NSLOCTEXT("Saltbound", "PlaceCove", "Saltbound Cove"), FVector2D(0.0, 0.0), FVector2D::ZeroVector, 0.f, false },
		// A sea stack with a ruined tower (SM_Landmark_Spire, about 28 m across at its foot); since the home island (D60)
		// further out, so walking its north beach does not count as landfall there.
		{ TEXT("Spire"), NSLOCTEXT("Saltbound", "PlaceSpire", "The Spire"), FVector2D(560.0, 260.0), FVector2D(28.0, 28.0), 0.f, true },
		// Ruin Isle (D47): its coast radius is 55 m, give or take a third.
		{ TEXT("RuinIsle"), NSLOCTEXT("Saltbound", "PlaceRuinIsle", "Ruin Isle"), FVector2D(1600.0, 600.0), FVector2D(55.0, 55.0), 0.f, true },
		// The far islands: the charts' ellipses.
		{ TEXT("Mistholm"), NSLOCTEXT("Saltbound", "PlaceMistholm", "Mistholm"), FVector2D(5500.0, 2500.0), FVector2D(159.0, 130.0), 20.f, true },
		{ TEXT("GullRock"), NSLOCTEXT("Saltbound", "PlaceGullRock", "Gull Rock"), FVector2D(7000.0, -1500.0), FVector2D(134.0, 119.0), 140.f, true },
		{ TEXT("Longshoal"), NSLOCTEXT("Saltbound", "PlaceLongshoal", "Longshoal"), FVector2D(4200.0, -4800.0), FVector2D(223.0, 87.0), 75.f, true },
		{ TEXT("StormWaters"), NSLOCTEXT("Saltbound", "PlaceStorm", "Storm waters"), FVector2D(0.0, -3000.0), FVector2D::ZeroVector, 0.f, false },
	};
	return Places;
}

const FSaltPlace* SaltPlaces::Find(FName Id)
{
	return All().FindByPredicate([Id](const FSaltPlace& P) { return P.Id == Id; });
}

float SaltPlaces::DistanceToShoreM(const FSaltPlace& Place, const FVector2D& WorldM)
{
	const FVector2D D = WorldM - Place.CentreM;
	const float Dist = D.Size();
	if (Place.ShoreM.X <= 0.f || Place.ShoreM.Y <= 0.f)
	{
		return Dist;
	}
	// The ellipse's radius towards the point.
	const float Yaw = FMath::DegreesToRadians(Place.ShoreYawDeg);
	const float C = FMath::Cos(Yaw), S = FMath::Sin(Yaw);
	const float Lx = D.X * C + D.Y * S, Ly = -D.X * S + D.Y * C;
	const float A = FMath::Atan2(Ly, Lx);
	const float R = Place.ShoreM.X * Place.ShoreM.Y
		/ FMath::Max(FMath::Sqrt(FMath::Square(Place.ShoreM.Y * FMath::Cos(A)) + FMath::Square(Place.ShoreM.X * FMath::Sin(A))), 1.f);
	return Dist - R;
}

FText SaltPlaces::FromHome(const FSaltPlace& Place)
{
	static const TCHAR* Points[] = {
		TEXT("north"), TEXT("north-north-east"), TEXT("north-east"), TEXT("east-north-east"), TEXT("east"), TEXT("east-south-east"),
		TEXT("south-east"), TEXT("south-south-east"), TEXT("south"), TEXT("south-south-west"), TEXT("south-west"), TEXT("west-south-west"),
		TEXT("west"), TEXT("west-north-west"), TEXT("north-west"), TEXT("north-north-west"),
	};
	const FSaltPlace* Home = Find(TEXT("SaltboundCove"));
	const FVector2D D = Place.CentreM - (Home ? Home->CentreM : FVector2D::ZeroVector);
	// North is +X, east +Y: the bearing is clockwise from north.
	const float Bearing = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) + 360.f, 360.f);
	const int32 Point = FMath::RoundToInt(Bearing / 22.5f) % 16;
	const float Metres = D.Size();
	const FString Distance = Metres < 1000.f ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(Metres / 10.f) * 10)
		: FString::Printf(TEXT("%.1f km"), Metres / 1000.f);
	return FText::Format(NSLOCTEXT("Saltbound", "LandfallFromHome", "{0} {1} of {2}"), FText::FromString(Distance), FText::FromString(Points[Point]),
		Home ? Home->Name : FText::GetEmpty());
}

TStatId USaltLandfallSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltLandfallSubsystem, STATGROUP_Tickables);
}

bool USaltLandfallSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USaltLandfallSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Accumulated += DeltaTime;
	if (Accumulated < 0.25f)
	{
		return;
	}
	Accumulated = 0.f;
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		CheckAt(Pawn->GetActorLocation());
	}
}

FName USaltLandfallSubsystem::CheckAt(const FVector& WorldCm)
{
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld());
	if (!Saves)
	{
		return NAME_None;
	}
	const FVector2D WorldM(WorldCm.X / 100.0, WorldCm.Y / 100.0);
	for (const FSaltPlace& P : SaltPlaces::All())
	{
		if (P.bIsland && !Saves->IsFound(P.Id) && SaltPlaces::DistanceToShoreM(P, WorldM) < SaltPlaces::DiscoverDistanceM)
		{
			Saves->MarkFound(P.Id);
			UE_LOG(LogSaltbound, Display, TEXT("SB_LANDFALL %s at (%.0f, %.0f) m"), *P.Id.ToString(), WorldM.X, WorldM.Y);
			OnLandfall.Broadcast(P.Id);
			return P.Id;
		}
	}
	return NAME_None;
}

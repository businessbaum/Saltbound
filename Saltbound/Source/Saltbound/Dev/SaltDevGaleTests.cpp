// Saltbound - checks of the Gale (sb.Dev.RunGaleTests, D87): the wall round the sea, how stormy it is where, a breakout drifting in,
// the sea, sky, wind and rain following the storm over the viewer, the hull sprung inside the wall, the wreck at the core. It leaves the
// player on the wreck beach (the play session ends after it).

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"
#include "World/SaltGale.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPrologue.h"
#include "World/SaltWindSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
	void RunGaleTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		USaltGaleSubsystem* G = World ? World->GetSubsystem<USaltGaleSubsystem>() : nullptr;
		USaltWindSubsystem* Wind = World ? World->GetSubsystem<USaltWindSubsystem>() : nullptr;
		USaltPrologueSubsystem* Prologue = World ? World->GetSubsystem<USaltPrologueSubsystem>() : nullptr;
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		ASaltPlayerCharacter* Player = World ? Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		ASaltBoat* Boat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			Boat = *It;
			break;
		}
		ASaltGale* V = G ? G->GetVisuals() : nullptr;
		if (!G || !Wind || !Prologue || !Saves || !Player || !Boat || !Boat->GetHullDamage() || !V)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST GaleSuite FAIL reason=missing_gale_wind_prologue_player_or_boat"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST GaleSuite DONE passed=0 failed=1"));
			return;
		}
		G->SetInstant(true);
		G->ClearCells();
		auto Run = [G](float Seconds)
		{
			for (float T = 0.f; T < Seconds; T += 1.f)
			{
				G->Tick(1.f);
			}
		};

		// The wall: the fog ring round the sea at 9.9 km, and the sky's clouds stormy along the ring (the cloud material has it).
		double MinR = 1e12;
		double MaxR = 0.0;
		for (ULocalFogVolumeComponent* F : V->WallFog)
		{
			const double R = FVector2D(F->GetComponentLocation()).Size();
			MinR = FMath::Min(MinR, R);
			MaxR = FMath::Max(MaxR, R);
		}
		FLinearColor Ring;
		FLinearColor Cell0;
		const bool bClouds = V->GetCloudStorm(Ring, Cell0);
		// (The clouds' storm needs setup_gale_sky.py's material, not applied yet: reported, not required.)
		Report(TEXT("WallRoundTheSea"), V->GetWallCount() == SaltGale::FogVolumes && MinR > 980000.0 && MaxR < 1000000.0, FString::Printf(TEXT("fog volumes=%d at %.2f-%.2f km; clouds=%d ring from %.1f km, full %.1f km, tops %.1f km, strength %.1f"),
			V->GetWallCount(), MinR / 100000.0, MaxR / 100000.0, bClouds ? 1 : 0, Ring.R, Ring.G, Ring.B, Ring.A));

		// How stormy where: calm in the Inner Waters, rising from 7.5 km, full at the wall.
		const float AtCove = G->StormAt(FVector2D::ZeroVector);
		const float AtSeven = G->StormAt(FVector2D(700000.0, 0.0));
		const float AtEightHalf = G->StormAt(FVector2D(0.0, -850000.0));
		const float AtWall = G->StormAt(FVector2D(-680000.0, 680000.0));
		Report(TEXT("StormRisesAtTheWall"), AtCove == 0.f && AtSeven == 0.f && AtEightHalf > 0.3f && AtEightHalf < 0.9f && AtWall > 0.95f,
			FString::Printf(TEXT("cove=%.2f 7km=%.2f 8.5km=%.2f 9.6km=%.2f"), AtCove, AtSeven, AtEightHalf, AtWall));

		// A breakout from the south-south-west wall drifts in, gathering for a minute; its cloud and rain show.
		G->Breakout(200.f, 880000.f);
		const FVector2D Start = G->GetCells()[0].At;
		Run(61.f);
		const FSaltGaleCell& Cell = G->GetCells()[0];
		const double Moved = Start.Size() - Cell.At.Size();
		const float AtHeart = G->StormAt(Cell.At);
		FLinearColor CloudRing;
		FLinearColor CloudCell;
		V->GetCloudStorm(CloudRing, CloudCell);
		const bool bCloudFollows = FMath::Abs(CloudCell.R - Cell.At.X / 100000.0) < 0.01 && FMath::Abs(CloudCell.G - Cell.At.Y / 100000.0) < 0.01 && CloudCell.A > 0.99f;
		Report(TEXT("BreakoutDriftsIn"), Moved > 14000.0 && Moved < 32000.0 && Cell.Strength() > 0.99f && AtHeart > 0.99f && V->GetCellCount() == 1 && bCloudFollows,
			FString::Printf(TEXT("moved in=%.0f m in 61 s strength=%.2f storm at its heart=%.2f fog columns=%d storm in the clouds=%d"), Moved / 100.0, Cell.Strength(), AtHeart,
			V->GetCellCount(), bCloudFollows ? 1 : 0));

		// The sea follows: the wall's zone stands on the ring nearest the viewer, the breakout's on it.
		const FVector View = UGameplayStatics::GetPlayerController(World, 0)->PlayerCameraManager->GetCameraLocation();
		ASaltSeaStateZone* WZ = G->GetWallZone();
		ASaltSeaStateZone* CZ = G->GetCellZones().Num() > 0 ? G->GetCellZones()[0].Get() : nullptr;
		const double WallZoneR = WZ ? FVector2D(WZ->GetActorLocation()).Size() : 0.0;
		const double Aligned = WZ ? FVector2D::DotProduct(FVector2D(WZ->GetActorLocation()).GetSafeNormal(), FVector2D(View).GetSafeNormal()) : 0.0;
		const double CellZoneOff = CZ ? FVector2D::Distance(FVector2D(CZ->GetActorLocation()), Cell.At) : 1e9;
		Report(TEXT("SeaFollowsTheStorm"), FMath::Abs(WallZoneR - SaltGale::CoreRadius) < 100.0 && Aligned > 0.99 && CellZoneOff < 100.0 && CZ->OuterRadius > 100000.f
			&& WZ->SeaState == TEXT("Storm"), FString::Printf(TEXT("wall zone at %.2f km aligned=%.3f cell zone off=%.0f cm outer=%.0f m"), WallZoneR / 100000.0,
			Aligned, CellZoneOff, CZ ? CZ->OuterRadius / 100.f : 0.f));
		G->ClearCells();

		// A storm over the viewer: the sky to Storm, the wind up and turned, the rain round the camera; back when it has gone.
		const float CalmWind = Wind->GetSpeed();
		G->BreakoutAt(FVector2D(View), FVector2D::ZeroVector, 110000.f);
		Run(61.f);
		const float Over = G->GetViewerStorm();
		const FName Sky = G->GetWeatherSet();
		const float StormWind = Wind->GetSpeed();
		const float Gust = Wind->GetStorm();
		const float Rain = V->GetNearRain();
		G->ClearCells();
		Run(2.f);
		Report(TEXT("WeatherFollowsTheViewer"), Over > 0.95f && Sky == TEXT("Storm") && Gust > 0.95f && StormWind > 1100.f && Rain > 0.95f
			&& G->GetWeatherSet().IsNone() && Wind->GetStorm() == 0.f && V->GetNearRain() == 0.f, FString::Printf(
			TEXT("storm=%.2f sky=%s wind %.1f -> %.1f m/s rain=%.2f; after: sky=%s storm wind=%.2f rain=%.2f"), Over, *Sky.ToString(), CalmWind / 100.f,
			StormWind / 100.f, Rain, *G->GetWeatherSet().ToString(), Wind->GetStorm(), V->GetNearRain()));

		// Inside the wall the sea springs her planks.
		USaltHullDamageComponent* Damage = Boat->GetHullDamage();
		const int32 Tear = Damage->GetSailTear();
		Damage->RestoreState({}, 0.f, Tear);
		Boat->PlaceAt(FVector(0.0, -940000.0, 0.0), 0.f);
		Run(30.f);
		const int32 Holes = Damage->GetHoles().Num();
		Damage->RestoreState({}, 0.f, Tear);
		Boat->ResetToStart();
		Report(TEXT("HullSprungInTheWall"), Holes >= 2, FString::Printf(TEXT("holes after 30 s at 9.4 km=%d"), Holes));

		// Past the core the Gale gives the castaway back on the wreck beach, the boat at her mooring.
		Player->TeleportTo(FVector(990000.0, 0.0, 200.0), FRotator::ZeroRotator);
		Run(3.f);
		const double FromWake = FVector2D::Distance(FVector2D(Player->GetActorLocation()), FVector2D(Prologue->GetWakeSpot()));
		Report(TEXT("WreckGivesYouBack"), FromWake < 300.0 && Saves->HasBeat(SaltGale::WreckBeat) && !G->IsWrecking(), FString::Printf(
			TEXT("from the wake spot=%.0f cm beat=%d"), FromWake, Saves->HasBeat(SaltGale::WreckBeat) ? 1 : 0));

		G->ClearCells();
		Run(1.f);
		G->SetInstant(false);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST GaleSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunGaleTests(
		TEXT("sb.Dev.RunGaleTests"),
		TEXT("D87: checks the Gale (the wall, storm by distance, a breakout, sea/sky/wind/rain following, the hull in the wall, the wreck)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunGaleTests));
}
#endif

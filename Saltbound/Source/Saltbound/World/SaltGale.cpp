// Saltbound - the Gale (D87).

#include "World/SaltGale.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPrologue.h"
#include "World/SaltWindSubsystem.h"

namespace
{
	const TCHAR* GaleDir = TEXT("/Game/Saltbound/Environment/Sky/Gale/");

	UStaticMesh* GaleMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s%s.%s"), GaleDir, Name, Name));
	}

	UMaterialInterface* GaleMaterial(const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s%s.%s"), GaleDir, Name, Name));
	}

	float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Breakouts: how they move and last. */
	constexpr float CellSpeedMin = 300.f;    // cm/s (6 kn)
	constexpr float CellSpeedMax = 500.f;    // cm/s (10 kn)
	constexpr float CellRadiusMin = 90000.f;
	constexpr float CellRadiusMax = 130000.f;
	constexpr int32 MaxCells = 3;
	/** The home island shelters the cove: a breakout that comes this close dies away (cm). */
	constexpr double ShelterRadius = 150000.0;

	FAutoConsoleCommandWithWorldAndArgs CmdGale(
		TEXT("sb.Gale"),
		TEXT("D87: the Gale. sb.Gale (print) | sb.Gale breakout [bearing deg] [km] | sb.Gale here [young] (a breakout over the viewer) | "
			 "sb.Gale near [bearing deg] [km] (a grown one to look at) | sb.Gale clear | sb.Gale strike | sb.Gale wreck | "
			 "sb.Gale look <fog density> <fog grey> <fog height km> <cloud storm> <coverage boost> <tops km> [falloff]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltGaleSubsystem* G = World ? World->GetSubsystem<USaltGaleSubsystem>() : nullptr;
			if (!G)
			{
				return;
			}
			const FString What = Args.Num() > 0 ? Args[0] : FString();
			APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
			const FVector P = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
			if (What == TEXT("breakout"))
			{
				G->Breakout(Args.Num() > 1 ? FCString::Atof(*Args[1]) : -1.f, Args.Num() > 2 ? FCString::Atof(*Args[2]) * 100000.f : -1.f);
			}
			else if (What == TEXT("here"))
			{
				// Over the viewer, already grown (captures; 'young' lets it gather).
				const bool bYoung = Args.Num() > 1 && Args[1] == TEXT("young");
				G->BreakoutAt(FVector2D(P) + FVector2D(30000.0, 0.0), FVector2D(-120.0, 60.0), 110000.f, bYoung ? 0.f : 61.f);
			}
			else if (What == TEXT("near"))
			{
				// Grown, about 3 km away on a bearing (default south-west), drifting across: to look at from the cove.
				const float B = FMath::DegreesToRadians(Args.Num() > 1 ? FCString::Atof(*Args[1]) : 220.f);
				const float D = (Args.Num() > 2 ? FCString::Atof(*Args[2]) : 3.f) * 100000.f;
				G->BreakoutAt(FVector2D(P) + FVector2D(FMath::Cos(B), FMath::Sin(B)) * D, FVector2D(FMath::Sin(B), -FMath::Cos(B)) * 300.f, 110000.f, 61.f);
			}
			else if (What == TEXT("clear"))
			{
				G->ClearCells();
			}
			else if (What == TEXT("strike") && G->GetVisuals())
			{
				const float A = FMath::FRandRange(0.f, 2.f * PI);
				G->GetVisuals()->Strike(P + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 120000.f, 60000.f);
			}
			else if (What == TEXT("wreck"))
			{
				G->Wreck();
			}
			else if (What == TEXT("look") && G->GetVisuals())
			{
				// sb.Gale look <fog density> <fog grey> <fog height km> <cloud storm 0..1> <coverage boost> <tops km> [falloff]
				FSaltGaleLook L = G->GetVisuals()->GetLook();
				float* Fields[] = { &L.FogDensity, &L.FogGrey, &L.FogHeightKm, &L.CloudStorm, &L.CoverageBoost, &L.TopsKm, &L.FogFalloff };
				for (int32 I = 1; I < Args.Num() && I <= 7; ++I)
				{
					*Fields[I - 1] = FCString::Atof(*Args[I]);
				}
				G->GetVisuals()->SetLook(L);
			}
			FString Line;
			for (const FSaltGaleCell& C : G->GetCells())
			{
				Line += FString::Printf(TEXT(" [#%d at (%.1f, %.1f) km r=%.1f km strength=%.2f age=%.0f/%.0f s]"), C.Id, C.At.X / 100000.0,
					C.At.Y / 100000.0, C.Radius / 100000.f, C.Strength(), C.Age, C.Life);
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_GALE viewer storm=%.2f at %.2f km, cells=%d%s"), G->GetViewerStorm(), FVector2D(P).Size() / 100000.0,
				G->GetCells().Num(), *Line);
		}));
}

// --- A breakout ------------------------------------------------------------------------------------------------------

float FSaltGaleCell::Strength() const
{
	return FMath::Min(Smooth(0.f, 60.f, Age), 1.f - Smooth(Life - 120.f, Life, Age));
}

// --- The visuals ---------------------------------------------------------------------------------------------------

ASaltGale::ASaltGale()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASaltGale::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	// The wall's foot: fog volumes round the sea, each 3.2 km across, about 1.3 km apart, so they melt into one veil of dark rain and
	// fog low over the sea under the storm clouds. Thin: a dense one turned the whole horizon into a flat sunlit screen.
	auto Fog = [this](const FName Name)
	{
		ULocalFogVolumeComponent* F = NewObject<ULocalFogVolumeComponent>(this, Name);
		F->SetupAttachment(GetRootComponent());
		F->RegisterComponent();
		return F;
	};
	for (int32 I = 0; I < SaltGale::FogVolumes; ++I)
	{
		const double Rad = 2.0 * UE_DOUBLE_PI * I / SaltGale::FogVolumes;
		ULocalFogVolumeComponent* F = Fog(*FString::Printf(TEXT("GaleFog%d"), I));
		F->SetWorldLocation(FVector(FMath::Cos(Rad) * SaltGale::FogRadius, FMath::Sin(Rad) * SaltGale::FogRadius, 0.0));
		F->SetFogPhaseG(0.1f);
		WallFog.Add(F);
	}
	// A breakout's fog column under its clouds (sized and faded with it).
	for (int32 I = 0; I < 3; ++I)
	{
		ULocalFogVolumeComponent* F = Fog(*FString::Printf(TEXT("GaleCellFog%d"), I));
		F->SetRadialFogExtinction(0.f);
		F->SetHeightFogExtinction(0.f);
		F->SetHeightFogFalloff(500.f);
		F->SetHeightFogOffset(-0.5f);
		F->SetFogAlbedo(FLinearColor(0.3f, 0.32f, 0.36f));
		F->SetVisibility(false);
		CellFog.Add(F);
	}

	// Lightning near the viewer: one bolt at a time, its flash a big light without shadows (the clouds flicker on their own).
	BoltMeshes = { GaleMesh(TEXT("SM_Gale_Bolt_A")), GaleMesh(TEXT("SM_Gale_Bolt_B")), GaleMesh(TEXT("SM_Gale_Bolt_C")) };
	Bolt = NewObject<UStaticMeshComponent>(this, TEXT("GaleBolt"));
	Bolt->SetupAttachment(GetRootComponent());
	Bolt->SetStaticMesh(BoltMeshes[0]);
	Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bolt->SetCastShadow(false);
	Bolt->SetVisibility(false);
	Bolt->RegisterComponent();
	if (UMaterialInterface* M = GaleMaterial(TEXT("MI_Gale_Bolt")))
	{
		BoltMID = UMaterialInstanceDynamic::Create(M, this);
		Bolt->SetMaterial(0, BoltMID);
	}
	FlashLight = NewObject<UPointLightComponent>(this, TEXT("GaleFlash"));
	FlashLight->SetupAttachment(GetRootComponent());
	FlashLight->SetCastShadows(false);
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(0.f);
	FlashLight->SetAttenuationRadius(350000.f);
	FlashLight->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
	FlashLight->SetVisibility(false);
	FlashLight->RegisterComponent();

	// The rain round the camera: the unit shaft (1 m), 6 m round the eye and 30 m high.
	NearRain = NewObject<UStaticMeshComponent>(this, TEXT("GaleNearRain"));
	NearRain->SetupAttachment(GetRootComponent());
	NearRain->SetStaticMesh(GaleMesh(TEXT("SM_Gale_RainShaft")));
	NearRain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NearRain->SetCastShadow(false);
	NearRain->SetWorldScale3D(FVector(6.f, 6.f, 30.f));
	NearRain->SetVisibility(false);
	NearRain->SetTranslucentSortPriority(2);
	NearRain->RegisterComponent();
	if (UMaterialInterface* M = GaleMaterial(TEXT("MI_Gale_RainNear")))
	{
		NearRainMID = UMaterialInstanceDynamic::Create(M, this);
		NearRain->SetMaterial(0, NearRainMID);
	}
	SetLook(WallLook);
	PushClouds({});
	FLinearColor Ring;
	FLinearColor Cell0;
	const bool bClouds = GetCloudStorm(Ring, Cell0);
	UE_LOG(LogSaltbound, Display, TEXT("SB_GALE built fog=%d clouds=%d ring=(%.1f, %.1f km, tops %.1f km, %.1f) bolt=%d rain=%d"), WallFog.Num(), bClouds ? 1 : 0,
		Ring.R, Ring.G, Ring.B, Ring.A, BoltMeshes[0] ? 1 : 0, NearRainMID ? 1 : 0);
}

UMaterialInstanceDynamic* ASaltGale::CloudMaterial() const
{
	for (TActorIterator<ASaltLightingRig> It(GetWorld()); It; ++It)
	{
		if (const UVolumetricCloudComponent* C = It->FindComponentByClass<UVolumetricCloudComponent>())
		{
			return Cast<UMaterialInstanceDynamic>(C->GetMaterial());
		}
	}
	return nullptr;
}

bool ASaltGale::GetCloudStorm(FLinearColor& OutRing, FLinearColor& OutCell0) const
{
	UMaterialInstanceDynamic* M = CloudMaterial();
	if (!M)
	{
		return false;
	}
	const FHashedMaterialParameterInfo Ring(TEXT("Gale_Ring"));
	const FHashedMaterialParameterInfo Cell(TEXT("Gale_Cell0"));
	return M->GetVectorParameterValue(Ring, OutRing) && M->GetVectorParameterValue(Cell, OutCell0);
}

void ASaltGale::PushClouds(const TArray<FSaltGaleCell>& Cells)
{
	UMaterialInstanceDynamic* M = CloudMaterial();
	if (!M)
	{
		return;
	}
	// The ring: the storm from the wall's face, full 700 m on, its tops as high as the look says (setup_gale_sky.py).
	M->SetVectorParameterValue(TEXT("Gale_Ring"), FLinearColor(SaltGale::FaceRadius / 100000.f, (SaltGale::FaceRadius + 70000.f) / 100000.f, WallLook.TopsKm,
		WallLook.CloudStorm));
	M->SetScalarParameterValue(TEXT("Gale_CoverageBoost"), WallLook.CoverageBoost);
	for (int32 I = 0; I < 3; ++I)
	{
		const FName Name(*FString::Printf(TEXT("Gale_Cell%d"), I));
		if (Cells.IsValidIndex(I))
		{
			const FSaltGaleCell& C = Cells[I];
			M->SetVectorParameterValue(Name, FLinearColor(C.At.X / 100000.f, C.At.Y / 100000.f, C.Radius * 1.2f / 100000.f, C.Strength()));
		}
		else
		{
			M->SetVectorParameterValue(Name, FLinearColor(0.f, 0.f, 1.f, 0.f));
		}
	}
}

void ASaltGale::SetLook(const FSaltGaleLook& Look)
{
	WallLook = Look;
	const float Base = ULocalFogVolumeComponent::GetBaseVolumeSize();
	for (ULocalFogVolumeComponent* F : WallFog)
	{
		F->SetWorldScale3D(FVector(160000.f, 160000.f, Look.FogHeightKm * 100000.f) / Base);
		F->SetRadialFogExtinction(Look.FogDensity);
		F->SetHeightFogExtinction(Look.FogDensity);
		F->SetHeightFogFalloff(Look.FogFalloff);
		F->SetFogAlbedo(FLinearColor(Look.FogGrey, Look.FogGrey * 1.05f, Look.FogGrey * 1.15f));
		F->SetVisibility(Look.FogDensity > 0.f);
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_GALE look fog=%.2f grey=%.2f height=%.1f km falloff=%.0f clouds=%.2f boost=%.2f tops=%.1f km"), Look.FogDensity,
		Look.FogGrey, Look.FogHeightKm, Look.FogFalloff, Look.CloudStorm, Look.CoverageBoost, Look.TopsKm);
}

int32 ASaltGale::GetCellCount() const
{
	int32 N = 0;
	for (const ULocalFogVolumeComponent* F : CellFog)
	{
		N += F && F->IsVisible() ? 1 : 0;
	}
	return N;
}

void ASaltGale::ShowCells(const TArray<FSaltGaleCell>& Cells)
{
	const float Base = ULocalFogVolumeComponent::GetBaseVolumeSize();
	for (int32 I = 0; I < CellFog.Num(); ++I)
	{
		ULocalFogVolumeComponent* F = CellFog[I];
		if (!Cells.IsValidIndex(I))
		{
			F->SetVisibility(false);
			continue;
		}
		const FSaltGaleCell& C = Cells[I];
		const float S = C.Strength();
		// A column of rain and fog under the storm, as wide as its heart and up to the clouds.
		const float R = C.Radius * 0.75f * (0.4f + 0.6f * S);
		F->SetWorldLocation(FVector(C.At.X, C.At.Y, 0.0));
		F->SetWorldScale3D(FVector(R, R, WallLook.FogHeightKm * 100000.f) / Base);
		F->SetRadialFogExtinction(WallLook.FogDensity * 2.f * S);
		F->SetHeightFogExtinction(WallLook.FogDensity * 2.f * S);
		F->SetFogAlbedo(FLinearColor(WallLook.FogGrey, WallLook.FogGrey * 1.05f, WallLook.FogGrey * 1.15f));
		F->SetVisibility(S > 0.01f);
	}
	PushClouds(Cells);
}

float ASaltGale::Flicker(float Age, float Length)
{
	// A strike: a hard first flash, a dimmer return stroke or two, gone.
	if (Age >= Length)
	{
		return 0.f;
	}
	const float Fade = 1.f - Age / Length;
	const float Strokes = (Age < 0.06f ? 1.f : 0.f) + (Age > 0.12f && Age < 0.18f ? 0.7f : 0.f) + (Age > 0.3f && Age < 0.34f ? 0.5f : 0.f);
	return FMath::Clamp(Strokes + 0.15f * Fade, 0.f, 1.f);
}

void ASaltGale::Strike(const FVector& At, float CloudBaseCm)
{
	if (!Bolt || BoltMeshes.Num() == 0)
	{
		return;
	}
	// The bolts are 1.5-2.3 km long; scale one to reach from the cloud's base to the sea.
	UStaticMesh* M = BoltMeshes[FMath::RandRange(0, BoltMeshes.Num() - 1)];
	Bolt->SetStaticMesh(M);
	const float Length = M ? FMath::Max(M->GetBounds().BoxExtent.Z * 2.f, 1.f) : 190000.f;
	const float S = FMath::Clamp(CloudBaseCm / Length, 0.15f, 1.5f);
	Bolt->SetWorldTransform(FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), FVector(At.X, At.Y, 0.0),
		FVector(FMath::Sqrt(S), FMath::Sqrt(S), S)));
	Bolt->SetVisibility(true);
	BoltAge = 0.f;
	BoltLeft = 0.45f;
	FlashLight->SetWorldLocation(FVector(At.X, At.Y, CloudBaseCm * 0.6f));
	FlashLight->SetVisibility(true);
}

void ASaltGale::SetNearRain(float Amount, const FVector& View)
{
	NearRainAmount = Amount;
	if (!NearRain)
	{
		return;
	}
	NearRain->SetVisibility(Amount > 0.01f);
	if (Amount > 0.01f)
	{
		NearRain->SetWorldLocation(View - FVector(0.0, 0.0, 1500.0));
		if (NearRainMID)
		{
			NearRainMID->SetScalarParameterValue(TEXT("Rain"), Amount);
		}
	}
}

void ASaltGale::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BoltLeft > 0.f)
	{
		BoltAge += DeltaSeconds;
		BoltLeft -= DeltaSeconds;
		const float V = Flicker(BoltAge, 0.45f);
		if (BoltMID)
		{
			BoltMID->SetScalarParameterValue(TEXT("Flash"), V);
		}
		FlashLight->SetIntensity(V * 4.0e8f);
		if (BoltLeft <= 0.f)
		{
			Bolt->SetVisibility(false);
			FlashLight->SetVisibility(false);
		}
	}
}

// --- The Gale ---------------------------------------------------------------------------------------------------------

bool USaltGaleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USaltGaleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltGaleSubsystem, STATGROUP_Tickables);
}

void USaltGaleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The open sea's map only (the test maps have no Gale).
	if (!InWorld.GetMapName().EndsWith(TEXT("L_VisualTest")))
	{
		return;
	}
	bActive = true;
	SpawnVisuals();
	FRandomStream R(FPlatformTime::Cycles());
	NextBreakoutIn = R.FRandRange(360.f, 600.f);
}

void USaltGaleSubsystem::SpawnVisuals()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters P;
	P.Name = TEXT("SaltGale");
	Visuals = World->SpawnActor<ASaltGale>(ASaltGale::StaticClass(), FTransform::Identity, P);
	if (Visuals)
	{
		Visuals->Build();
	}
	// The sea follows the viewer (ASaltSeaStateZone): one zone for the wall, kept at the ring's point nearest the viewer, one for
	// each breakout.
	auto Zone = [World](const TCHAR* Name)
	{
		FActorSpawnParameters ZP;
		ZP.Name = Name;
		ASaltSeaStateZone* Z = World->SpawnActor<ASaltSeaStateZone>(ASaltSeaStateZone::StaticClass(), FTransform::Identity, ZP);
		if (Z)
		{
			Z->SeaState = TEXT("Storm");
			Z->InnerRadius = -2.f;
			Z->OuterRadius = -1.f;
		}
		return Z;
	};
	WallZone = Zone(TEXT("SeaStateZone_Gale"));
	for (int32 I = 0; I < MaxCells; ++I)
	{
		CellZones.Add(Zone(*FString::Printf(TEXT("SeaStateZone_GaleCell%d"), I)));
	}
}

float USaltGaleSubsystem::WallAt(const FVector2D& XY) const
{
	return Smooth(SaltGale::StormStart, SaltGale::StormFull, XY.Size());
}

float USaltGaleSubsystem::StormAt(const FVector2D& XY) const
{
	float S = WallAt(XY);
	for (const FSaltGaleCell& C : Cells)
	{
		const float D = FVector2D::Distance(XY, C.At);
		S = FMath::Max(S, (1.f - Smooth(C.Radius * 0.55f, C.Radius * 1.5f, D)) * C.Strength());
	}
	return S;
}

int32 USaltGaleSubsystem::Breakout(float BearingDeg, float DistanceCm)
{
	FRandomStream R(FPlatformTime::Cycles());
	if (BearingDeg < 0.f)
	{
		// Most often from the south-west, where the relays fail (Islands.md 4).
		BearingDeg = R.FRand() < 0.6f ? R.FRandRange(170.f, 260.f) : R.FRandRange(0.f, 360.f);
	}
	if (DistanceCm < 0.f)
	{
		DistanceCm = SaltGale::FaceRadius - 20000.f;
	}
	const float Rad = FMath::DegreesToRadians(BearingDeg);
	const FVector2D Out(FMath::Cos(Rad), FMath::Sin(Rad));
	// Drifting in, a little to one side of the cove.
	const float Turn = FMath::DegreesToRadians(R.FRandRange(-35.f, 35.f));
	const FVector2D In = -Out.GetRotated(FMath::RadiansToDegrees(Turn));
	const int32 Id = BreakoutAt(Out * DistanceCm, In * R.FRandRange(CellSpeedMin, CellSpeedMax), R.FRandRange(CellRadiusMin, CellRadiusMax));
	Cells.Last().bFromWall = true;
	return Id;
}

int32 USaltGaleSubsystem::BreakoutAt(const FVector2D& At, const FVector2D& Velocity, float Radius, float StartAge)
{
	if (Cells.Num() >= MaxCells)
	{
		Cells.RemoveAt(0);
	}
	FSaltGaleCell C;
	C.At = At;
	C.Velocity = Velocity;
	C.Radius = Radius;
	C.Life = FMath::FRandRange(1080.f, 1800.f);
	C.Age = FMath::Clamp(StartAge, 0.f, C.Life - 130.f);
	C.Id = NextId++;
	Cells.Add(C);
	UE_LOG(LogSaltbound, Display, TEXT("SB_GALE breakout #%d at (%.1f, %.1f) km r=%.1f km moving %.1f m/s"), C.Id, At.X / 100000.0, At.Y / 100000.0,
		Radius / 100000.f, Velocity.Size() / 100.f);
	return C.Id;
}

void USaltGaleSubsystem::ClearCells()
{
	Cells.Reset();
	if (Visuals)
	{
		Visuals->ShowCells(Cells);
	}
}

void USaltGaleSubsystem::UpdateCells(float DeltaTime)
{
	for (int32 I = Cells.Num() - 1; I >= 0; --I)
	{
		FSaltGaleCell& C = Cells[I];
		C.Age += DeltaTime;
		C.At += C.Velocity * DeltaTime;
		// The cove is sheltered: a storm that reaches it dies away quickly.
		if (C.bFromWall && C.At.Size() < ShelterRadius + C.Radius && C.Age < C.Life - 120.f)
		{
			C.Life = C.Age + 120.f;
		}
		if (C.Age >= C.Life)
		{
			UE_LOG(LogSaltbound, Display, TEXT("SB_GALE breakout #%d died away at (%.1f, %.1f) km"), C.Id, C.At.X / 100000.0, C.At.Y / 100000.0);
			Cells.RemoveAt(I);
		}
	}
	if (!bInstant)
	{
		NextBreakoutIn -= DeltaTime;
		if (NextBreakoutIn <= 0.f)
		{
			NextBreakoutIn = FMath::FRandRange(480.f, 900.f);
			Breakout();
		}
	}
	if (Visuals)
	{
		Visuals->ShowCells(Cells);
	}
}

FVector USaltGaleSubsystem::ViewLocation() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC && PC->PlayerCameraManager)
	{
		return PC->PlayerCameraManager->GetCameraLocation();
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	return Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
}

void USaltGaleSubsystem::ApplyToWorld(float DeltaTime)
{
	UWorld* World = GetWorld();
	const FVector View = ViewLocation();
	const FVector2D V(View);
	ViewerStorm = StormAt(V);

	// The sea: the wall's zone at the ring point nearest the viewer (full from 9.4 km, nothing inside 7.5 km); a zone per breakout.
	if (WallZone)
	{
		const FVector2D Dir = V.Size() > 1.0 ? V.GetSafeNormal() : FVector2D(1.0, 0.0);
		WallZone->SetActorLocation(FVector(Dir * SaltGale::CoreRadius, 0.0));
		WallZone->InnerRadius = 40000.f;
		WallZone->OuterRadius = SaltGale::CoreRadius - SaltGale::StormStart;
	}
	for (int32 I = 0; I < CellZones.Num(); ++I)
	{
		ASaltSeaStateZone* Z = CellZones[I];
		if (!Z)
		{
			continue;
		}
		if (Cells.IsValidIndex(I))
		{
			const FSaltGaleCell& C = Cells[I];
			const float S = C.Strength();
			Z->SetActorLocation(FVector(C.At, 0.0));
			Z->SeaState = TEXT("Storm");
			Z->InnerRadius = C.Radius * 0.5f * S;
			Z->OuterRadius = FMath::Max(C.Radius * 1.5f * S, Z->InnerRadius + 1.f);
		}
		else
		{
			Z->InnerRadius = -2.f;
			Z->OuterRadius = -1.f;
		}
	}

	// The sky: the rig's weather follows the storm over the viewer; it goes back to what it was once the storm has passed.
	ASaltLightingRig* Rig = nullptr;
	for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
	{
		Rig = *It;
		break;
	}
	if (Rig)
	{
		const FName Want = ViewerStorm >= 0.6f ? FName(TEXT("Storm")) : ViewerStorm >= 0.35f ? FName(TEXT("Overcast"))
			: ViewerStorm >= 0.15f ? FName(TEXT("Cloudy")) : NAME_None;
		if (!Want.IsNone())
		{
			if (WeatherSet.IsNone())
			{
				BaseWeather = Rig->CurrentWeather;      // entering a storm: the weather to go back to
			}
			if (WeatherSet != Want)
			{
				Rig->SetWeather(Want, bInstant ? 0.f : 10.f);
				WeatherSet = Want;
			}
		}
		else if (!WeatherSet.IsNone())
		{
			if (!BaseWeather.IsNone())
			{
				Rig->SetWeather(BaseWeather, bInstant ? 0.f : 15.f);
			}
			WeatherSet = NAME_None;
		}
	}

	// The wind: against the boat at the wall (blowing in from it), round and in at a breakout's heart.
	if (USaltWindSubsystem* Wind = World->GetSubsystem<USaltWindSubsystem>())
	{
		float FromDeg = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
		float Best = WallAt(V);
		for (const FSaltGaleCell& C : Cells)
		{
			const float S = (1.f - Smooth(C.Radius * 0.55f, C.Radius * 1.5f, FVector2D::Distance(V, C.At))) * C.Strength();
			if (S > Best)
			{
				Best = S;
				const FVector2D ToCentre = C.At - V;
				FromDeg = FMath::RadiansToDegrees(FMath::Atan2(ToCentre.Y, ToCentre.X)) - 70.f;
			}
		}
		Wind->SetStorm(ViewerStorm, FromDeg, 1600.f);
	}

	if (Visuals)
	{
		Visuals->SetNearRain(Smooth(0.3f, 0.8f, ViewerStorm), View);
	}
}

void USaltGaleSubsystem::UpdateLightning(float DeltaTime, const FVector& View)
{
	if (!Visuals)
	{
		return;
	}
	// The clouds flicker on their own (the engine's storm clouds); bolts strike near the viewer once the storm is over them.
	StrikeIn -= DeltaTime;
	if (StrikeIn <= 0.f)
	{
		StrikeIn = FMath::FRandRange(4.f, 10.f);
		if (ViewerStorm > 0.5f)
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const float D = FMath::FRandRange(60000.f, 250000.f);
			Visuals->Strike(View + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * D, FMath::FRandRange(40000.f, 80000.f));
		}
	}
}

void USaltGaleSubsystem::UpdateWreck(float DeltaTime, const FVector& View)
{
	UWorld* World = GetWorld();
	ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Player)
	{
		return;
	}
	ASaltBoat* Boat = nullptr;
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		Boat = *It;
		break;
	}
	const double R = FVector2D(Player->GetActorLocation()).Size();
	// Inside the wall the sea springs the hull's planks, faster the deeper she goes.
	if (Boat && Boat->GetHullDamage())
	{
		const double BoatR = FVector2D(Boat->GetActorLocation()).Size();
		if (BoatR > SaltGale::FaceRadius)
		{
			const float Depth = FMath::Clamp(float((BoatR - SaltGale::FaceRadius) / (SaltGale::CoreRadius - SaltGale::FaceRadius)), 0.f, 1.f);
			HoleIn -= DeltaTime;
			if (HoleIn <= 0.f)
			{
				HoleIn = FMath::Lerp(14.f, 4.f, Depth);
				const bool bAdded = Boat->GetHullDamage()->AddHole(FMath::FRandRange(-3.5f, 3.5f), FMath::RandBool() ? 1 : -1, Depth > 0.6f ? 2 : 1);
				UE_LOG(LogSaltbound, Display, TEXT("SB_GALE the sea springs a plank (depth %.2f, added=%d)"), Depth, bAdded ? 1 : 0);
			}
		}
		else
		{
			HoleIn = 8.f;
		}
	}
	// Past the core nobody stays: two seconds there and the Gale gives the castaway back.
	if (R > SaltGale::CoreRadius && !IsWrecking())
	{
		CoreTime += DeltaTime;
		if (CoreTime > 2.f)
		{
			Wreck();
		}
	}
	else
	{
		CoreTime = 0.f;
	}
}

void USaltGaleSubsystem::Wreck()
{
	if (IsWrecking())
	{
		return;
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_GALE wreck"));
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC && PC->PlayerCameraManager && !bInstant)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 1.5f, FLinearColor::Black, false, true);
	}
	WreckIn = bInstant ? 0.f : 1.6f;
	if (bInstant)
	{
		FinishWreck();
	}
}

void USaltGaleSubsystem::FinishWreck()
{
	WreckIn = -1.f;
	CoreTime = 0.f;
	UWorld* World = GetWorld();
	ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	USaltPrologueSubsystem* Prologue = World->GetSubsystem<USaltPrologueSubsystem>();
	// The boat is found at her mooring, her holes stopped and her water out (as D74's foundering); the sail as it was.
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		if (USaltHullDamageComponent* D = It->GetHullDamage())
		{
			D->RestoreState({}, 0.f, D->GetSailTear());
		}
		It->ResetToStart();
		break;
	}
	// The castaway wakes on the wreck beach, as every castaway before them.
	if (Player && Prologue)
	{
		const FVector Spot = Prologue->GetWakeSpot();
		const FRotator Facing(0.f, SaltStory::WakeYaw, 0.f);
		Player->TeleportTo(Spot + FVector(0.0, 0.0, 120.0), Facing);
		if (AController* C = Player->GetController())
		{
			C->SetControlRotation(Facing);
		}
		if (!bInstant)
		{
			Player->StartWakeUp(6.f);
		}
		Prologue->Reach(SaltGale::WreckBeat);
	}
	if (ASaltPlayerController* SPC = Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
	{
		SPC->ShowNotice(NSLOCTEXT("SaltGale", "Wrecked", "The Gale broke her and gave you back, as it gives back every castaway. She was found at her mooring."));
	}
	ViewerStorm = StormAt(FVector2D(ViewLocation()));
}

void USaltGaleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bActive)
	{
		return;
	}
	if (WreckIn >= 0.f)
	{
		WreckIn -= DeltaTime;
		if (WreckIn < 0.f)
		{
			FinishWreck();
		}
		return;
	}
	UpdateCells(DeltaTime);
	ApplyToWorld(DeltaTime);
	const FVector View = ViewLocation();
	UpdateLightning(DeltaTime, View);
	UpdateWreck(DeltaTime, View);
}

// Saltbound - camera-following ocean surface with shared Gerstner waves.

#include "Ocean/SaltOcean.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Core/SaltContent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Ocean/SaltOceanSubsystem.h"

namespace SaltOceanParams
{
	static const FName Wave[FSaltWaveField::MaxWaves] = {
		TEXT("W0"), TEXT("W1"), TEXT("W2"), TEXT("W3"), TEXT("W4"), TEXT("W5"),
		TEXT("W6"), TEXT("W7"), TEXT("W8"), TEXT("W9"), TEXT("W10"), TEXT("W11") };
	static const FName Calm[FSaltWaveField::MaxCalmZones] = { TEXT("Calm0"), TEXT("Calm1"), TEXT("Calm2"), TEXT("Calm3") };
	static const FName Phase0123(TEXT("Phase0123"));
	static const FName Phase4567(TEXT("Phase4567"));
	static const FName Phase891011(TEXT("Phase891011"));
	static const FName PrevPhase0123(TEXT("PrevPhase0123"));
	static const FName PrevPhase4567(TEXT("PrevPhase4567"));
	static const FName PrevPhase891011(TEXT("PrevPhase891011"));
	static const FName CalmMin(TEXT("CalmMin"));
	static const FName Chop(TEXT("Chop"));
	static const FName Globals(TEXT("Globals"));
	static const FName Ripples(TEXT("RippleOffsets"));
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdOceanFreeze(
		TEXT("sb.Ocean.Freeze"),
		TEXT("Freeze ocean time (waves, ripples, foam): sb.Ocean.Freeze 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<ASaltOcean> It(World); It; ++It)
			{
				It->bFreezeTime = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !It->bFreezeTime;
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdOceanTime(
		TEXT("sb.Ocean.Time"),
		TEXT("Set ocean time in seconds (identical sea conditions for comparisons): sb.Ocean.Time <seconds>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0)
			{
				for (TActorIterator<ASaltOcean> It(World); It; ++It)
				{
					It->SetOceanTime(FCString::Atod(*Args[0]));
				}
			}
		}));
}

ASaltOcean::ASaltOcean()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	// Tick early so boats (which depend on us) see this frame's phases.
	PrimaryActorTick.bHighPriority = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeTiles = [this, Root](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetAbsolute(true, true, true); // instances are placed in world space
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetCanEverAffectNavigation(false);
		C->bAffectDistanceFieldLighting = false;
		C->bVisibleInRayTracing = false;
		C->SetNumCustomDataFloats(3);
		C->SetMobility(EComponentMobility::Movable);
		return C;
	};
	FullTiles = MakeTiles(TEXT("FullTiles"));
	QuarterTiles = MakeTiles(TEXT("QuarterTiles"));

	// Content defaults (MPC, material, tile meshes) are resolved in OnConstruction; see SaltContent.h.

	// Open-sea default, revision 5 (owner reference: Sea of Thieves; D24). A clear hierarchy of scale:
	// - three long swells from neighbouring directions interfere into irregular groups; they carry most of
	//   the height and of the horizontal (Gerstner) displacement: broad troughs, narrower crests;
	// - medium waves (21-6 m) are visible on the swell faces but clearly subordinate (ak ~0.09 vs ~0.1-0.13)
	//   and vary in patches (ChopVariation), so wave faces are not uniformly crumpled;
	// - short waves (< 4 m) are mostly shading detail.
	// Heights are for SeaState scale 1 ("Rough" uses 1.3); sum of steepness 0.995 keeps the surface free of loops.
	auto Add = [this](float Dir, float Lambda, float Amp, float Steep, float Phase)
	{
		FSaltWaveSpec S;
		S.DirectionDeg = Dir;
		S.Wavelength = Lambda;
		S.Amplitude = Amp;
		S.Steepness = Steep;
		S.PhaseOffset = Phase;
		Waves.Add(S);
	};
	Add(20.f, 7400.f, 118.f, 0.24f, 0.0f);
	Add(41.f, 5300.f, 86.f, 0.19f, 1.3f);
	Add(4.f, 3900.f, 52.f, 0.14f, 2.1f);
	Add(63.f, 2100.f, 30.f, 0.10f, 4.0f);
	Add(-22.f, 1450.f, 19.f, 0.08f, 0.7f);
	Add(31.f, 930.f, 11.f, 0.07f, 5.2f);
	Add(82.f, 610.f, 6.5f, 0.055f, 3.3f);
	Add(-41.f, 410.f, 3.6f, 0.045f, 1.9f);
	Add(13.f, 280.f, 2.2f, 0.03f, 2.8f);
	Add(54.f, 190.f, 1.3f, 0.02f, 0.4f);
	Add(-63.f, 130.f, 0.75f, 0.015f, 4.6f);
	Add(101.f, 90.f, 0.45f, 0.01f, 3.7f);
}

void ASaltOcean::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyContent();
	RebuildWaveField();
	LastSelectionView = FVector(1.0e12);
	UpdateTiles(true);
	PushParameters();
}

void ASaltOcean::ApplyContent()
{
	SaltContent::ResolveDefault<UMaterialParameterCollection>(ParameterCollection, SaltContent::OceanParameterCollection);
	SaltContent::ResolveDefault<UMaterialInterface>(Material, SaltContent::OceanMaterial);
	SaltContent::ResolveDefault<UStaticMesh>(TileMesh, SaltContent::OceanTile);
	SaltContent::ResolveDefault<UStaticMesh>(QuarterTileMesh, SaltContent::OceanQuarterTile);
	FullTiles->SetStaticMesh(TileMesh);
	QuarterTiles->SetStaticMesh(QuarterTileMesh);
	FullTiles->SetMaterial(0, Material);
	QuarterTiles->SetMaterial(0, Material);
}

void ASaltOcean::BeginPlay()
{
	Super::BeginPlay();
	// Cooked maps do not re-run construction; this covers actors saved before the defaults were resolved.
	ApplyContent();
	RebuildWaveField();
	if (USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>())
	{
		Sub->RegisterOcean(this);
	}
}

void ASaltOcean::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>())
	{
		Sub->UnregisterOcean(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ASaltOcean::RebuildWaveField()
{
	Field.Build(Waves, AmplitudeScale, SteepnessScale, CalmZones, GetActorLocation().Z);
	Field.SetChop(SwellCount, ChopVariation, ChopPatchSize);
	Field.SetTime(OceanTime);
	// Culling bounds of the displaced surface come from the tile meshes' extended bounds (set on import).
}

void ASaltOcean::SetOceanTime(double NewTime)
{
	OceanTime = NewTime;
	Field.SetTime(OceanTime);
	bHavePrevPhases = false;
	PushParameters();
}

void ASaltOcean::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Remember this frame's phases before advancing: they are next frame's "previous" phases.
	for (int32 i = 0; i < FSaltWaveField::MaxWaves; ++i)
	{
		PrevPhases[i] = Field.Waves.IsValidIndex(i) ? Field.Waves[i].Phase : 0.f;
	}
	bHavePrevPhases = true;
	if (!bFreezeTime)
	{
		OceanTime += (double)DeltaSeconds * TimeScale;
	}
	Field.WaterLevel = GetActorLocation().Z;
	Field.SetTime(OceanTime);
	PushParameters();
	UpdateTiles(false);
}

FSaltWaterSample ASaltOcean::SampleSurface(FVector2D WorldXY) const
{
	return Field.Sample(WorldXY);
}

void ASaltOcean::PushParameters()
{
	UWorld* World = GetWorld();
	if (!World || !ParameterCollection)
	{
		return;
	}
	UMaterialParameterCollectionInstance* Inst = World->GetParameterCollectionInstance(ParameterCollection);
	if (!Inst)
	{
		return;
	}
	float Phases[FSaltWaveField::MaxWaves] = {};
	for (int32 i = 0; i < FSaltWaveField::MaxWaves; ++i)
	{
		FLinearColor V(0.f, 0.f, 0.f, 0.f);
		if (Field.Waves.IsValidIndex(i))
		{
			const FSaltWaveField::FWave& W = Field.Waves[i];
			V = FLinearColor((float)W.K.X, (float)W.K.Y, W.Amplitude, W.Horizontal);
			Phases[i] = W.Phase;
		}
		Inst->SetVectorParameterValue(SaltOceanParams::Wave[i], V);
	}
	Inst->SetVectorParameterValue(SaltOceanParams::Phase0123, FLinearColor(Phases[0], Phases[1], Phases[2], Phases[3]));
	Inst->SetVectorParameterValue(SaltOceanParams::Phase4567, FLinearColor(Phases[4], Phases[5], Phases[6], Phases[7]));
	Inst->SetVectorParameterValue(SaltOceanParams::Phase891011, FLinearColor(Phases[8], Phases[9], Phases[10], Phases[11]));
	const float* Prev = bHavePrevPhases ? PrevPhases : Phases;
	Inst->SetVectorParameterValue(SaltOceanParams::PrevPhase0123, FLinearColor(Prev[0], Prev[1], Prev[2], Prev[3]));
	Inst->SetVectorParameterValue(SaltOceanParams::PrevPhase4567, FLinearColor(Prev[4], Prev[5], Prev[6], Prev[7]));
	Inst->SetVectorParameterValue(SaltOceanParams::PrevPhase891011, FLinearColor(Prev[8], Prev[9], Prev[10], Prev[11]));
	for (int32 j = 0; j < FSaltWaveField::MaxCalmZones; ++j)
	{
		const FVector4f& Z = Field.Calm[j];
		Inst->SetVectorParameterValue(SaltOceanParams::Calm[j], FLinearColor(Z.X, Z.Y, Z.Z, Z.W));
	}
	Inst->SetVectorParameterValue(SaltOceanParams::CalmMin, FLinearColor(Field.CalmMin[0], Field.CalmMin[1], Field.CalmMin[2], Field.CalmMin[3]));
	// Chop patches (x: factor in calm patches, y: 2pi/patch size, z: swell count) and w: swell height
	// (sum of swell amplitudes, cm) so the material can normalise crest masks for any sea state.
	float SwellHeight = 0.f;
	for (int32 i = 0; i < FMath::Min(Field.SwellCount, Field.Waves.Num()); ++i)
	{
		SwellHeight += Field.Waves[i].Amplitude;
	}
	Inst->SetVectorParameterValue(SaltOceanParams::Chop, FLinearColor(Field.ChopMin, Field.ChopFreq, (float)Field.SwellCount, FMath::Max(SwellHeight, 1.f)));
	// Z: ocean time for slow material animation (shore bands), wrapped hourly to keep float precision.
	Inst->SetVectorParameterValue(SaltOceanParams::Globals, FLinearColor((float)Field.Waves.Num(), (float)Field.WaterLevel,
		(float)FMath::Fmod(OceanTime, 3600.0), 0.f));
	// Ripple UV offsets wrapped in double precision: seamless scrolling for any session length.
	auto Wrap = [](double V) { return (float)(V - FMath::FloorToDouble(V)); };
	Inst->SetVectorParameterValue(SaltOceanParams::Ripples, FLinearColor(
		Wrap(RippleScroll.X * OceanTime), Wrap(RippleScroll.Y * OceanTime), Wrap(RippleScroll.Z * OceanTime), Wrap(RippleScroll.W * OceanTime)));
}

// --- CDLOD tile selection -----------------------------------------------------------

bool ASaltOcean::GetViewLocation(FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		OutLocation = World->ViewLocationsRenderedLastFrame[0];
		return true;
	}
	OutLocation = GetActorLocation() + FVector(0.f, 0.f, 200.f);
	return false;
}

void ASaltOcean::UpdateTiles(bool bForce)
{
	if (!TileMesh || !QuarterTileMesh || TileCells < 4)
	{
		return;
	}
	FVector View;
	GetViewLocation(View);
	if (!bForce && FVector::DistSquared(View, LastSelectionView) < FMath::Square(ReselectDistance))
	{
		return;
	}
	LastSelectionView = View;

	TArray<FTileInstance> Full;
	TArray<FTileInstance> Quarter;
	const int32 TopLod = LodCount - 1;
	const double RootSize = Lod0NodeSize * FMath::Pow(2.0, (double)TopLod);
	// Root grid: a 3x3 block of top-level nodes centred on the viewer's root cell keeps the horizon covered.
	const FVector Origin = GetActorLocation();
	const double CellX = FMath::FloorToDouble((View.X - Origin.X) / RootSize + 0.5);
	const double CellY = FMath::FloorToDouble((View.Y - Origin.Y) / RootSize + 0.5);
	for (int32 dy = -1; dy <= 1; ++dy)
	{
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			const FVector2D Center(Origin.X + (CellX + dx) * RootSize, Origin.Y + (CellY + dy) * RootSize);
			if (!SelectNode(Center, RootSize, TopLod, View, Full, Quarter))
			{
				Full.Add({ Center, (float)(RootSize / TileCells), TopLod });
			}
		}
	}
	ApplyTiles(FullTiles, Full);
	ApplyTiles(QuarterTiles, Quarter);
	DebugTileCount = Full.Num() + Quarter.Num();
}

bool ASaltOcean::SelectNode(const FVector2D& Center, double Size, int32 Lod, const FVector& View, TArray<FTileInstance>& OutFull, TArray<FTileInstance>& OutQuarter) const
{
	const double Half = Size * 0.5;
	const double Z = Field.WaterLevel;
	// Distance from the viewer to the node's (flat) footprint.
	const double Cx = FMath::Clamp((double)View.X, Center.X - Half, Center.X + Half);
	const double Cy = FMath::Clamp((double)View.Y, Center.Y - Half, Center.Y + Half);
	const double DistSq = FMath::Square(View.X - Cx) + FMath::Square(View.Y - Cy) + FMath::Square(View.Z - Z);
	if (DistSq > FMath::Square(RangeOf(Lod)))
	{
		return false;
	}
	const float CellSize = (float)(Size / TileCells);
	if (Lod == 0 || DistSq > FMath::Square(RangeOf(Lod - 1)))
	{
		OutFull.Add({ Center, CellSize, Lod });
		return true;
	}
	const double Q = Size * 0.25;
	const FVector2D ChildCenters[4] = {
		Center + FVector2D(-Q, -Q), Center + FVector2D(Q, -Q), Center + FVector2D(-Q, Q), Center + FVector2D(Q, Q) };
	for (const FVector2D& Child : ChildCenters)
	{
		if (!SelectNode(Child, Half, Lod - 1, View, OutFull, OutQuarter))
		{
			// This quadrant stays at the current LOD (same cell size, half the cells).
			OutQuarter.Add({ Child, CellSize, Lod });
		}
	}
	return true;
}

void ASaltOcean::ApplyTiles(UInstancedStaticMeshComponent* Component, const TArray<FTileInstance>& Tiles)
{
	Component->ClearInstances();
	TArray<FTransform> Transforms;
	Transforms.Reserve(Tiles.Num());
	const double Z = Field.WaterLevel;
	for (const FTileInstance& T : Tiles)
	{
		Transforms.Add(FTransform(FQuat::Identity, FVector(T.Center.X, T.Center.Y, Z), FVector(T.CellSize, T.CellSize, 1.f)));
	}
	Component->AddInstances(Transforms, false, true, false);
	for (int32 i = 0; i < Tiles.Num(); ++i)
	{
		const int32 Lod = Tiles[i].Lod;
		const double End = RangeOf(Lod);
		const double Prev = RangeOf(Lod - 1);
		const double Start = Prev + (End - Prev) * (1.0 - MorphFraction);
		const float Data[3] = { Tiles[i].CellSize, (float)Start, (float)End };
		Component->SetCustomData(i, Data, false);
	}
	Component->MarkRenderStateDirty();
}

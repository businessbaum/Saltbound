// Saltbound - the sea's events (D88; see SaltSeaEvents.h).

#include "World/SaltSeaEvents.h"

#include "Boat/SaltBoat.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Saltbound.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPrologue.h"

namespace SaltSeaEventsLocal
{
	const TCHAR* SeaLightMesh = TEXT("/Game/Saltbound/Props/Events/SM_SeaLight.SM_SeaLight");
	// The Old World's turquoise (MI_AncientGlow's GlowColor; MI_SeaGlow fades it to its outline).
	const FLinearColor Turquoise(0.35f, 0.95f, 0.85f);
	constexpr float PeakGlow = 24.f;           // MI_SeaGlow's GlowIntensity at the pass's height (260 burnt it white, a cut-out;
	                                           // 45 read as a solid blue thing near the surface)
	constexpr float PeakCandela = 1300.f;
	// The pass, from the boat at its start (cm): out to one side and deep, under the keel, out to the other side and deeper.
	constexpr float Across = 3200.f;
	constexpr float DeepOut = 1600.f;
	constexpr float DeepUnder = 780.f;

	FAutoConsoleCommandWithWorldAndArgs CmdSeaLight(
		TEXT("sb.Event.SeaLight"),
		TEXT("D88: the light under the water passes under the player's boat now: sb.Event.SeaLight [record] (record saves the beat); ")
		TEXT("sb.Event.SeaLight why: what holds it back."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltSeaEventsSubsystem* Events = World ? World->GetSubsystem<USaltSeaEventsSubsystem>() : nullptr;
			if (!Events)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0] == TEXT("why"))
			{
				FString Why;
				const bool bNow = Events->SeaLightConditions(&Why);
				UE_LOG(LogSaltbound, Display, TEXT("SB_SEA_LIGHT conditions=%d %s"), bNow ? 1 : 0, *Why);
				return;
			}
			Events->PassSeaLight(Args.Num() > 0 && Args[0] == TEXT("record"));
		}));
}

const FName USaltSeaEventsSubsystem::SeaLightBeat(TEXT("SeaLight"));

// --- the light under the water --------------------------------------------------------------------------------------------------

ASaltSeaLight::ASaltSeaLight()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);
	Body->SetCanEverAffectNavigation(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Body);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetLightColor(SaltSeaEventsLocal::Turquoise);
	Light->SetAttenuationRadius(2600.f);
	Light->SetSourceLength(600.f);
	Light->SetSourceRadius(60.f);
	Light->SetCastShadows(false);
}

void ASaltSeaLight::Pass(const ASaltBoat* Boat)
{
	using namespace SaltSeaEventsLocal;
	if (!Boat)
	{
		return;
	}
	if (!Body->GetStaticMesh())
	{
		Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SeaLightMesh));
		Glow = Body->CreateAndSetMaterialInstanceDynamic(0);
	}
	// Under the boat where she is, across her heading (from one side or the other), a little ahead of her (she sails on over it).
	const FVector At = Boat->GetActorLocation();
	const FVector Fwd = Boat->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	const float Side = FMath::RandBool() ? 1.f : -1.f;
	From = FVector(At.X, At.Y, 0.0) - Right * (Across * Side) + Fwd * 900.f - FVector(0.f, 0.f, DeepOut);
	Under = FVector(At.X, At.Y, 0.0) + Fwd * 350.f - FVector(0.f, 0.f, DeepUnder);
	To = FVector(At.X, At.Y, 0.0) + Right * (Across * Side) - Fwd * 300.f - FVector(0.f, 0.f, DeepOut * 1.4f);
	Time = 0.f;
	SetActorLocation(From);
	UE_LOG(LogSaltbound, Display, TEXT("SB_SEA_LIGHT pass from %s under %s to %s"), *From.ToString(), *Under.ToString(), *To.ToString());
}

void ASaltSeaLight::Tick(float DeltaSeconds)
{
	using namespace SaltSeaEventsLocal;
	Super::Tick(DeltaSeconds);
	if (Time < 0.f)
	{
		return;
	}
	Time += DeltaSeconds;
	const float T = FMath::Clamp(Time / Duration, 0.f, 1.f);
	if (Time >= Duration)
	{
		Destroy();
		return;
	}
	// A curve through the point under the keel at the middle (its control point set so), a slow sway as it swims.
	const FVector C = Under * 2.f - (From + To) * 0.5f;
	auto At = [&](float U) { return From * FMath::Square(1.f - U) + C * (2.f * (1.f - U) * U) + To * (U * U); };
	FVector P = At(T);
	const FVector Tangent = (At(FMath::Min(T + 0.01f, 1.f)) - At(FMath::Max(T - 0.01f, 0.f))).GetSafeNormal();
	const FVector Sway = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal() * (60.f * FMath::Sin(Time * 1.4f));
	P += Sway;
	SetActorLocationAndRotation(P, FRotationMatrix::MakeFromX(Tangent + FVector(0.f, 0.f, 0.05f * FMath::Sin(Time * 0.9f))).ToQuat());
	// It brightens as it comes up, is brightest under the keel, breathes slowly, and is gone as it sinks.
	const float Fade = FMath::SmoothStep(0.f, 0.3f, T) * (1.f - FMath::SmoothStep(0.62f, 1.f, T));
	const float Breath = 0.72f + 0.28f * FMath::Sin(Time * 2.3f);
	if (Glow)
	{
		Glow->SetScalarParameterValue(TEXT("GlowIntensity"), PeakGlow * Fade * Breath);
	}
	Light->SetIntensity(PeakCandela * Fade * Breath);
}

// --- when -----------------------------------------------------------------------------------------------------------------------

bool USaltSeaEventsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USaltSeaEventsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltSeaEventsSubsystem, STATGROUP_Tickables);
}

ASaltBoat* USaltSeaEventsSubsystem::FindBoat() const
{
	for (TActorIterator<ASaltBoat> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool USaltSeaEventsSubsystem::SeaLightConditions(FString* OutWhyNot) const
{
	auto No = [OutWhyNot](const TCHAR* Why) { if (OutWhyNot) { *OutWhyNot = Why; } return false; };
	UWorld* World = GetWorld();
	const ASaltBoat* Boat = FindBoat();
	if (!World || !Boat)
	{
		return No(TEXT("no boat"));
	}
	if (Boat->GetHold() != ESaltBoatHold::Free || Boat->GetSpeedKnots() < 1.5f)
	{
		return No(TEXT("the boat is not under way"));
	}
	const ASaltLightingRig* Rig = nullptr;
	for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
	{
		Rig = *It;
		break;
	}
	if (!Rig || Rig->GetNightAmount() < 0.65f)
	{
		return No(TEXT("not night"));
	}
	const FSaltMooring& Home = Boat->GetHomeMooring();
	if (Home.IsValid() && FVector::Dist2D(Home.Pose.GetLocation(), Boat->GetActorLocation()) < 30000.f)
	{
		return No(TEXT("too near home"));
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player || FVector::Dist(Player->GetActorLocation(), Boat->GetActorLocation()) > 1200.f)
	{
		return No(TEXT("the player is not aboard"));
	}
	if (OutWhyNot)
	{
		*OutWhyNot = TEXT("now");
	}
	return true;
}

ASaltSeaLight* USaltSeaEventsSubsystem::PassSeaLight(bool bRecord)
{
	UWorld* World = GetWorld();
	ASaltBoat* Boat = FindBoat();
	if (!World || !Boat)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Passing = World->SpawnActor<ASaltSeaLight>(ASaltSeaLight::StaticClass(), FTransform::Identity, Params);
	if (Passing)
	{
		Passing->Pass(Boat);
	}
	if (bRecord)
	{
		USaltSaveSubsystem* Saves = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<USaltSaveSubsystem>() : nullptr;
		const USaltPrologueSubsystem* Prologue = World->GetSubsystem<USaltPrologueSubsystem>();
		if (Saves)
		{
			Saves->SetBeat(SeaLightBeat, Prologue ? Prologue->Now() : 0.f);
		}
	}
	return Passing;
}

void USaltSeaEventsSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Clock += DeltaTime;
	if (Clock < NextCheck)
	{
		return;
	}
	NextCheck = Clock + 2.f;
	if (Passing && IsValid(Passing))
	{
		return;
	}
	UWorld* World = GetWorld();
	const USaltSaveSubsystem* Saves = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<USaltSaveSubsystem>() : nullptr;
	// Once, on the first night at sea after casting off.
	if (!Saves || !Saves->HasBeat(SaltStory::CastOff) || Saves->HasBeat(SeaLightBeat))
	{
		return;
	}
	if (SeaLightConditions())
	{
		PassSeaLight(true);
	}
}

// Saltbound - development: static mesh that rides the ocean surface (simple kinematic buoyancy preview).

#include "Dev/SaltDevFloater.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Ocean/SaltOcean.h"
#include "Ocean/SaltOceanSubsystem.h"

ASaltDevFloater::ASaltDevFloater()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetRootComponent(Mesh);
}

void ASaltDevFloater::BeginPlay()
{
	Super::BeginPlay();
	BaseXY = GetActorLocation();
	Yaw = GetActorRotation().Yaw;
	if (USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>())
	{
		if (ASaltOcean* Ocean = Sub->GetOcean())
		{
			AddTickPrerequisiteActor(Ocean); // sample this frame's waves
		}
	}
}

void ASaltDevFloater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const USaltOceanSubsystem* Sub = GetWorld()->GetSubsystem<USaltOceanSubsystem>();
	if (!Sub || DeltaSeconds <= 0.f)
	{
		return;
	}
	const FRotator YawRot(0.f, Yaw, 0.f);
	const FVector Fwd = YawRot.RotateVector(FVector::ForwardVector);
	const FVector Right = YawRot.RotateVector(FVector::RightVector);
	auto H = [&](float F, float R)
	{
		const FVector P = BaseXY + Fwd * F + Right * R;
		return (float)Sub->SampleWater(FVector2D(P.X, P.Y)).Location.Z;
	};
	const float Bow = H(HalfExtent.X, 0.f), Stern = H(-HalfExtent.X, 0.f);
	const float Port = H(0.f, -HalfExtent.Y), Stbd = H(0.f, HalfExtent.Y);
	const float Centre = H(0.f, 0.f);
	const FVector Target(
		0.4f * Centre + 0.15f * (Bow + Stern + Port + Stbd),
		FMath::RadiansToDegrees(FMath::Atan2(Bow - Stern, 2.f * HalfExtent.X)),
		FMath::RadiansToDegrees(FMath::Atan2(Port - Stbd, 2.f * HalfExtent.Y)));
	if (!bInitialised)
	{
		State = Target;
		bInitialised = true;
	}
	// Critically-damped-ish spring per channel (semi-implicit Euler).
	const float W = 2.f * PI * Frequency;
	const FVector Accel = (Target - State) * (W * W) - StateVel * (2.f * Damping * W);
	StateVel += Accel * DeltaSeconds;
	State += StateVel * DeltaSeconds;

	const FQuat OldQ = GetActorQuat();
	SetActorLocationAndRotation(FVector(BaseXY.X, BaseXY.Y, State.X + Freeboard), FRotator(State.Y, Yaw, State.Z), false, nullptr, ETeleportType::None);
	AngularVelocity = AngularVelocityFromDelta(OldQ, GetActorQuat(), DeltaSeconds);
	Mesh->ComponentVelocity = FVector(0.f, 0.f, StateVel.X);
}

// Saltbound - something hanging from a pivot that swings like a damped pendulum (D53).

#include "Props/SaltSwingComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float SwingMaxSubstep = 1.f / 120.f;
	/** A pivot that jumps further than this in one frame was placed, not moved: hang down at rest again. */
	constexpr float SwingTeleportDistance = 150.f;
}

USaltSwingComponent::USaltSwingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetMobility(EComponentMobility::Movable);
}

void USaltSwingComponent::BeginPlay()
{
	Super::BeginPlay();
	Settle();
}

FQuat USaltSwingComponent::ParentQuat() const
{
	const USceneComponent* Parent = GetAttachParent();
	return Parent ? Parent->GetComponentQuat() : FQuat::Identity;
}

FVector USaltSwingComponent::ParentDown() const
{
	return ParentQuat().RotateVector(FVector::DownVector);
}

void USaltSwingComponent::Settle()
{
	LastPivot = GetComponentLocation();
	Bob = LastPivot + ParentDown() * Length;
	BobVel = FVector::ZeroVector;
	bStarted = true;
	ApplyPose();
}

void USaltSwingComponent::Kick(const FVector& Velocity)
{
	BobVel += Velocity;
}

float USaltSwingComponent::GetSwingAngle() const
{
	const FVector D = (Bob - GetComponentLocation()).GetSafeNormal();
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(D | ParentDown(), -1.f, 1.f)));
}

void USaltSwingComponent::ApplyPose()
{
	const FVector Down = ParentDown();
	const FVector D = (Bob - GetComponentLocation()).GetSafeNormal();
	const FQuat Tilt = D.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(Down, D);
	SetWorldRotation(Tilt * ParentQuat());
}

void USaltSwingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (DeltaTime <= 0.f)
	{
		return;
	}
	const FVector Pivot = GetComponentLocation();
	if (!bStarted || FVector::DistSquared(Pivot, LastPivot) > FMath::Square(SwingTeleportDistance))
	{
		Settle();
		return;
	}
	const FVector Down = ParentDown();
	const FVector PivotVel = (Pivot - LastPivot) / DeltaTime;
	const float GravityZ = GetWorld() ? GetWorld()->GetGravityZ() : -980.f;
	const float Omega = FMath::Sqrt(FMath::Abs(GravityZ) / FMath::Max(Length, 1.f));
	const float CosMax = FMath::Cos(FMath::DegreesToRadians(MaxAngle));
	const float SinMax = FMath::Sin(FMath::DegreesToRadians(MaxAngle));
	StrikeCooldown = FMath::Max(StrikeCooldown - DeltaTime, 0.f);

	// Whoever walks into it pushes it away (the player's capsule, by its own motion relative to the pivot).
	FVector PushAccel = FVector::ZeroVector;
	if (PushRadius > 0.f)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		const UCapsuleComponent* Capsule = Pawn ? Pawn->FindComponentByClass<UCapsuleComponent>() : nullptr;
		if (Capsule)
		{
			const FVector C = Capsule->GetComponentLocation();
			const FVector PawnVel = bHasPawn ? (C - LastPawn) / DeltaTime : FVector::ZeroVector;
			LastPawn = C;
			bHasPawn = true;
			const float R = Capsule->GetScaledCapsuleRadius();
			const float HalfH = Capsule->GetScaledCapsuleHalfHeight();
			const FVector Axis = FVector(C.X, C.Y, FMath::Clamp(Bob.Z, C.Z - HalfH + R, C.Z + HalfH - R));
			FVector Away = Bob - Axis;
			const float Dist = Away.Size();
			const float Reach = R + PushRadius;
			if (Dist < Reach && Dist > 0.1f)
			{
				Away /= Dist;
				// Spring out of the capsule, and at least the speed the capsule closes in at.
				PushAccel = Away * (Reach - Dist) * 600.f;
				const float Closing = FVector::DotProduct(PawnVel - PivotVel, Away);
				const float Along = FVector::DotProduct(BobVel - PivotVel, Away);
				if (Closing > Along)
				{
					BobVel += Away * (Closing - Along);
				}
			}
		}
	}

	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / SwingMaxSubstep), 1, 12);
	const float H = DeltaTime / Steps;
	const float Decay = FMath::Exp(-2.f * Damping * Omega * H);
	for (int32 I = 0; I < Steps; ++I)
	{
		const FVector P = FMath::Lerp(LastPivot, Pivot, float(I + 1) / Steps);
		BobVel.Z += GravityZ * H;
		BobVel += PushAccel * H;
		BobVel = PivotVel + (BobVel - PivotVel) * Decay;
		const FVector Old = Bob;
		FVector D = (Bob + BobVel * H - P).GetSafeNormal();
		if (D.IsNearlyZero())
		{
			D = Down;
		}
		// The limit: a cone round the parent's down.
		bool bStruck = false;
		FVector Side = FVector::ZeroVector;
		const float CosA = D | Down;
		if (CosA < CosMax)
		{
			Side = (D - Down * CosA).GetSafeNormal();
			if (Side.IsNearlyZero())
			{
				Side = FVector::CrossProduct(Down, FVector::RightVector).GetSafeNormal();
			}
			D = Down * CosMax + Side * SinMax;
			bStruck = true;
		}
		Bob = P + D * Length;
		BobVel = (Bob - Old) / H;
		if (bStruck)
		{
			// Bounce off the cone's surface (its outward normal at the body).
			const FVector Normal = (Side * CosMax - Down * SinMax).GetSafeNormal();
			const FVector Rel = BobVel - PivotVel;
			const float Into = FVector::DotProduct(Rel, Normal);
			if (Into > 0.f)
			{
				BobVel -= Normal * Into * (1.f + Restitution);
			}
			if (StrikeCooldown <= 0.f && Into > 2.f)
			{
				++Strikes;
				LastStrikeSpeed = Into;
				StrikeCooldown = 0.12f;
			}
		}
	}
	LastPivot = Pivot;
	ApplyPose();
}

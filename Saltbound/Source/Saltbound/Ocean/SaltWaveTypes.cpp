// Saltbound - shared Gerstner wave model (CPU side). Mirrors Saltbound/Shaders/SaltOcean.ush.

#include "Ocean/SaltWaveTypes.h"

FSaltWaveField::FSaltWaveField()
{
	for (int32 i = 0; i < MaxCalmZones; ++i)
	{
		Calm[i] = FVector4f(1.0e9f, 1.0e9f, 0.f, 1.f); // far away: no effect
		CalmMin[i] = 1.f;
	}
}

void FSaltWaveField::Build(const TArray<FSaltWaveSpec>& Specs, float AmplitudeScale, float SteepnessScale, const TArray<FSaltCalmZone>& Zones, double InWaterLevel)
{
	WaterLevel = InWaterLevel;
	Waves.Reset();
	for (const FSaltWaveSpec& S : Specs)
	{
		if (Waves.Num() >= MaxWaves || S.Wavelength <= 1.f)
		{
			continue;
		}
		FWave W;
		const double k = 2.0 * UE_DOUBLE_PI / S.Wavelength;
		const double Rad = FMath::DegreesToRadians((double)S.DirectionDeg);
		W.Dir = FVector2D(FMath::Cos(Rad), FMath::Sin(Rad));
		W.K = W.Dir * k;
		W.Amplitude = S.Amplitude * AmplitudeScale;
		W.Horizontal = (float)(FMath::Clamp(S.Steepness * SteepnessScale, 0.f, 1.f) / k);
		W.Omega = FMath::Sqrt(Gravity * k);
		W.PhaseOffset = S.PhaseOffset;
		Waves.Add(W);
	}
	for (int32 i = 0; i < MaxCalmZones; ++i)
	{
		if (Zones.IsValidIndex(i))
		{
			const FSaltCalmZone& Z = Zones[i];
			Calm[i] = FVector4f((float)Z.Center.X, (float)Z.Center.Y, Z.InnerRadius, FMath::Max(Z.OuterRadius, Z.InnerRadius + 1.f));
			CalmMin[i] = FMath::Clamp(Z.MinFactor, 0.f, 1.f);
		}
		else
		{
			Calm[i] = FVector4f(1.0e9f, 1.0e9f, 0.f, 1.f);
			CalmMin[i] = 1.f;
		}
	}
}

void FSaltWaveField::SetTime(double TimeSeconds)
{
	for (FWave& W : Waves)
	{
		W.Phase = (float)FMath::Fmod(W.Omega * TimeSeconds + W.PhaseOffset, 2.0 * UE_DOUBLE_PI);
	}
}

float FSaltWaveField::Attenuation(const FVector2D& P) const
{
	float Att = 1.f;
	for (int32 j = 0; j < MaxCalmZones; ++j)
	{
		const float R = (float)FVector2D::Distance(P, FVector2D(Calm[j].X, Calm[j].Y));
		const float T = FMath::Clamp((R - Calm[j].Z) / FMath::Max(Calm[j].W - Calm[j].Z, 1.f), 0.f, 1.f);
		const float S = T * T * (3.f - 2.f * T);
		Att = FMath::Min(Att, FMath::Lerp(CalmMin[j], 1.f, S));
	}
	return Att;
}

void FSaltWaveField::SetChop(int32 InSwellCount, float Variation, float PatchSize)
{
	SwellCount = FMath::Max(0, InSwellCount);
	ChopMin = 1.f - FMath::Clamp(Variation, 0.f, 1.f);
	ChopFreq = PatchSize > 1.f ? (float)(2.0 * UE_DOUBLE_PI / PatchSize) : 0.f;
}

float FSaltWaveField::ChopFactor(const FVector2D& P) const
{
	if (ChopMin >= 1.f || ChopFreq <= 0.f)
	{
		return 1.f;
	}
	// Three skewed plane waves at incommensurate frequencies: irregular patches, no visible grid.
	const float X = (float)P.X * ChopFreq, Y = (float)P.Y * ChopFreq;
	const float A = FMath::Sin(X * 0.83f + Y * 0.31f + 1.3f);
	const float B = FMath::Sin(-X * 0.27f + Y * 0.91f + 4.1f);
	const float C = FMath::Sin(X * 0.58f - Y * 0.66f + 2.6f);
	const float N = FMath::Clamp(0.5f + 0.28f * (A + B) + 0.18f * C, 0.f, 1.f);
	const float T = FMath::Clamp((N - 0.25f) / 0.5f, 0.f, 1.f);
	return FMath::Lerp(ChopMin, 1.f, T * T * (3.f - 2.f * T));
}

FVector FSaltWaveField::Displacement(const FVector2D& P0) const
{
	const float Att = Attenuation(P0);
	const float Chop = ChopFactor(P0);
	FVector Disp = FVector::ZeroVector;
	for (int32 i = 0; i < Waves.Num(); ++i)
	{
		const FWave& W = Waves[i];
		const float F = Att * (i < SwellCount ? 1.f : Chop);
		float S, C;
		FMath::SinCos(&S, &C, (float)FVector2D::DotProduct(W.K, P0) - W.Phase);
		Disp.X += W.Dir.X * (W.Horizontal * F * C);
		Disp.Y += W.Dir.Y * (W.Horizontal * F * C);
		Disp.Z += W.Amplitude * F * S;
	}
	return Disp;
}

void FSaltWaveField::NormalAndJacobian(const FVector2D& P0, FVector& OutNormal, float& OutJacobian) const
{
	const float Att = Attenuation(P0);
	const float Chop = ChopFactor(P0);
	FVector Dx(1.f, 0.f, 0.f);
	FVector Dy(0.f, 1.f, 0.f);
	for (int32 i = 0; i < Waves.Num(); ++i)
	{
		const FWave& W = Waves[i];
		const float F = Att * (i < SwellCount ? 1.f : Chop);
		const float A = W.Amplitude * F;
		const float H = W.Horizontal * F;
		float S, C;
		FMath::SinCos(&S, &C, (float)FVector2D::DotProduct(W.K, P0) - W.Phase);
		Dx.X += W.Dir.X * (-H * S * W.K.X);
		Dx.Y += W.Dir.Y * (-H * S * W.K.X);
		Dx.Z += A * C * W.K.X;
		Dy.X += W.Dir.X * (-H * S * W.K.Y);
		Dy.Y += W.Dir.Y * (-H * S * W.K.Y);
		Dy.Z += A * C * W.K.Y;
	}
	OutNormal = FVector::CrossProduct(Dx, Dy).GetSafeNormal();
	OutJacobian = (float)(Dx.X * Dy.Y - Dx.Y * Dy.X);
}

FSaltWaterSample FSaltWaveField::Sample(const FVector2D& XY, int32 Iterations) const
{
	// Find the undisplaced point P0 whose horizontal displacement lands on XY.
	FVector2D P0 = XY;
	FVector Disp = Displacement(P0);
	for (int32 i = 0; i < Iterations; ++i)
	{
		P0 = XY - FVector2D(Disp.X, Disp.Y);
		Disp = Displacement(P0);
	}
	FSaltWaterSample Out;
	Out.Location = FVector(XY.X, XY.Y, WaterLevel + Disp.Z);
	NormalAndJacobian(P0, Out.Normal, Out.Jacobian);
	return Out;
}

float FSaltWaveField::MaxHeight() const
{
	float H = 0.f;
	for (const FWave& W : Waves)
	{
		H += W.Amplitude;
	}
	return H;
}

float FSaltWaveField::MaxHorizontal() const
{
	float H = 0.f;
	for (const FWave& W : Waves)
	{
		H += W.Horizontal;
	}
	return H;
}

// Saltbound - FFT ocean spectrum (CPU side). MUST stay in sync with Shaders/SaltOceanSpectrum.ush.

#include "SaltOceanFFT.h"

namespace SaltOceanFFT
{
	namespace
	{
		constexpr float Pi = 3.14159265f;

		// Band splits by wavelength (cm): cascade 0 >= 600, cascade 1 90..600, cascade 2 down to 4 texels.
		const FCascade Cascades[NumCascades] = {
			{ 25000.f, 0.f, 0.f, 2.f * Pi / 600.f },
			{ 3700.f, 23.4f, 2.f * Pi / 600.f, 2.f * Pi / 90.f },
			{ 600.f, 51.7f, 2.f * Pi / 90.f, 2.f * Pi / (4.f * 600.f / Size) },
		};

		uint32 Pcg(uint32 V)
		{
			const uint32 State = V * 747796405u + 2891336453u;
			const uint32 Word = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
			return (Word >> 22u) ^ Word;
		}

		FVector2f Gauss(int32 N, int32 M, uint32 Cascade, uint32 Seed)
		{
			const uint32 Key = ((uint32(N) & 0xFFFFu) << 16u) | (uint32(M) & 0xFFFFu);
			const uint32 H1 = Pcg(Key ^ Pcg(Cascade + Seed * 16u));
			const uint32 H2 = Pcg(H1);
			const float U1 = (float(H1 >> 8u) + 1.f) / 16777217.f;
			const float U2 = float(H2 >> 8u) / 16777216.f;
			const float R = FMath::Sqrt(-2.f * FMath::Loge(U1));
			return FVector2f(R * FMath::Cos(2.f * Pi * U2), R * FMath::Sin(2.f * Pi * U2));
		}

		float Lgamma(float X)
		{
			const float P = X * (X + 1.f) * (X + 2.f) * (X + 3.f) * (X + 4.f) * (X + 5.f);
			const float Z = X + 6.f;
			const float Lg = (Z - 0.5f) * FMath::Loge(Z) - Z + 0.918938533f + 1.f / (12.f * Z) - 1.f / (360.f * Z * Z * Z);
			return Lg - FMath::Loge(P);
		}

		/** Omnidirectional spectrum without the scale (psi / Scale). */
		float PsiShape(float Kl, float Kp, float Gamma, float KDamp)
		{
			const float Kr = Kl / Kp;
			const float Sigma = Kr <= 1.f ? 0.07f : 0.09f;
			const float Sq = FMath::Sqrt(Kr) - 1.f;
			const float R = FMath::Exp(-Sq * Sq / (2.f * Sigma * Sigma));
			float Psi = 1.f / (Kr * Kr * Kr) * FMath::Exp(-1.25f / (Kr * Kr)) * FMath::Pow(Gamma, R);
			if (KDamp > 0.f)
			{
				const float Kd = Kl / KDamp;
				Psi *= FMath::Exp(-Kd * Kd);
			}
			return Psi;
		}

		/** Mirrors SaltSpectrumAmplitude (Spec0 = wind dir, kp, scale; Spec1 = gamma, spread, min spread, kDamp). */
		float Amplitude(const FVector2f& K, float Dk, const FVector4f& Spec0, const FVector4f& Spec1, const FVector4f& Band)
		{
			const float Kl = K.Size();
			if (Kl < FMath::Max(Band.X, 1e-7f) || Kl >= Band.Y || Spec0.W <= 0.f)
			{
				return 0.f;
			}
			const float Kr = Kl / Spec0.Z;
			const float Psi = Spec0.W * PsiShape(Kl, Spec0.Z, Spec1.X, Spec1.W);
			const float S = FMath::Max(Spec1.Y * (Kr < 1.f ? FMath::Pow(Kr, 2.5f) : FMath::Pow(Kr, -1.25f)), Spec1.Z);
			const float HalfCos = FMath::Sqrt(FMath::Clamp(0.5f + 0.5f * (K.X / Kl * Spec0.X + K.Y / Kl * Spec0.Y), 0.f, 1.f));
			const float Q = FMath::Exp(Lgamma(S + 1.f) - Lgamma(S + 0.5f)) / (2.f * FMath::Sqrt(Pi));
			const float D = Q * FMath::Pow(FMath::Max(HalfCos, 1e-6f), 2.f * S);
			return FMath::Sqrt(FMath::Max(Psi * D / Kl, 0.f) * Dk * Dk * 0.5f) * Band.Z;
		}

		double Phase(float Kl, double Time)
		{
			// Same quantisation as SaltWavePhase, in float like the GPU (a different integer here would let the
			// wave drift away from the rendered one); the phase itself in double.
			const float W = FMath::Sqrt((float)Gravity * Kl);
			const double Q = (double)FMath::RoundToFloat(W * (float)LoopSeconds / (2.f * Pi));
			const double Frac = Time / LoopSeconds - FMath::FloorToDouble(Time / LoopSeconds);
			const double X = Q * Frac;
			return 2.0 * UE_DOUBLE_PI * (X - FMath::FloorToDouble(X));
		}

		/** In-place inverse FFT (e^+i) of N complex values, N a power of two. */
		void InverseFFT(FVector2f* Data, int32 N)
		{
			for (int32 i = 1, j = 0; i < N; ++i)
			{
				int32 Bit = N >> 1;
				for (; j & Bit; Bit >>= 1)
				{
					j ^= Bit;
				}
				j ^= Bit;
				if (i < j)
				{
					Swap(Data[i], Data[j]);
				}
			}
			for (int32 Len = 2; Len <= N; Len <<= 1)
			{
				const float Ang = 2.f * Pi / Len;
				for (int32 i = 0; i < N; i += Len)
				{
					for (int32 j = 0; j < Len / 2; ++j)
					{
						const FVector2f W(FMath::Cos(Ang * j), FMath::Sin(Ang * j));
						const FVector2f A = Data[i + j];
						const FVector2f B = Data[i + j + Len / 2];
						const FVector2f WB(W.X * B.X - W.Y * B.Y, W.X * B.Y + W.Y * B.X);
						Data[i + j] = A + WB;
						Data[i + j + Len / 2] = A - WB;
					}
				}
			}
		}
	}

	const FCascade& GetCascade(int32 Index)
	{
		return Cascades[FMath::Clamp(Index, 0, NumCascades - 1)];
	}

	float FSpectrum::ComputeScale() const
	{
		if (SignificantHeight <= 0.f || PeakWavelength <= 0.f)
		{
			return 0.f;
		}
		// Variance = integral of psi over the simulated band (log-spaced trapezoid in ln k).
		const float Kp = 2.f * Pi / PeakWavelength;
		const float KDamp = ShortWaveLength > 0.f ? 2.f * Pi / ShortWaveLength : 0.f;
		const double K0 = 2.0 * UE_DOUBLE_PI / Cascades[0].PatchSize;
		const double K1 = Cascades[NumCascades - 1].KMax;
		const int32 Steps = 2048;
		const double LnStep = FMath::Loge(K1 / K0) / Steps;
		double Sum = 0.0;
		for (int32 i = 0; i <= Steps; ++i)
		{
			const double K = K0 * FMath::Exp(LnStep * i);
			const double F = PsiShape((float)K, Kp, Gamma, KDamp) * K;
			Sum += (i == 0 || i == Steps) ? 0.5 * F : F;
		}
		const double Var = Sum * LnStep;
		const double Target = FMath::Square(SignificantHeight / 4.0);
		return Var > 0.0 ? (float)(Target / Var) : 0.f;
	}

	bool FSpectrum::SameShape(const FSpectrum& O) const
	{
		return SignificantHeight == O.SignificantHeight && PeakWavelength == O.PeakWavelength && WindDirectionDeg == O.WindDirectionDeg
			&& Gamma == O.Gamma && SpreadPeak == O.SpreadPeak && SpreadMin == O.SpreadMin && ShortWaveLength == O.ShortWaveLength
			&& CascadeGain == O.CascadeGain && CascadeChop == O.CascadeChop && Seed == O.Seed;
	}

	// --- Low band ------------------------------------------------------------------------------------

	void FLowBand::SetSpectrum(const FSpectrum& InSpectrum)
	{
		if (bHaveSpectrum && Spectrum.SameShape(InSpectrum))
		{
			return;
		}
		Spectrum = InSpectrum;
		bHaveSpectrum = true;
		GridTime = -1.0;
		Components.Reset();
		const float Scale = Spectrum.ComputeScale();
		if (Scale <= 0.f || Spectrum.CascadeGain.X <= 0.f)
		{
			return;
		}
		const FCascade& C = Cascades[0];
		const float Dk = 2.f * Pi / C.PatchSize;
		const float WindRad = FMath::DegreesToRadians(Spectrum.WindDirectionDeg);
		// Cascade 0 is not rotated: the tile frame is the world frame.
		const FVector4f Spec0(FMath::Cos(WindRad), FMath::Sin(WindRad), 2.f * Pi / Spectrum.PeakWavelength, Scale);
		const FVector4f Spec1(Spectrum.Gamma, Spectrum.SpreadPeak, Spectrum.SpreadMin,
			Spectrum.ShortWaveLength > 0.f ? 2.f * Pi / Spectrum.ShortWaveLength : 0.f);
		const FVector4f Band(C.KMin, C.KMax, Spectrum.CascadeGain.X, 0.f);
		for (int32 M = -MaxIndex; M <= MaxIndex; ++M)
		{
			for (int32 N = -MaxIndex; N <= MaxIndex; ++N)
			{
				if (N == 0 && M == 0)
				{
					continue;
				}
				const FVector2f K(N * Dk, M * Dk);
				const float A = Amplitude(K, Dk, Spec0, Spec1, Band);
				const float Am = Amplitude(-K, Dk, Spec0, Spec1, Band);
				if (A <= 0.f && Am <= 0.f)
				{
					continue;
				}
				FComponent Comp;
				Comp.N = N;
				Comp.M = M;
				Comp.H0 = Gauss(N, M, 0, Spectrum.Seed) * (A * 0.70710678f);
				Comp.H0m = Gauss(-N, -M, 0, Spectrum.Seed) * (Am * 0.70710678f);
				Comp.Kx = K.X;
				Comp.Ky = K.Y;
				Comp.Kl = K.Size();
				Components.Add(Comp);
			}
		}
	}

	void FLowBand::Update(double Time)
	{
		if (Time == GridTime)
		{
			return;
		}
		GridTime = Time;
		const int32 G = GridSize;
		GridA.SetNumZeroed(G * G);
		GridB.SetNumZeroed(G * G);
		if (Components.Num() == 0)
		{
			return;
		}
		// Four complex fields (same packing as SpectrumCS): Z1 = h + i Dx, Z2 = Dy + i hx, Z3 = hy + i Dxx,
		// Z4 = Dyy + i Dxy, laid out on a G x G lattice (negative indices wrap).
		TArray<FVector2f> Z[4];
		for (TArray<FVector2f>& Arr : Z)
		{
			Arr.SetNumZeroed(G * G);
		}
		const float Chop = Spectrum.CascadeChop.X;
		for (const FComponent& C : Components)
		{
			const double Ph = Phase(C.Kl, Time);
			const FVector2f E((float)FMath::Cos(Ph), (float)FMath::Sin(Ph));
			auto CMul = [](const FVector2f& A, const FVector2f& B) { return FVector2f(A.X * B.X - A.Y * B.Y, A.X * B.Y + A.Y * B.X); };
			const FVector2f H = CMul(C.H0, FVector2f(E.X, -E.Y)) + CMul(FVector2f(C.H0m.X, -C.H0m.Y), E);
			const FVector2f IH(-H.Y, H.X);
			const float Ux = C.Kx / C.Kl;
			const float Uy = C.Ky / C.Kl;
			const int32 Idx = ((C.M + G) % G) * G + ((C.N + G) % G);
			Z[0][Idx] = H - Ux * Chop * H;
			Z[1][Idx] = Uy * Chop * IH - C.Kx * H;
			Z[2][Idx] = C.Ky * IH - C.Kx * Ux * Chop * IH;
			Z[3][Idx] = -C.Ky * Uy * Chop * H - C.Kx * Uy * Chop * IH;
		}
		TArray<FVector2f> Column;
		Column.SetNumUninitialized(G);
		for (TArray<FVector2f>& Arr : Z)
		{
			for (int32 Row = 0; Row < G; ++Row)
			{
				InverseFFT(&Arr[Row * G], G);
			}
			for (int32 Col = 0; Col < G; ++Col)
			{
				for (int32 Row = 0; Row < G; ++Row)
				{
					Column[Row] = Arr[Row * G + Col];
				}
				InverseFFT(Column.GetData(), G);
				for (int32 Row = 0; Row < G; ++Row)
				{
					Arr[Row * G + Col] = Column[Row];
				}
			}
		}
		for (int32 i = 0; i < G * G; ++i)
		{
			GridA[i] = FVector4f(Z[0][i].X, Z[0][i].Y, Z[1][i].X, Z[1][i].Y);
			GridB[i] = FVector4f(Z[2][i].X, Z[2][i].Y, Z[3][i].X, Z[3][i].Y);
		}
	}

	FLowBand::FSample FLowBand::Sample(const FVector2D& P) const
	{
		FSample S;
		if (Components.Num() == 0 || GridA.Num() != GridSize * GridSize)
		{
			return S;
		}
		const double L = Cascades[0].PatchSize;
		const double Gx = P.X / L * GridSize;
		const double Gy = P.Y / L * GridSize;
		const double Fx = FMath::FloorToDouble(Gx);
		const double Fy = FMath::FloorToDouble(Gy);
		const float Tx = (float)(Gx - Fx);
		const float Ty = (float)(Gy - Fy);
		const int32 X0 = ((int32)FMath::Fmod(Fx, (double)GridSize) + GridSize) % GridSize;
		const int32 Y0 = ((int32)FMath::Fmod(Fy, (double)GridSize) + GridSize) % GridSize;
		const int32 X1 = (X0 + 1) % GridSize;
		const int32 Y1 = (Y0 + 1) % GridSize;
		auto Bilerp = [&](const TArray<FVector4f>& Grid)
		{
			const FVector4f A = FMath::Lerp(Grid[Y0 * GridSize + X0], Grid[Y0 * GridSize + X1], Tx);
			const FVector4f B = FMath::Lerp(Grid[Y1 * GridSize + X0], Grid[Y1 * GridSize + X1], Tx);
			return FMath::Lerp(A, B, Ty);
		};
		const FVector4f A = Bilerp(GridA);
		const FVector4f B = Bilerp(GridB);
		S.H = A.X; S.Dx = A.Y; S.Dy = A.Z; S.Hx = A.W;
		S.Hy = B.X; S.Dxx = B.Y; S.Dyy = B.Z; S.Dxy = B.W;
		return S;
	}
}

// Saltbound - FFT ocean (Tessendorf): GPU simulation of the wind sea and a CPU mirror of its long waves.
//
// Three cascades (tiles of different size and rotation) split the spectrum by wavelength:
//   0: 250 m tile, waves >= 6 m       (wave shapes: crests, faces, troughs; geometry)
//   1: 37 m tile,  0.9 m .. 6 m       (smaller waves carried by the large ones; geometry + shading)
//   2: 6 m tile,   9 cm .. 0.9 m      (ripples; shading only)
// Every frame the compute passes write, per cascade, three wrap-around textures with mips:
//   Disp  (Dx, Dy, h, Dxy)   Deriv (hx, hy, Dxx, Dyy)   Aux (unused, hx^2, hy^2, hx hy)
// plus DispPrev (last frame's Disp) for motion vectors. The ocean material samples them at the water's
// undisplaced (Lagrangian) position: tile uv = R(rotation) * P0 / PatchSize + 0.5 / Size.
// Gameplay queries use FLowBand: the same spectrum's waves with |n|, |m| <= LowBandMaxIndex in cascade 0
// (>= ~12 m), evaluated on the CPU with a 64^2 FFT.
// Shader side: Saltbound/Shaders/SaltOceanFFT.usf and SaltOceanSpectrum.ush (keep in sync).

#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

namespace SaltOceanFFT
{
	constexpr int32 Size = 256;
	constexpr int32 NumCascades = 3;
	/** The wave field repeats exactly after this many seconds (frequencies are quantised to it). */
	constexpr double LoopSeconds = 1000.0;
	constexpr double Gravity = 980.665; // cm/s^2

	struct FCascade
	{
		float PatchSize = 0.f;     // cm
		float RotationDeg = 0.f;
		float KMin = 0.f;          // rad/cm, band of this cascade
		float KMax = 0.f;
	};

	SALTBOUNDRENDER_API const FCascade& GetCascade(int32 Index);

	/** Wind-sea spectrum (JONSWAP-like with Mitsuyasu cos^2s spreading). */
	struct SALTBOUNDRENDER_API FSpectrum
	{
		/** Significant wave height of the wind sea, 4 * standard deviation of the height (cm). */
		float SignificantHeight = 150.f;
		/** Wavelength of the spectral peak (cm): the dominant wind-wave size. */
		float PeakWavelength = 3500.f;
		/** Direction the waves travel towards (degrees, 0 = +X). */
		float WindDirectionDeg = 30.f;
		/** JONSWAP peak enhancement (1 = Pierson-Moskowitz, 3.3 = young sea). */
		float Gamma = 3.3f;
		/** cos^2s spreading exponent at the peak (higher = longer, more parallel crests) and its floor for
		 *  waves far from the peak (lower = more varied directions, short-crested). */
		float SpreadPeak = 8.f;
		float SpreadMin = 2.f;
		/** Waves shorter than this are damped (low wind: smooth patches between ripples). 0 = off (cm). */
		float ShortWaveLength = 0.f;
		/** Amplitude multiplier per cascade (e.g. quieter ripples). */
		FVector3f CascadeGain = FVector3f(1.f, 1.f, 1.f);
		/** Horizontal displacement factor per cascade (crest sharpness). */
		FVector3f CascadeChop = FVector3f(1.f, 1.f, 0.6f);
		uint32 Seed = 1;

		/** Spectrum scale for SignificantHeight (numerical integral over all cascades). */
		float ComputeScale() const;
		bool SameShape(const FSpectrum& O) const;
	};

	/** Settings for one simulation frame. */
	struct FFrame
	{
		FSpectrum Spectrum;
		double Time = 0.0;
		float DeltaTime = 0.f;
	};

	struct FTargets
	{
		UTextureRenderTarget2D* Disp[NumCascades] = {};
		UTextureRenderTarget2D* DispPrev[NumCascades] = {};
		UTextureRenderTarget2D* Deriv[NumCascades] = {};
		UTextureRenderTarget2D* Aux[NumCascades] = {};
		bool IsValid() const;
	};

	/** Queues this frame's simulation on the render thread (call once per frame from the game thread). */
	SALTBOUNDRENDER_API void Enqueue(const FFrame& Frame, const FTargets& Targets);

	/** Long waves of cascade 0 on the CPU, bilinearly sampled from a 64^2 grid (tile frame = world). */
	class SALTBOUNDRENDER_API FLowBand
	{
	public:
		static constexpr int32 GridSize = 64;
		static constexpr int32 MaxIndex = 21;

		struct FSample
		{
			float H = 0.f, Dx = 0.f, Dy = 0.f, Hx = 0.f, Hy = 0.f, Dxx = 0.f, Dyy = 0.f, Dxy = 0.f;
		};

		/** Rebuilds the wave amplitudes when the spectrum changed. */
		void SetSpectrum(const FSpectrum& Spectrum);
		/** Evaluates the fields for a time (cheap no-op if unchanged). */
		void Update(double Time);
		/** Fields at a world position (cm, relative to the ocean origin). */
		FSample Sample(const FVector2D& P) const;
		bool HasWaves() const { return Components.Num() > 0; }

	private:
		struct FComponent
		{
			int32 N = 0, M = 0;
			FVector2f H0 = FVector2f::ZeroVector;
			FVector2f H0m = FVector2f::ZeroVector;
			float Kx = 0.f, Ky = 0.f, Kl = 0.f;
		};
		TArray<FComponent> Components;
		TArray<FVector4f> GridA;   // h, Dx, Dy, hx
		TArray<FVector4f> GridB;   // hy, Dxx, Dyy, Dxy
		FSpectrum Spectrum;
		bool bHaveSpectrum = false;
		double GridTime = -1.0;
	};
}

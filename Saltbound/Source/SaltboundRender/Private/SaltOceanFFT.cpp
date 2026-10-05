// Saltbound - FFT ocean: compute shader classes and the per-frame render graph (see SaltOceanFFT.h).

#include "SaltOceanFFT.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GenerateMips.h"
#include "GlobalShader.h"
#include "Misc/App.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

namespace
{
	class FSaltSpectrumCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSaltSpectrumCS);
		SHADER_USE_PARAMETER_STRUCT(FSaltSpectrumCS, FGlobalShader);
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_ARRAY(FVector4f, CascadeA, [SaltOceanFFT::NumCascades])
			SHADER_PARAMETER_ARRAY(FVector4f, CascadeB, [SaltOceanFFT::NumCascades])
			SHADER_PARAMETER(FVector4f, Spec0)
			SHADER_PARAMETER(FVector4f, Spec1)
			SHADER_PARAMETER(float, TimeFrac)
			SHADER_PARAMETER(float, LoopSeconds)
			SHADER_PARAMETER(uint32, Seed)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, OutSpectrum)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FSaltSpectrumCS, "/Saltbound/SaltOceanFFT.usf", "SpectrumCS", SF_Compute);

	class FSaltFFTCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSaltFFTCS);
		SHADER_USE_PARAMETER_STRUCT(FSaltFFTCS, FGlobalShader);
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, Vertical)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, FFTData)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			// The FFT transforms in place (float4 UAV loads). DX12 / SM6 has them; the D3D11 SM5 fallback needs the opt-in or
			// the cook fails ("UAV loads from additional typed formats").
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FSaltFFTCS, "/Saltbound/SaltOceanFFT.usf", "FFTCS", SF_Compute);

	class FSaltResolveCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSaltResolveCS);
		SHADER_USE_PARAMETER_STRUCT(FSaltResolveCS, FGlobalShader);
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float4>, FFTResult)
			SHADER_PARAMETER(uint32, Cascade)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDisp)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDeriv)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutAux)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FSaltResolveCS, "/Saltbound/SaltOceanFFT.usf", "ResolveCS", SF_Compute);

	struct FRenderTargets
	{
		FTextureRenderTargetResource* Disp[SaltOceanFFT::NumCascades] = {};
		FTextureRenderTargetResource* DispPrev[SaltOceanFFT::NumCascades] = {};
		FTextureRenderTargetResource* Deriv[SaltOceanFFT::NumCascades] = {};
		FTextureRenderTargetResource* Aux[SaltOceanFFT::NumCascades] = {};
	};

	struct FShaderFrame
	{
		FVector4f CascadeA[SaltOceanFFT::NumCascades];
		FVector4f CascadeB[SaltOceanFFT::NumCascades];
		FVector4f Spec0;
		FVector4f Spec1;
		float TimeFrac = 0.f;
		uint32 Seed = 1;
	};

	FRDGTextureRef Register(FRDGBuilder& GraphBuilder, FTextureRenderTargetResource* Resource, const TCHAR* Name)
	{
		FRHITexture* Texture = Resource ? Resource->GetRenderTargetTexture() : nullptr;
		return Texture ? RegisterExternalTexture(GraphBuilder, Texture, Name) : nullptr;
	}

	void AddPasses(FRDGBuilder& GraphBuilder, const FShaderFrame& F, const FRenderTargets& T)
	{
		using namespace SaltOceanFFT;
		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		RDG_EVENT_SCOPE(GraphBuilder, "SaltOceanFFT");

		const FRDGTextureDesc SpecDesc = FRDGTextureDesc::Create2DArray(FIntPoint(Size, Size), PF_A32B32G32R32F,
			FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV, NumCascades * 2);
		FRDGTextureRef Spectrum = GraphBuilder.CreateTexture(SpecDesc, TEXT("SaltOcean.Spectrum"));

		{
			FSaltSpectrumCS::FParameters* P = GraphBuilder.AllocParameters<FSaltSpectrumCS::FParameters>();
			for (int32 c = 0; c < NumCascades; ++c)
			{
				P->CascadeA[c] = F.CascadeA[c];
				P->CascadeB[c] = F.CascadeB[c];
			}
			P->Spec0 = F.Spec0;
			P->Spec1 = F.Spec1;
			P->TimeFrac = F.TimeFrac;
			P->LoopSeconds = (float)LoopSeconds;
			P->Seed = F.Seed;
			P->OutSpectrum = GraphBuilder.CreateUAV(Spectrum);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Spectrum"), ShaderMap->GetShader<FSaltSpectrumCS>(), P,
				FIntVector(Size / 8, Size / 8, NumCascades));
		}
		for (uint32 Vertical = 0; Vertical < 2; ++Vertical)
		{
			FSaltFFTCS::FParameters* P = GraphBuilder.AllocParameters<FSaltFFTCS::FParameters>();
			P->Vertical = Vertical;
			P->FFTData = GraphBuilder.CreateUAV(Spectrum);
			FComputeShaderUtils::AddPass(GraphBuilder, Vertical ? RDG_EVENT_NAME("FFT columns") : RDG_EVENT_NAME("FFT rows"),
				ShaderMap->GetShader<FSaltFFTCS>(), P, FIntVector(Size, NumCascades * 2, 1));
		}

		FGenerateMipsParams MipParams;
		MipParams.AddressU = AM_Wrap;
		MipParams.AddressV = AM_Wrap;
		for (int32 c = 0; c < NumCascades; ++c)
		{
			FRDGTextureRef Disp = Register(GraphBuilder, T.Disp[c], TEXT("SaltOcean.Disp"));
			FRDGTextureRef DispPrev = Register(GraphBuilder, T.DispPrev[c], TEXT("SaltOcean.DispPrev"));
			FRDGTextureRef Deriv = Register(GraphBuilder, T.Deriv[c], TEXT("SaltOcean.Deriv"));
			FRDGTextureRef Aux = Register(GraphBuilder, T.Aux[c], TEXT("SaltOcean.Aux"));
			if (!Disp || !DispPrev || !Deriv || !Aux)
			{
				continue;
			}
			// Last frame's displacement (all mips) for the velocity pass.
			FRHICopyTextureInfo AllMips;
			AllMips.NumMips = FMath::Min(Disp->Desc.NumMips, DispPrev->Desc.NumMips);
			AddCopyTexturePass(GraphBuilder, Disp, DispPrev, AllMips);

			FSaltResolveCS::FParameters* P = GraphBuilder.AllocParameters<FSaltResolveCS::FParameters>();
			P->FFTResult = Spectrum;
			P->Cascade = (uint32)c;
			P->OutDisp = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Disp, 0));
			P->OutDeriv = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Deriv, 0));
			P->OutAux = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Aux, 0));
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Resolve %d", c), ShaderMap->GetShader<FSaltResolveCS>(), P,
				FIntVector(Size / 8, Size / 8, 1));

			for (FRDGTextureRef Tex : { Disp, Deriv, Aux })
			{
				if (Tex->Desc.NumMips > 1)
				{
					FGenerateMips::Execute(GraphBuilder, FeatureLevel, Tex, MipParams);
				}
				GraphBuilder.SetTextureAccessFinal(Tex, ERHIAccess::SRVMask);
			}
			GraphBuilder.SetTextureAccessFinal(DispPrev, ERHIAccess::SRVMask);
		}
	}

	bool Usable(const UTextureRenderTarget2D* RT)
	{
		return RT && RT->SizeX == SaltOceanFFT::Size && RT->SizeY == SaltOceanFFT::Size;
	}
}

namespace SaltOceanFFT
{
	bool FTargets::IsValid() const
	{
		for (int32 c = 0; c < NumCascades; ++c)
		{
			if (!Usable(Disp[c]) || !Usable(DispPrev[c]) || !Usable(Deriv[c]) || !Usable(Aux[c]))
			{
				return false;
			}
		}
		return true;
	}

	void Enqueue(const FFrame& Frame, const FTargets& Targets)
	{
		// Nothing to render in a commandlet (the cook loads the maps with a null RHI: a dispatch there trips an ensure).
		if (!Targets.IsValid() || !FApp::CanEverRender())
		{
			return;
		}
		const FSpectrum& S = Frame.Spectrum;
		FShaderFrame F;
		for (int32 c = 0; c < NumCascades; ++c)
		{
			const FCascade& C = GetCascade(c);
			const float Rot = FMath::DegreesToRadians(C.RotationDeg);
			F.CascadeA[c] = FVector4f(C.PatchSize, FMath::Cos(Rot), FMath::Sin(Rot), S.CascadeChop[c]);
			F.CascadeB[c] = FVector4f(C.KMin, C.KMax, S.CascadeGain[c], 0.f);
		}
		// The scale is a numerical integral: recompute only when the spectrum changes (game thread only).
		static FSpectrum CachedSpectrum;
		static float CachedScale = -1.f;
		if (CachedScale < 0.f || !CachedSpectrum.SameShape(S))
		{
			CachedScale = S.ComputeScale();
			CachedSpectrum = S;
		}
		const float Wind = FMath::DegreesToRadians(S.WindDirectionDeg);
		F.Spec0 = FVector4f(FMath::Cos(Wind), FMath::Sin(Wind), 2.f * UE_PI / FMath::Max(S.PeakWavelength, 1.f), CachedScale);
		F.Spec1 = FVector4f(S.Gamma, S.SpreadPeak, S.SpreadMin, S.ShortWaveLength > 0.f ? 2.f * UE_PI / S.ShortWaveLength : 0.f);
		F.TimeFrac = (float)(Frame.Time / LoopSeconds - FMath::FloorToDouble(Frame.Time / LoopSeconds));
		F.Seed = S.Seed;

		FRenderTargets T;
		for (int32 c = 0; c < NumCascades; ++c)
		{
			T.Disp[c] = Targets.Disp[c]->GameThread_GetRenderTargetResource();
			T.DispPrev[c] = Targets.DispPrev[c]->GameThread_GetRenderTargetResource();
			T.Deriv[c] = Targets.Deriv[c]->GameThread_GetRenderTargetResource();
			T.Aux[c] = Targets.Aux[c]->GameThread_GetRenderTargetResource();
		}
		ENQUEUE_RENDER_COMMAND(SaltOceanFFT)([F, T](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SaltOceanFFT"));
			AddPasses(GraphBuilder, F, T);
			GraphBuilder.Execute();
		});
	}
}

// Saltbound - render module: maps the project's shader folder for global shaders.

#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FSaltboundRenderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// /Saltbound/... -> <Project>/Shaders (SaltOceanFFT.usf, SaltOceanSpectrum.ush, SaltOcean.ush).
		AddShaderSourceDirectoryMapping(TEXT("/Saltbound"), FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders")));
	}
};

IMPLEMENT_MODULE(FSaltboundRenderModule, SaltboundRender)

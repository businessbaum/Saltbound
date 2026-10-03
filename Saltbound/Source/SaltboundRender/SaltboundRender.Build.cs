// Saltbound - render module: global compute shaders (FFT ocean). Loads at PostConfigInit, because global
// shader types must be registered before the global shader map is compiled.

using UnrealBuildTool;

public class SaltboundRender : ModuleRules
{
	public SaltboundRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "Projects" });
	}
}

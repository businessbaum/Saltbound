# Regenerates Visual Studio solution files (they are not version controlled).
. "$PSScriptRoot\Common.ps1"
$ubt = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'
& $ubt -projectfiles "-project=$ProjectFile" -game -rocket -progress
exit $LASTEXITCODE

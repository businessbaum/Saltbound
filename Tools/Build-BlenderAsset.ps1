# Runs a Blender asset build script headless, e.g.:  Tools/Build-BlenderAsset.ps1 build_test_crate
# The script writes Art/Blender/Source/<Asset>.blend and Art/Export/<Category>/<Asset>.fbx.
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Script,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ScriptArgs = @()
)
. "$PSScriptRoot\Common.ps1"
$path = Join-Path $RepoRoot "Art\Blender\Scripts\$Script.py"
if (-not (Test-Path $path)) { throw "Blender script not found: $path" }
& $BlenderExe -b --factory-startup -P $path -- @ScriptArgs
exit $LASTEXITCODE

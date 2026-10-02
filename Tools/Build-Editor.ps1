# Compiles the editor target (close the editor first; Live Coding blocks full builds).
param([string]$Configuration = 'Development')
. "$PSScriptRoot\Common.ps1"
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { Write-Warning 'UnrealEditor is running - the build will fail if the Saltbound module is loaded.' }
& $BuildBat SaltboundEditor Win64 $Configuration "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
exit $LASTEXITCODE

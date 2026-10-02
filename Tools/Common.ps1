# Shared paths for Saltbound tool scripts. Dot-source this file: . "$PSScriptRoot\Common.ps1"
$ErrorActionPreference = 'Stop'
$RepoRoot    = Split-Path -Parent $PSScriptRoot
$ProjectDir  = Join-Path $RepoRoot 'Saltbound'
$ProjectFile = Join-Path $ProjectDir 'Saltbound.uproject'
# Engine location: override with the SALTBOUND_ENGINE environment variable if needed.
$EngineRoot  = if ($env:SALTBOUND_ENGINE) { $env:SALTBOUND_ENGINE } else { 'C:\Program Files\Epic Games\UE_5.8' }
$EditorExe   = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$EditorCmd   = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$BuildBat    = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$RunUAT      = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$BlenderExe  = if ($env:SALTBOUND_BLENDER) { $env:SALTBOUND_BLENDER } else { 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' }

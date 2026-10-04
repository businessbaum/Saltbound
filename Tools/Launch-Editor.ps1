# Opens the Saltbound project in the Unreal Editor (MCP server and Python remote execution start automatically via project config).
param([string]$Map = '')
. "$PSScriptRoot\Common.ps1"
# A second editor on the same project locks the content files (saves fail with error 32, sharing violation).
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { Write-Output 'Unreal Editor is already running.'; exit 0 }
$argsList = @("`"$ProjectFile`"")
if ($Map) { $argsList += $Map }
Start-Process -FilePath $EditorExe -ArgumentList $argsList

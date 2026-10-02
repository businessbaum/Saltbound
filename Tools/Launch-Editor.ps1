# Opens the Saltbound project in the Unreal Editor (MCP server and Python remote execution start automatically via project config).
param([string]$Map = '')
. "$PSScriptRoot\Common.ps1"
$argsList = @("`"$ProjectFile`"")
if ($Map) { $argsList += $Map }
Start-Process -FilePath $EditorExe -ArgumentList $argsList

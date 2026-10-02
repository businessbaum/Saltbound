# Builds, cooks and stages a standalone Windows build into Builds/<Configuration>/Windows.
param([ValidateSet('Development', 'Shipping', 'DebugGame')][string]$Configuration = 'Development')
. "$PSScriptRoot\Common.ps1"
$archive = Join-Path $RepoRoot "Builds\$Configuration"
& $RunUAT BuildCookRun "-project=$ProjectFile" -noP4 -platform=Win64 "-clientconfig=$Configuration" `
    -build -cook -stage -pak -iostore -archive "-archivedirectory=$archive" -utf8output -unattended -nocompileeditor
exit $LASTEXITCODE

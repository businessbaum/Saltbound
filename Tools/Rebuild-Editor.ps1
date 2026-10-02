# Save dirty packages, quit the running editor, compile the editor target, and relaunch the editor.
# Use after C++ changes. Pass -NoLaunch to only build.
param([switch]$NoLaunch, [switch]$NoSave)
. "$PSScriptRoot\Common.ps1"

$editor = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($editor) {
    $save = if ($NoSave) { '' } else { 'unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)' }
    & python "$PSScriptRoot\ue_exec.py" -c "import unreal`n$save`nunreal.SystemLibrary.quit_editor()" | Out-Null
    if (-not $editor.WaitForExit(120000)) {
        throw 'Editor did not exit within 120 s (a dialog may be open). Close it manually and re-run.'
    }
    Start-Sleep -Seconds 2
}

& $BuildBat SaltboundEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
$code = $LASTEXITCODE
if ($code -ne 0) { Write-Error "Build failed ($code)"; exit $code }

if (-not $NoLaunch) {
    Start-Process -FilePath $EditorExe -ArgumentList @("`"$ProjectFile`"")
    # Wait for the MCP endpoint, which comes up once the editor has finished loading.
    $deadline = (Get-Date).AddMinutes(10)
    while ((Get-Date) -lt $deadline) {
        try { $c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 8000); $c.Close(); break } catch { Start-Sleep -Seconds 3 }
    }
    Start-Sleep -Seconds 5
    Write-Output 'Editor ready.'
}
exit 0

# Starts the packaged build, optionally captures a screenshot of its window after a delay, then closes it.
# Example (smoke test): Tools\Run-Build.ps1 -Seconds 30 -Screenshot Saved\smoke.png -ExecCmds "stat fps,stat unit"
param(
    [ValidateSet('Development', 'Shipping', 'DebugGame')][string]$Configuration = 'Development',
    [int]$Seconds = 0,
    [string]$Screenshot = '',
    [string]$ExecCmds = '',
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [switch]$Fullscreen
)
. "$PSScriptRoot\Common.ps1"
$exe = Join-Path $RepoRoot "Builds\$Configuration\Windows\Saltbound.exe"
if (-not (Test-Path $exe)) { throw "Build not found: $exe (run Tools\Package-Game.ps1)" }
$mode = if ($Fullscreen) { '-fullscreen' } else { '-windowed' }
$gameArgs = @($mode, "-resx=$ResX", "-resy=$ResY", '-log', '-nosplash')
if ($ExecCmds) { $gameArgs += "-ExecCmds=`"$ExecCmds`"" }
$proc = Start-Process -FilePath $exe -ArgumentList $gameArgs -PassThru
if ($Seconds -le 0) { return }

Start-Sleep -Seconds $Seconds
if ($Screenshot) {
    Add-Type -AssemblyName System.Drawing
    Add-Type @"
using System; using System.Runtime.InteropServices;
public static class SbWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
    $game = Get-Process -Name 'Saltbound*' | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($game) {
        [SbWin]::SetForegroundWindow($game.MainWindowHandle) | Out-Null
        Start-Sleep -Milliseconds 500
        $r = New-Object SbWin+RECT
        [SbWin]::GetWindowRect($game.MainWindowHandle, [ref]$r) | Out-Null
        $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
        $bmp.Save($Screenshot, [System.Drawing.Imaging.ImageFormat]::Png)
        $g.Dispose(); $bmp.Dispose()
        Write-Output "Screenshot: $Screenshot"
    } else { Write-Warning 'Game window not found' }
}
Get-Process -Name 'Saltbound*' -ErrorAction SilentlyContinue | ForEach-Object { $null = $_.CloseMainWindow() }
Start-Sleep -Seconds 3
Get-Process -Name 'Saltbound*' -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output 'Build closed.'

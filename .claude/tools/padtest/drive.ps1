param([string]$Exe, [string]$Dir, [string]$OutPrefix, [string]$Steps)
# Drives the game with keys POSTED to its window (no focus stealing) and captures the window's own
# content with PrintWindow. Steps: "wait:6;key:O;wait:2;shot:prefs;key:M;wait:1.5;shot:mods"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
}
"@
function Shot($h, $path) {
  $r = New-Object W+RECT
  [W]::GetClientRect($h, [ref]$r) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  [W]::PrintWindow($h, $hdc, 3) | Out-Null
  $g.ReleaseHdc($hdc)
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  "shot $path"
}
function Key($h, [string]$k) {
  $vk = switch ($k) { 'ESC' { 0x1B } 'ENTER' { 0x0D } 'UP' { 0x26 } 'DOWN' { 0x28 } 'LEFT' { 0x25 } 'RIGHT' { 0x27 } default { [int][char]$k.ToUpper() } }
  $sc = [W]::MapVirtualKey($vk, 0)
  $ext = if ($vk -in 0x25, 0x26, 0x27, 0x28) { 1 -shl 24 } else { 0 }
  $down = [IntPtr](1 -bor ($sc -shl 16) -bor $ext)
  $up = [IntPtr]([int64](1 -bor ($sc -shl 16) -bor $ext) -bor 0xC0000000)
  [W]::PostMessage($h, 0x0100, [IntPtr]$vk, $down) | Out-Null
  Start-Sleep -Milliseconds 80
  [W]::PostMessage($h, 0x0101, [IntPtr]$vk, $up) | Out-Null
  "key $k"
}
$p = Start-Process -FilePath $Exe -WorkingDirectory $Dir -PassThru
$h = [IntPtr]::Zero
foreach ($step in $Steps.Split(';')) {
  $kv = $step.Split(':', 2)
  $p.Refresh()
  if ($p.HasExited) { "EXITED early, code $($p.ExitCode)"; exit 1 }
  if ($h -eq [IntPtr]::Zero) { $h = $p.MainWindowHandle }
  switch ($kv[0]) {
    'wait' { Start-Sleep -Milliseconds ([double]$kv[1] * 1000) }
    'key' { Key $h $kv[1] }
    'shot' { Shot $h "$OutPrefix$($kv[1]).png" }
  }
}
$p.Refresh()
if ($p.HasExited) { "EXITED, code $($p.ExitCode)" } else { Stop-Process -Id $p.Id -Force; "stopped" }

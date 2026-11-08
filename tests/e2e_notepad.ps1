<#
  Stilleto end-to-end test: global hotkey -> microphone capture -> local whisper -> insertion into Notepad.

  Runs the real application. Because no human speaks during an automated run, the developer hook
  STILLETO_TEST_AUDIO substitutes whisper.cpp's JFK sample for the captured audio after the real
  microphone capture; everything else (hotkey, device open/close, state machine, worker thread,
  insertion, focus handling, floating widget) is the production path.

  Usage:  powershell -ExecutionPolicy Bypass -File tests\e2e_notepad.ps1 [-Exe <path>] [-Shots <dir>]
  Do not type or click while it runs.
#>
param(
    [string]$Exe = "$PSScriptRoot\..\out\build\vs2026\Stilleto_artefacts\Release\Stilleto.exe",
    [string]$Wav = "$PSScriptRoot\..\external\whisper.cpp\samples\jfk.wav",
    [string]$Shots = "",
    [string]$Expect = "ask not what your country can do for you",   # regex the inserted text must match
    [string]$Forbid = ""                                             # regex it must NOT match (e.g. fillers)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices; using System.Text;
public class E2E {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO { public int cbSize, flags; public IntPtr active, focus, capture, menuOwner, moveSize, caret; public RECT caretRect; }
  [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint tid, ref GUITHREADINFO info);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageTimeout(IntPtr h, uint msg, IntPtr w, StringBuilder l, uint flags, uint timeout, out IntPtr result);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr h, uint msg, IntPtr w, IntPtr l, uint flags, uint timeout, out IntPtr result);

  public static void Key(byte vk, bool down) { keybd_event(vk, 0, down ? 0u : 2u, UIntPtr.Zero); }
  public static void Hotkey() { Key(0x11,true); Key(0x12,true); Key(0x20,true); Key(0x20,false); Key(0x12,false); Key(0x11,false); }
  public static void Nudge() { Key(0xE8,true); Key(0xE8,false); }
  public static string Title(IntPtr h) { var sb = new StringBuilder(512); GetWindowText(h, sb, 512); return sb.ToString(); }
  public static string EditorText(IntPtr window) {
    uint pid; uint tid = GetWindowThreadProcessId(window, out pid);
    var info = new GUITHREADINFO(); info.cbSize = Marshal.SizeOf(info);
    if (!GetGUIThreadInfo(tid, ref info) || info.focus == IntPtr.Zero) return null;
    IntPtr len; SendMessageTimeout(info.focus, 0x000E, IntPtr.Zero, IntPtr.Zero, 2, 2000, out len);
    var sb = new StringBuilder(len.ToInt32() + 1); IntPtr copied;
    SendMessageTimeout(info.focus, 0x000D, (IntPtr)(len.ToInt32() + 1), sb, 2, 2000, out copied);
    return sb.ToString();
  }
}
'@
[void][E2E]::SetProcessDPIAware()

function Shot([string]$name) {
    if (-not $Shots) { return }
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen(0, 0, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path $Shots "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
}
Add-Type -AssemblyName System.Windows.Forms

$results = [ordered]@{}
function Check([string]$name, [bool]$ok, [string]$detail = "") {
    $results[$name] = $ok
    Write-Output ("[{0}] {1} {2}" -f ($(if ($ok) { " OK " } else { "FAIL" })), $name, $detail)
}

# 1. Fresh Stilleto instance with the test hook, started to the tray.
Get-Process Stilleto -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
$env:STILLETO_TEST_AUDIO = (Resolve-Path $Wav).Path
$app = Start-Process (Resolve-Path $Exe).Path -ArgumentList "--minimized" -PassThru
Remove-Item Env:STILLETO_TEST_AUDIO
Start-Sleep -Seconds 4   # model load

# 2. Notepad on a temporary file, in the foreground.
$file = Join-Path $env:TEMP ("stilleto-e2e-{0}.txt" -f [Environment]::TickCount)
Set-Content -Path $file -Value "" -NoNewline
Start-Process notepad.exe $file
$stem = [IO.Path]::GetFileNameWithoutExtension($file)
$notepad = [IntPtr]::Zero
for ($i = 0; $i -lt 100 -and $notepad -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 150
    foreach ($p in Get-Process notepad -ErrorAction SilentlyContinue) {
        if ($p.MainWindowTitle -like "*$stem*") { $notepad = $p.MainWindowHandle }
    }
}
if ($notepad -eq [IntPtr]::Zero) { throw "Notepad window not found" }
for ($i = 0; $i -lt 10 -and [E2E]::GetForegroundWindow() -ne $notepad; $i++) { [E2E]::Nudge(); [void][E2E]::SetForegroundWindow($notepad); Start-Sleep -Milliseconds 200 }
Start-Sleep -Milliseconds 800
Check "notepad in foreground" ([E2E]::GetForegroundWindow() -eq $notepad)

# 3. Hotkey -> record for 2.5 s -> hotkey.
[E2E]::Hotkey()
Start-Sleep -Milliseconds 1500
Shot "e2e-recording"
Check "focus kept while recording (widget did not activate)" ([E2E]::GetForegroundWindow() -eq $notepad) ([E2E]::Title([E2E]::GetForegroundWindow()))
Start-Sleep -Milliseconds 1000
[E2E]::Hotkey()
Start-Sleep -Milliseconds 300
Shot "e2e-processing"

# 4. Wait for transcription + insertion.
$text = ""
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt 30) {
    Start-Sleep -Milliseconds 250
    $text = [E2E]::EditorText($notepad)
    if ($text.Trim().Length -gt 0) { Start-Sleep -Milliseconds 400; $text = [E2E]::EditorText($notepad); break }
}
$elapsed = $sw.Elapsed.TotalSeconds
Shot "e2e-completed"
Check "transcript inserted into Notepad" ($text -match $Expect) ("after {0:N1}s: '{1}'" -f $elapsed, $text)
Check "inserted exactly once" (([regex]::Matches($text, $Expect)).Count -eq 1)
if ($Forbid) { Check "disfluencies removed" (-not ($text -match $Forbid)) "forbidden pattern: $Forbid" }
Check "focus still on Notepad after insertion" ([E2E]::GetForegroundWindow() -eq $notepad)

# 5. Log evidence of the real microphone capture and worker-thread transcription.
$log = Get-Content "$env:APPDATA\Stilleto\stilleto.log" -Tail 12
$log | ForEach-Object { Write-Output "    log: $_" }
Check "microphone captured audio" (($log -join "`n") -match "Recording stopped: [1-9]")

# 6. Clean up: save the temp file (so closing never prompts) and close the tab, only if Notepad is foreground.
if ([E2E]::GetForegroundWindow() -eq $notepad) {
    [E2E]::Key(0x11,$true); [E2E]::Key(0x53,$true); [E2E]::Key(0x53,$false); [E2E]::Key(0x11,$false); Start-Sleep -Milliseconds 500
    [E2E]::Key(0x11,$true); [E2E]::Key(0x57,$true); [E2E]::Key(0x57,$false); [E2E]::Key(0x11,$false); Start-Sleep -Milliseconds 500
}
Remove-Item $file -ErrorAction SilentlyContinue
Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue

$failed = @($results.Values | Where-Object { -not $_ }).Count
Write-Output ("`n{0} checks, {1} failed" -f $results.Count, $failed)
exit $failed

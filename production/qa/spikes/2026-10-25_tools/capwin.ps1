param([string]$TitleMatch, [string]$OutPath)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public class CW { public delegate bool EnumProc(IntPtr h, IntPtr l);
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
[DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
public struct RECT { public int L,T,R,B; }
public static IntPtr Find(string m){ IntPtr found=IntPtr.Zero; EnumWindows((h,l)=>{ if(IsWindowVisible(h)){ var sb=new StringBuilder(256); GetWindowText(h,sb,256); if(sb.ToString().Contains(m)){ found=h; return false; } } return true; }, IntPtr.Zero); return found; } }
"@
$h = [CW]::Find($TitleMatch)
if ($h -eq [IntPtr]::Zero) { "NOTFOUND $TitleMatch"; exit 1 }
[CW]::keybd_event(0x12,0,0,[UIntPtr]::Zero); [CW]::keybd_event(0x12,0,2,[UIntPtr]::Zero)
[CW]::ShowWindow($h,9) | Out-Null; [CW]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 600
$r = New-Object CW+RECT; [CW]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L; $hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $hh))
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
"saved $OutPath ${w}x${hh}"

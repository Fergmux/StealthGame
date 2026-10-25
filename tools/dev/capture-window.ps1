param(
  [Parameter(Mandatory)] [string]$TitleMatch,   # e.g. "Client 1"
  [Parameter(Mandatory)] [string]$Out,          # PNG path
  [int]$Width = 650, [int]$Height = 530         # crop from the window's top-left
)
# Captures an Unreal Editor window (e.g. a PIE client) with PrintWindow, even when it is behind other windows.
# Requires Editor Preferences > Performance > "Use Less CPU when in Background" OFF, or frames go stale.
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Text; using System.Drawing; using System.Runtime.InteropServices;
public class StealthWin {
  public delegate bool EP(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EP f, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  public static IntPtr Find(uint pid, string match) { IntPtr found = IntPtr.Zero; EnumWindows((h,l)=>{ uint p; GetWindowThreadProcessId(h,out p); if(p==pid && IsWindowVisible(h)){ var sb=new StringBuilder(256); GetWindowText(h,sb,256); if(sb.ToString().Contains(match)){ found=h; return false; } } return true; }, IntPtr.Zero); return found; }
  public static void Cap(IntPtr h, string path, int w, int ht) { var full=new Bitmap(2600,1600); using(var g=Graphics.FromImage(full)){ var dc=g.GetHdc(); PrintWindow(h,dc,2); g.ReleaseHdc(dc);} var crop=new Bitmap(w,ht); using(var g=Graphics.FromImage(crop)){ g.DrawImage(full,new Rectangle(0,0,w,ht),new Rectangle(0,0,w,ht),GraphicsUnit.Pixel);} crop.Save(path); }
}
"@
$proc = Get-Process UnrealEditor -ErrorAction Stop | Select-Object -First 1
$h = [StealthWin]::Find([uint32]$proc.Id, $TitleMatch)
if ($h -eq [IntPtr]::Zero) { "no window matching '$TitleMatch'"; exit 1 }
[StealthWin]::Cap($h, $Out, $Width, $Height)
"captured $Out"

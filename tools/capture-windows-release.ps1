param([string]$Executable='dist/1.2.0/x64/Look@LAN-ng.exe',[string]$Output='docs/images/windows-1.2.0.png')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing.Common
$looklanCode = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class LooklanCapture {
 public delegate bool Callback(IntPtr window,IntPtr data);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback,IntPtr data);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,System.Text.StringBuilder text,int capacity);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent,IntPtr after,string cls,string title);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window,int show);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
 [StructLayout(LayoutKind.Sequential)] struct Rect {public int left,top,right,bottom;}
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr window,IntPtr dc,uint flags);
 public static IntPtr Find(uint pid){IntPtr result=IntPtr.Zero;EnumWindows((window,data)=>{uint owner;GetWindowThreadProcessId(window,out owner);if(owner==pid){var name=new System.Text.StringBuilder(128);GetClassName(window,name,name.Capacity);if(name.ToString()=="LookAtLANngWindow"){result=window;return false;}}return true;},IntPtr.Zero);return result;}
 public static void Capture(IntPtr window,string path){var previousDpi=SetThreadDpiAwarenessContext(new IntPtr(-4));try{Rect rect;if(!GetWindowRect(window,out rect))throw new Exception("No window rectangle");using(var bitmap=new Bitmap(rect.right-rect.left,rect.bottom-rect.top)){using(var graphics=Graphics.FromImage(bitmap)){var dc=graphics.GetHdc();try{if(!PrintWindow(window,dc,2))throw new Exception("PrintWindow failed");}finally{graphics.ReleaseHdc(dc);}}bitmap.Save(path,ImageFormat.Png);}}finally{SetThreadDpiAwarenessContext(previousDpi);}}
}
'@
$looklanReferences=@('System.Drawing.Common','System.Drawing.Primitives')+@(Get-ChildItem -LiteralPath $PSHOME -Filter 'System.Private.Windows*.dll' | Select-Object -ExpandProperty FullName)
Add-Type -TypeDefinition $looklanCode -ReferencedAssemblies $looklanReferences
$looklanExe=(Resolve-Path -LiteralPath $Executable).Path
$looklanOutput=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Output))
New-Item -ItemType Directory -Force (Split-Path $looklanOutput) > $null
$looklanSettings=Join-Path (Split-Path $looklanExe) 'Look@LAN-ng.ini'
$looklanHadSettings=Test-Path -LiteralPath $looklanSettings
$looklanProcess=Start-Process -FilePath $looklanExe -PassThru -WindowStyle Hidden
$looklanWindow=[IntPtr]::Zero
try {
 $looklanDeadline=[DateTime]::UtcNow.AddSeconds(10)
 while($looklanWindow -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $looklanDeadline){$looklanWindow=[LooklanCapture]::Find($looklanProcess.Id);Start-Sleep -Milliseconds 100}
 if($looklanWindow -eq [IntPtr]::Zero){throw 'Owned application window not found'}
 [LooklanCapture]::SetWindowPos($looklanWindow,[IntPtr]::Zero,40,40,1600,900,20) > $null
 [LooklanCapture]::ShowWindow($looklanWindow,4) > $null
 [LooklanCapture]::SendMessage($looklanWindow,0x111,[IntPtr]101,[IntPtr]::Zero) > $null
 $looklanToolbar=[LooklanCapture]::FindWindowEx($looklanWindow,[IntPtr]::Zero,'ToolbarWindow32',$null)
 $looklanDeadline=[DateTime]::UtcNow.AddSeconds(120)
 Start-Sleep -Milliseconds 500
 while([LooklanCapture]::SendMessage($looklanToolbar,0x409,[IntPtr]101,[IntPtr]::Zero) -eq [IntPtr]::Zero){if([DateTime]::UtcNow -ge $looklanDeadline){throw 'Real scan timed out'};Start-Sleep -Milliseconds 250}
 Start-Sleep -Milliseconds 1500
 [LooklanCapture]::Capture($looklanWindow,$looklanOutput)
 Write-Output $looklanOutput
} finally {
 if($looklanWindow -ne [IntPtr]::Zero){[LooklanCapture]::SendMessage($looklanWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero) > $null}
 if(!$looklanProcess.WaitForExit(10000)){$looklanProcess.Kill();$looklanProcess.WaitForExit()}
 if(!$looklanHadSettings -and (Test-Path -LiteralPath $looklanSettings)){Remove-Item -LiteralPath $looklanSettings}
}

param([Parameter(Mandatory)][string]$Executable,[string]$OutputDirectory='build/validation/windows-profile-session')
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices;
public static class ProfileSessionUI {
 public delegate bool Callback(IntPtr h,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback c,IntPtr p);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool SetDlgItemText(IntPtr h,int id,string text);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h,Callback c,IntPtr p);
 [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr h);
 [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] static extern IntPtr GetMenu(IntPtr h);
 [DllImport("user32.dll")] static extern int GetMenuItemCount(IntPtr h);
 [DllImport("user32.dll")] static extern uint GetMenuItemID(IntPtr h,int index);
 [DllImport("user32.dll")] static extern IntPtr GetSubMenu(IntPtr h,int index);
 [DllImport("user32.dll")] static extern uint GetMenuState(IntPtr h,uint id,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr h,uint msg,IntPtr w,string text);
 [DllImport("user32.dll",EntryPoint="SendMessageW")] static extern IntPtr KeyMessage(IntPtr h,uint msg,IntPtr w,IntPtr l);
 static void TypeFilename(IntPtr h,string path){KeyMessage(h,0xB1,IntPtr.Zero,new IntPtr(-1));KeyMessage(h,0x102,new IntPtr(8),new IntPtr(1));foreach(char c in path)KeyMessage(h,0x102,new IntPtr(c),new IntPtr(1));}
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p,IntPtr a,string cls,string text);
 public static IntPtr Find(uint pid,string cls){IntPtr found=IntPtr.Zero;EnumWindows((h,p)=>{uint owner;GetWindowThreadProcessId(h,out owner);var b=new StringBuilder(128);GetClassName(h,b,b.Capacity);if(owner==pid && b.ToString()==cls){found=h;return false;}return true;},IntPtr.Zero);return found;}
 public static bool Filename(IntPtr dialog,string path){bool found=false;EnumChildWindows(dialog,(h,p)=>{var b=new StringBuilder(128);GetClassName(h,b,b.Capacity);if(b.ToString()=="Edit"){if(GetDlgCtrlID(h)==1001){TypeFilename(h,path);found=true;return false;}for(IntPtr a=h;a!=IntPtr.Zero && a!=dialog;a=GetParent(a)){int id=GetDlgCtrlID(a);if(id==1148 || id==1152){TypeFilename(h,path);found=true;return false;}}}return true;},IntPtr.Zero);return found;}
 public static string Controls(IntPtr dialog){var output=new StringBuilder();EnumChildWindows(dialog,(h,p)=>{var b=new StringBuilder(128);GetClassName(h,b,b.Capacity);output.Append(b+":"+GetDlgCtrlID(h)+" ");return true;},IntPtr.Zero);return output.ToString();}
 static bool Checked(IntPtr menu,uint id){for(int i=0;i<GetMenuItemCount(menu);i++){if(GetMenuItemID(menu,i)==id)return (GetMenuState(menu,(uint)i,0x400)&8)!=0;IntPtr sub=GetSubMenu(menu,i);if(sub!=IntPtr.Zero && Checked(sub,id))return true;}return false;}
 public static bool ThemeChecked(IntPtr window,uint id){return Checked(GetMenu(window),id);}
}
'@
$base=[IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
if(Test-Path -LiteralPath $base){throw 'Use a fresh test directory'}
New-Item -ItemType Directory $base > $null
$exe=Join-Path $base 'Look@LAN-ng.exe';Copy-Item -LiteralPath (Resolve-Path -LiteralPath $Executable).Path -Destination $exe
$ini=Join-Path $base 'Look@LAN-ng.ini';$profiles=Join-Path $base 'profiles';$checks=[Collections.Generic.List[string]]::new()
function Check($condition,[string]$label){if(!$condition){throw $label};$checks.Add($label);Write-Output "PASS $label"}
function Wait-For($predicate,[int]$seconds=15){$deadline=[DateTime]::UtcNow.AddSeconds($seconds);do{$value=& $predicate;if($value -is [IntPtr]){if($value -ne [IntPtr]::Zero){return $value}}elseif($value){return $value};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline);throw 'UI test timed out'}
function Launch { $script:p=Start-Process -FilePath $exe -PassThru -WindowStyle Hidden;$script:w=Wait-For { [ProfileSessionUI]::Find($p.Id,'LookAtLANngWindow') };Wait-For {$script:toolbar=[ProfileSessionUI]::FindWindowEx($w,[IntPtr]::Zero,'ToolbarWindow32',$null);$script:list=[ProfileSessionUI]::FindWindowEx($w,[IntPtr]::Zero,'SysListView32',$null);return $toolbar-ne[IntPtr]::Zero -and $list-ne[IntPtr]::Zero -and ([ProfileSessionUI]::ThemeChecked($w,160) -or [ProfileSessionUI]::ThemeChecked($w,161) -or [ProfileSessionUI]::ThemeChecked($w,162))}>$null }
function Command([int]$id){[ProfileSessionUI]::PostMessage($w,0x111,[IntPtr]$id,[IntPtr]::Zero)>$null}
function Close-App { [ProfileSessionUI]::PostMessage($w,0x10,[IntPtr]::Zero,[IntPtr]::Zero)>$null;if(!$p.WaitForExit(10000)){throw 'Application close timed out'};Check ($p.ExitCode -eq 0) ('Clean close, exit '+$p.ExitCode) }
function Scan {Command 101;Start-Sleep -Milliseconds 200;Wait-For { [ProfileSessionUI]::SendMessage($toolbar,0x409,[IntPtr]101,[IntPtr]::Zero) -ne [IntPtr]::Zero } 120 >$null;Check ([ProfileSessionUI]::SendMessage($list,0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -gt 0) 'Real LAN scan found hosts'}
function Dark-Table { $color=[ProfileSessionUI]::SendMessage($list,0x1000,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64();return (($color -band 255) -lt 96 -and (($color -shr 8) -band 255) -lt 96 -and (($color -shr 16) -band 255) -lt 96) }
function Set-Theme([int]$id){Command $id;Wait-For {[ProfileSessionUI]::ThemeChecked($w,[uint32]$id)}> $null}
try {
 Launch;Check ([ProfileSessionUI]::ThemeChecked($w,160)) 'Default theme mode is System';Set-Theme 162;Wait-For {Dark-Table}>$null;Check ([ProfileSessionUI]::ThemeChecked($w,162)) 'View menu selects Dark';Command 104;Scan;Close-App
 Check (!(Test-Path -LiteralPath $ini) -and !(Test-Path -LiteralPath $profiles)) 'Temporary launch/language/scan creates no INI or profiles folder'
 Launch;Check ([ProfileSessionUI]::ThemeChecked($w,160)) 'Temporary theme override is not persisted';Set-Theme 162;Wait-For {Dark-Table}>$null;Scan;Command 141
 $dialog=Wait-For {[ProfileSessionUI]::Find($p.Id,'#32770')}
 Start-Sleep -Milliseconds 700
 $chosen=Join-Path $base 'Office.lanprofile'
 Wait-For {[ProfileSessionUI]::Filename($dialog,$chosen)} >$null
 Check $true 'Native save dialog filename'
 [ProfileSessionUI]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)>$null
 $profile=Join-Path $profiles 'Office.lanprofile';$history=$profile+'.lanhistory'
 Wait-For {(Test-Path -LiteralPath $ini) -and (Test-Path -LiteralPath $profile) -and (Test-Path -LiteralPath $history)} >$null
 Command 105;Close-App
 Check ([bool](Get-Content -LiteralPath $ini|Select-String '^Theme=dark$')) 'Saved profile permits persistent Theme=dark'
 Check ((Get-Content -LiteralPath $profile -First 1) -eq 'Look@LAN-ngProfile=2') 'Profile v2 saved in profiles'
 $bytes=[IO.File]::ReadAllBytes($history);$count=[BitConverter]::ToUInt32($bytes,12);$sequence=[BitConverter]::ToUInt64($bytes,32)
 Check ($count -gt 0 -and $sequence -gt 0) 'Hosts and event history persisted'
 Launch
 Wait-For {[ProfileSessionUI]::SendMessage($list,0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -eq $count} >$null
 Check $true 'Only profile autoloads and restores host table without scanning'
 Check ((Dark-Table) -and [ProfileSessionUI]::ThemeChecked($w,162)) 'Dark mode restored with saved settings'
 Check ([ProfileSessionUI]::SendMessage($toolbar,0x409,[IntPtr]101,[IntPtr]::Zero)-ne[IntPtr]::Zero) 'Scan button re-enabled after history load'
 Scan;Close-App
 $after=[IO.File]::ReadAllBytes($history)
 Check ([BitConverter]::ToUInt64($after,32) -gt $sequence) 'Events continue across restart'
 Check (@(Get-ChildItem -LiteralPath $profiles -Filter '*.tmp-*').Count -eq 0) 'No leftover history temporary files'
 Launch;Set-Theme 161;Wait-For {!(Dark-Table)}> $null;Check ([ProfileSessionUI]::ThemeChecked($w,161)) 'View menu switches to Light';Close-App
 Launch;Check ([ProfileSessionUI]::ThemeChecked($w,161)) 'Explicit Light mode persists';Set-Theme 160;Check ([ProfileSessionUI]::ThemeChecked($w,160)) 'View menu returns to System';Close-App
 Check ([bool](Get-Content -LiteralPath $ini|Select-String '^Theme=system$')) 'System preference persists instead of the effective palette'
 [ordered]@{exe_sha256=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant();checks=$checks} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $base 'results.json') -Encoding utf8
} finally {
 if($p -and !$p.HasExited){[ProfileSessionUI]::PostMessage($w,0x10,[IntPtr]::Zero,[IntPtr]::Zero)>$null;if(!$p.WaitForExit(10000)){$p.Kill()}}
}

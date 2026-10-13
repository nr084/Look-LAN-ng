"""Run the prebuilt GTK application on Linux in an isolated D-Bus/Xvfb session.

Dependencies: python3-pyatspi, python3-xlib, python3-pil, xvfb, xdotool, openbox.
Example: dbus-run-session -- python3 test-linux-gui.py --binary ./LookLAN.sh
Compilation must be performed on the Windows build host before this test.
"""
import argparse,os,sys,time,subprocess,json,pathlib,tempfile,pyatspi
from Xlib import X,display,protocol
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary',type=pathlib.Path,required=True)
parser.add_argument('--screenshot-only',action='store_true')
parser.add_argument('--output-dir',type=pathlib.Path,default=pathlib.Path(tempfile.gettempdir()))
args=parser.parse_args();binary=args.binary.resolve()
root=pathlib.Path(tempfile.mkdtemp(prefix='looklan-gui-',dir=args.output_dir.resolve()))
nic_data=json.loads(subprocess.check_output(['ip','-j','-4','address','show'],text=True))
own=next(a['local'] for n in nic_data if n['ifname']!='lo' and n['operstate']=='UP' for a in n['addr_info'] if a['family']=='inet')
import ipaddress
subnet=str(ipaddress.ip_network(own+'/24',strict=False))
os.environ['XDG_CONFIG_HOME']=str(root/'gui-config')
print('Test artifacts:',root,flush=True)
passed=[]
def check(ok,name):
 if not ok: raise AssertionError(name)
 passed.append(name);print('PASS',name,flush=True)
def wait(fn,seconds=10):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  try:
   v=fn()
   if v:return v
  except Exception:pass
  time.sleep(.1)
 raise AssertionError('Timed out: '+repr(fn))
def descendants(a):
 yield a
 for b in a:
  try:yield from descendants(b)
  except Exception:continue
def nodes():
 result=[]
 for a in descendants(pyatspi.Registry.getDesktop(0)):
  try:
   if a.getRoleName()!='invalid':result.append(a)
  except Exception:continue
 return result
def find(role,name=None):
 for a in nodes():
  if a.getRoleName()==role and (name is None or a.name==name):return a
 raise LookupError((role,name))
def click(name):
 try:a=find('button',name)
 except LookupError:a=find('menu item',name)
 a.queryAction().doAction(0);time.sleep(.15)
def inputs():return [a for a in nodes() if a.getRoleName()=='text' and a.getState().contains(pyatspi.STATE_EDITABLE)]
def set_range(s):inputs()[0].queryEditableText().setTextContents(s)
def cells():
 t=find('table').queryTable()
 result=[]
 states={'ONLINE','OFFLINE','NEW','CHANGED','UNKNOWN','В СЕТИ','НЕ В СЕТИ','НОВЫЙ','ИЗМЕНЁН','НЕИЗВЕСТНО'}
 for r in range(t.nRows):
  row=[]
  for c in range(t.nColumns):
   cell=t.getAccessibleAt(r,c);value=cell.name.strip()
   if c==0:value=next((a.name.strip() for a in descendants(cell) if a.name.strip() in states),value)
   row.append(value)
  result.append(row)
 return result
def ready():return any(a.getRoleName()=='label' and a.name.startswith(('Ready |','Готово |')) for a in nodes())
def close():
 # Ask the window manager to close the exact process window, as a user does.
 # Hidden GTK leader windows and old accessibility objects are not targets.
 wid=subprocess.check_output(['xdotool','search','--onlyvisible','--all','--pid',str(p.pid),'--name','^Look@LAN-ng']).split()[-1].decode()
 subprocess.run(['xdotool','windowactivate','--sync',wid,'key','alt+F4'],check=True)

def start(lang='en_US.UTF-8',caption='Scan'):
 env=os.environ.copy();env['LANG']=lang;env['LC_ALL']=lang
 f=open(root/'gui.log','a');p=subprocess.Popen([str(binary)],env=env,stdout=f,stderr=f)
 def mapped():
  d=display.Display()
  try:
   for w in d.screen().root.query_tree().children:
    children=[w]+list(w.query_tree().children)
    for child in children:
     prop=child.get_full_property(d.intern_atom('_NET_WM_PID'),X.AnyPropertyType)
     if prop is not None and int(prop.value[0])==p.pid and child.get_attributes().map_state==X.IsViewable:return True
   return False
  finally:d.close()
 wait(mapped);time.sleep(.3);wait(lambda:find('button',caption));return p
def choose_language(name):
 combo=[a for a in nodes() if a.getRoleName()=='combo box' and a.name in ('System / Система','English','Русский')][0]
 combo.queryAction().doAction(0);time.sleep(.1)
 find('menu item',name).queryAction().doAction(0);time.sleep(.2)
def file_dialog(path):
 time.sleep(.4)
 windows=subprocess.check_output(['xdotool','search','--onlyvisible','--name','^(Save|Open profile)$']).split()
 if windows:subprocess.run(['xdotool','windowfocus',windows[-1].decode()],check=True)
 subprocess.run(['xdotool','key','ctrl+l'],check=True);time.sleep(.2)
 subprocess.run(['xdotool','key','ctrl+a'],check=True)
 subprocess.run(['xdotool','type','--clearmodifiers',str(path)],check=True)
 time.sleep(.4)
 geo=subprocess.check_output(['xdotool','getwindowgeometry','--shell',windows[-1].decode()],text=True)
 g=dict(line.split('=',1) for line in geo.splitlines())
 subprocess.run(['xdotool','mousemove','--window',windows[-1].decode(),str(int(g['WIDTH'])-50),str(int(g['HEIGHT'])-25),'click','1'],check=True)
 time.sleep(.4)
 time.sleep(1.5)
 if not pathlib.Path(path).exists():
  from PIL import ImageGrab
  ImageGrab.grab().save(root/'dialog.png')
  for a in nodes():
   try:print(a.getRoleName(),repr(a.name),a.queryText().getText(0,-1) if a.getRoleName()=='text' else '',flush=True)
   except Exception:pass
def dialog_window(title):
 return wait(lambda:subprocess.check_output(['xdotool','search','--onlyvisible','--name','^'+title+'$']).split()[-1].decode())
def fill_dialog(title,values):
 wid=dialog_window(title);subprocess.run(['xdotool','windowfocus',wid],check=True)
 for i,value in enumerate(values):
  subprocess.run(['xdotool','key','ctrl+a'],check=True);subprocess.run(['xdotool','type','--clearmodifiers',value],check=True)
  if i+1<len(values):subprocess.run(['xdotool','key','Tab'],check=True)
 subprocess.run(['xdotool','key','Return'],check=True);time.sleep(.5)
 wait(lambda:subprocess.run(['xdotool','search','--onlyvisible','--name','^'+title+'$'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0)
def selected_ip(ip):
 r=next(i for i,row in enumerate(cells()) if row[1]==ip);find('table').queryTable().addRowSelection(r);time.sleep(.15)
def right_click(ip,label):
 r=next(i for i,row in enumerate(cells()) if row[1]==ip);rect=find('table').queryTable().getAccessibleAt(r,1).queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
 subprocess.run(['xdotool','mousemove',str(rect.x+20),str(rect.y+rect.height//2),'click','3'],check=True);time.sleep(.2);find('menu item',label).queryAction().doAction(0);time.sleep(.3)
x=subprocess.Popen(['Xvfb','-displayfd','1','-screen','0','1280x800x24','-nolisten','tcp'],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
os.environ['DISPLAY']=':'+x.stdout.readline().strip()
p=None;wm=None
try:
 time.sleep(1);wm=subprocess.Popen(['openbox'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);time.sleep(.5)
 for path in root.glob('export.*'):path.unlink()
 (root/'saved.lanprofile').unlink(missing_ok=True)
 p=start();check(ready(),'English system locale startup')
 if args.screenshot_only:
  click('Scan');wait(ready,180);selected_ip(own)
  from PIL import ImageGrab
  ImageGrab.grab().save(root/'release-window.png');check(True,'Real default-subnet scan release screenshot');close();check(p.wait(timeout=5)==0,'Screenshot application closes cleanly');p=None;sys.exit(0)

 set_range(own);click('Scan');wait(ready)
 check(any(r[1]==own and r[0]=='ONLINE' and r[2] and r[3] for r in cells()),'Own IP real scan, name and MAC')
 from PIL import ImageGrab
 ImageGrab.grab().save(root/'main-window.png')
 for j in range(3):
  click('Scan');wait(ready)
 check(len(cells())==1,'Repeat scan deduplicates host')
 inputs()[1].queryEditableText().setTextContents('no-such-host');time.sleep(.2);check(len(cells())==0,'Filter hides nonmatching host')
 inputs()[1].queryEditableText().setTextContents('');time.sleep(.2);check(len(cells())==1,'Filter restores host')
 for ext in ['csv','txt','html','HTML']:
  click('Export');file_dialog(root/('export.'+ext));check((root/('export.'+ext)).exists(),'Export '+ext)
 click('Save profile');file_dialog(root/'saved.lanprofile');check((root/'saved.lanprofile').exists(),'Save UTF-8 profile')
 wait(lambda:any(a.name=='Default' for a in nodes()));check(True,'Saved profile appears in sidebar');click('Open profile');file_dialog(root/'saved.lanprofile');wait(ready);check(inputs()[0].queryText().getText(0,-1)==own+'-'+own,'Open saved profile restores range')
 data=(root/'export.HTML').read_text(encoding='utf-8');check('<html' in data.lower(),'Uppercase HTML extension selects HTML')
 gateway=subprocess.check_output(['ip','-j','route','show','default'],text=True);gateway=json.loads(gateway)[0]['gateway']
 set_range(gateway);click('Scan');wait(ready,20);check(any(r[1]==gateway and r[0]=='ONLINE' for r in cells()),'Real gateway scan')
 set_range('invalid range');click('Scan')
 from PIL import ImageGrab
 ImageGrab.grab().save(root/'invalid-range.png')
 subprocess.run(['xdotool','key','Return'],check=True);time.sleep(.3);wait(ready);check(True,'Invalid range dialog and recovery')
 set_range(subnet);click('Scan');time.sleep(.3)
 wid=subprocess.check_output(['xdotool','search','--name','^Look@LAN-ng']).split()[-1].decode();subprocess.run(['xdotool','windowfocus',wid,'key','F5'],check=True);time.sleep(.3)
 check(not any(a.getRoleName()=='alert' for a in nodes()),'F5 during active scan does not show error')
 click('Stop');wait(ready);check(True,'Stop returns to idle')
 click('Profile');fill_dialog('Profile',['GUI profile',own+';'+gateway,'22','100','100','8','5','Profile note']);check(inputs()[0].queryText().getText(0,-1)=='','Profile editor applies multiple ranges')
 click('Scan');wait(ready);check(len(cells())==2,'Scan uses both profile ranges')
 check(all(any(a.name==name for a in nodes()) for name in ['General','Ports','Ping','Trace','History','Log']),'Six detail tabs present');selected_ip(own);click('Ping');wait(ready);check(True,'GUI Ping completes without blocking')
 click('Trace');wait(ready);check(True,'GUI Trace completes without blocking')
 log_tab=find('page tab','Log');rect=log_tab.queryComponent().getExtents(pyatspi.DESKTOP_COORDS);subprocess.run(['xdotool','mousemove',str(rect.x+rect.width//2),str(rect.y+rect.height//2),'click','1'],check=True);time.sleep(.2)
 tables=[a for a in nodes() if a.getRoleName()=='table'];check(any(a.queryTable().nColumns==4 and a.queryTable().nRows>0 for a in tables),'Native Log table contains session events')
 rect=find('page tab','General').queryComponent().getExtents(pyatspi.DESKTOP_COORDS);subprocess.run(['xdotool','mousemove',str(rect.x+rect.width//2),str(rect.y+rect.height//2),'click','1'],check=True);time.sleep(.2)
 selected_ip(own);click('Notes');fill_dialog('Notes',['GUI device note']);selected_ip(gateway);selected_ip(own)
 texts=[a.queryText().getText(0,-1) for a in nodes() if a.getRoleName()=='text' and not a.getState().contains(pyatspi.STATE_EDITABLE)]
 check(any('GUI device note' in s for s in texts),'Device notes displayed in details')
 for title in ['Statistics','History']:
  click(title);wid=dialog_window(title);subprocess.run(['xdotool','windowfocus',wid,'key','Escape'],check=True);time.sleep(.3);wait(ready);check(True,title+' dialog opens and closes')
 right_click(own,'Copy IP')
 import gi
 gi.require_version('Gtk','3.0');gi.require_version('Gdk','3.0')
 from gi.repository import Gtk,Gdk
 Gtk.init([]);check(Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD).wait_for_text()==own,'Context Copy IP clipboard')
 right_click(own,'Copy hostname');check(bool(Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD).wait_for_text()),'Context Copy hostname clipboard')
 click('Monitoring');wait(lambda:find('label','Monitoring | 2 / 2 | Hosts: 2'),15);click('Stop');wait(ready);check(True,'GUI monitoring starts and Stop disables schedule')
 click('Single host');fill_dialog('Single host',['localhost']);wait(ready);check(any(row[1]=='127.0.0.1' for row in cells()),'Single hostname resolves and scans');check(inputs()[0].queryText().getText(0,-1)=='','Single host preserves profile ranges')
 imported=root/'missing-interface.lanprofile';data=(root/'saved.lanprofile').read_text(encoding='utf-8');import re;data=re.sub(r'(?m)^interface_id=.*$', 'interface_id=missing-looklan-test',data);imported.write_text(data,encoding='utf-8');click('Open profile');file_dialog(imported);wid=dialog_window('Profile interface unavailable');subprocess.run(['xdotool','windowfocus',wid],check=True);click('Use selected interface');wait(ready);check(inputs()[0].queryText().getText(0,-1)==own+'-'+own,'Imported profile requires explicit interface remapping')
 set_range(subnet);click('Scan');time.sleep(.2);t=time.monotonic();close();check(p.wait(timeout=5)==0,'Close during scan exits cleanly');check(time.monotonic()-t<3,'Close during scan bounded under 3s');p=None
 p=start();choose_language('Русский');wait(lambda:find('button','Сканировать'));check(True,'Immediate Russian UI switch');close();check(p.wait(timeout=5)==0,'Idle close');p=None;time.sleep(.3)
 p=start(caption='Сканировать');check(True,'Saved Russian overrides English system locale');choose_language('System / Система');wait(lambda:find('button','Scan'));close();p.wait(timeout=5);p=None;time.sleep(.3)
 p=start('ru_RU.UTF-8','Сканировать');check(True,'Russian system locale selects Russian');choose_language('English');wait(lambda:find('button','Scan'));close();p.wait(timeout=5);p=None;time.sleep(.3)
 p=start('ru_RU.UTF-8','Scan');check(True,'Saved English overrides Russian system locale');choose_language('System / Система');close();p.wait(timeout=5);p=None
except Exception:
 from PIL import ImageGrab
 ImageGrab.grab().save(root/'failure.png')
 print(subprocess.check_output(['xdotool','search','--name','^Look@LAN-ng'],text=True),flush=True)
 raise
finally:
 if p and p.poll() is None:p.terminate();p.wait(timeout=5)
 if wm:wm.terminate();wm.wait(timeout=5)
 x.terminate();x.wait(timeout=5)
 (root/'gui-results.json').write_text(json.dumps(passed,ensure_ascii=False,indent=2))

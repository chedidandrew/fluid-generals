"""Build graphical permanent-install player ZIPs (Python 3.9+; Windows)."""
from pathlib import Path
import argparse,hashlib,json,shutil,zipfile,tempfile,os,subprocess
ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path,required=True);ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--output',type=Path);ap.add_argument('--version',default='1.0.2');ap.add_argument('--bootstrap',type=Path);args=ap.parse_args()
repo=Path(__file__).resolve().parents[1];out=(args.output or repo/'dist').resolve();out.mkdir(parents=True,exist_ok=True)
assert args.runtime.is_file(),'Supply the Microsoft-signed vc_redist.x86.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,t):p.write_text(t,encoding='utf-8-sig' if p.suffix=='.ps1' else 'utf8',newline='\r\n')
excluded={'GeneralsGameCode/Core/Tools/wolSetup/WOLAPI/wolapi.dll','GeneralsGameCode/Core/Tools/wolSetup/WOLAPI/woldbg.dll','GeneralsGameCode/Generals/Run/BrowserEngine.dll','GeneralsGameCode/GeneralsMD/Run/BrowserEngine.dll','ThirdParty/stb-src/tests/oversample/oversample.exe'}
with tempfile.TemporaryDirectory(dir=out,prefix='package-') as temp:
 work=Path(temp);bootstrap=args.bootstrap or work/'bootstrap'
 if args.bootstrap is None:
  bootstrap.mkdir();csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework/v4.0.30319/csc.exe'
  for name,extra in [('FluidSetup.exe',[]),('FluidLauncher.exe',['/define:LAUNCHER'])]:
   subprocess.run([str(csc),'/nologo','/target:winexe','/platform:anycpu','/optimize+','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll','/out:'+str(bootstrap/name),*extra,str(repo/'tools/FluidBootstrap.cs')],check=True)
 source=work/'EngineSource.zip'
 with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for tree,label in [(repo/'engine','GeneralsGameCode'),(repo/'ThirdParty','ThirdParty')]:
   for p in sorted(tree.rglob('*')):
    name=label+'/'+p.relative_to(tree).as_posix()
    if p.is_file() and '.git' not in p.relative_to(tree).parts and name not in excluded:z.write(p,name)
  for name in ['tools/FluidBootstrap.cs','tools/package_release.py','installer/Setup.ps1','installer/payload/Launch-Fluid.ps1','installer/payload/Ensure-AdaptiveCameraInput.ps1','installer/payload/Ensure-OwnedMilesAudio.ps1','BUILD.txt','CREDITS.txt']:
   z.write(repo/name,'FluidInstallerSource/'+name)
 for game in ['Both','Generals','ZeroHour']:
  name={'Both':'Fluid-Generals-and-Zero-Hour','Generals':'Fluid-Generals','ZeroHour':'Fluid-Generals-Zero-Hour'}[game];target=work/name;target.mkdir();payload=target/'payload';payload.mkdir();(target/'runtime').mkdir()
  shutil.copy2(repo/'installer/Setup.ps1',target/'Setup.ps1');shutil.copy2(bootstrap/'FluidSetup.exe',target/'FluidSetup.exe');shutil.copy2(bootstrap/'FluidLauncher.exe',payload/'FluidLauncher.exe')
  write(target/'setup-config.json',json.dumps({'game':game,'version':args.version}))
  for p in (repo/'installer/payload').glob('*.ps1'):shutil.copy2(p,payload/p.name)
  shutil.copytree(repo/'licenses',payload/'licenses');shutil.copy2(source,payload/'EngineSource.zip');shutil.copy2(args.runtime,target/'runtime/vc_redist.x86.exe')
  for which,var,exe in [('Generals','Generals','generalsvsim.exe'),('ZeroHour','GeneralsMD','generalszhsim.exe')]:
   if game not in ['Both',which]:continue
   d=payload/f'bin/{which}';d.mkdir(parents=True);shutil.copy2(args.build/f'{var}/Release/{exe}',d/exe)
  write(target/'README.txt',(repo/f'player-docs/{game}-README.txt').read_text().replace('v1.0.2','v'+args.version))
  manifest={p.relative_to(target).as_posix():{'size':p.stat().st_size,'sha256':sha(p)} for p in sorted(target.rglob('*')) if p.is_file()}
  write(target/'transfer-manifest.json',json.dumps(manifest,indent=2)+'\n')
  dest=out/f'{name}-v{args.version}.zip';assert not dest.exists(),'Do not overwrite an existing release ZIP'
  with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
   for p in sorted(target.rglob('*')):
    if p.is_file():z.write(p,name+'/'+p.relative_to(target).as_posix())
  print(dest.name+' '+sha(dest))
 dest=out/f'Fluid-EngineSource-v{args.version}.zip';assert not dest.exists();shutil.copy2(source,dest)

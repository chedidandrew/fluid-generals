"""Build player ZIPs from a compiled Fluid checkout (Python 3.9+)."""
from pathlib import Path
import argparse,hashlib,json,shutil,zipfile,tempfile
ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path,required=True);ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--output',type=Path);ap.add_argument('--version',default='1.0.0');args=ap.parse_args()
repo=Path(__file__).resolve().parents[1];out=(args.output or repo/'dist').resolve();out.mkdir(parents=True,exist_ok=True)
assert args.runtime.is_file(),'Supply the Microsoft-signed vc_redist.x86.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,t):p.write_text(t,encoding='utf-8-sig' if p.suffix=='.ps1' else 'utf8',newline='\r\n')
names={'Generals':'Command & Conquer: Fluid Generals','ZeroHour':'Command & Conquer: Fluid Generals Zero Hour'}
excluded={'GeneralsGameCode/Core/Tools/wolSetup/WOLAPI/wolapi.dll','GeneralsGameCode/Core/Tools/wolSetup/WOLAPI/woldbg.dll','GeneralsGameCode/Generals/Run/BrowserEngine.dll','GeneralsGameCode/GeneralsMD/Run/BrowserEngine.dll','ThirdParty/stb-src/tests/oversample/oversample.exe'}
with tempfile.TemporaryDirectory(dir=out,prefix='package-') as temp:
 stage=Path(temp)/'Fluid-Generals-and-Zero-Hour';shutil.copytree(repo/'installer',stage)
 payload=stage/'payload';(stage/'runtime').mkdir();shutil.copy2(args.runtime,stage/'runtime/vc_redist.x86.exe');shutil.copytree(repo/'licenses',payload/'licenses')
 for game,var,exe in [('Generals','Generals','generalsvsim.exe'),('ZeroHour','GeneralsMD','generalszhsim.exe')]:
  d=payload/f'bin/{game}';d.mkdir(parents=True);shutil.copy2(args.build/f'{var}/Release/{exe}',d/exe)
 with zipfile.ZipFile(payload/'EngineSource.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for tree,label in [(repo/'engine','GeneralsGameCode'),(repo/'ThirdParty','ThirdParty')]:
   for p in sorted(tree.rglob('*')):
    name=label+'/'+p.relative_to(tree).as_posix()
    if p.is_file() and '.git' not in p.relative_to(tree).parts and name not in excluded:z.write(p,name)
 for game in ['Both','Generals','ZeroHour']:
  name='Fluid-Generals-and-Zero-Hour' if game=='Both' else ('Fluid-Generals' if game=='Generals' else 'Fluid-Generals-Zero-Hour')
  target=Path(temp)/('output-'+game);shutil.copytree(stage,target)
  if game!='Both':
   other='Generals' if game=='ZeroHour' else 'ZeroHour'
   # All deletion targets are fixed descendants of this disposable stage.
   shutil.rmtree(target/f'payload/bin/{other}');(target/f'payload/Launch-{other}.cmd').unlink()
   p=target/'Setup.ps1';t=p.read_text(encoding='utf-8-sig');t=t.replace("[ValidateSet('','Both','Generals','ZeroHour')][string]$Game=''",f"[ValidateSet('{game}')][string]$Game='{game}'");write(p,t)
   p=target/'INSTALL.cmd';write(p,p.read_text().replace('Setup.ps1"','Setup.ps1" -Game '+game))
  write(target/'README.txt',(repo/f'player-docs/{game}-README.txt').read_text().replace('v1.0.0','v'+args.version))
  manifest={p.relative_to(target).as_posix():{'size':p.stat().st_size,'sha256':sha(p)} for p in sorted(target.rglob('*')) if p.is_file()}
  write(target/'transfer-manifest.json',json.dumps(manifest,indent=2)+'\n')
  dest=out/f'{name}-v{args.version}.zip';assert not dest.exists(),'Do not overwrite an existing release ZIP'
  with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
   for p in sorted(target.rglob('*')):
    if p.is_file():z.write(p,name+'/'+p.relative_to(target).as_posix())
  print(dest.name+' '+sha(dest))

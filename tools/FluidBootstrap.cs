// Fluid graphical setup and hidden launcher. 0BSD; see HELPERS-LICENSE.txt.
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Drawing;
using System.Windows.Forms;
using System.Web.Script.Serialization;

static class Bootstrap {
 internal static readonly string Root=AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
 internal static JavaScriptSerializer Json=new JavaScriptSerializer();
 internal static Dictionary<string,object> Map(object value){return value as Dictionary<string,object> ?? new Dictionary<string,object>();}
 internal static string Value(Dictionary<string,object> map,string key){return map.ContainsKey(key) ? Convert.ToString(map[key]) : "";}
 internal static string Quote(string value){
  StringBuilder b=new StringBuilder("\"");int slashes=0;
  foreach(char c in value){if(c=='\\'){slashes++;continue;}if(c=='\"'){b.Append('\\',slashes*2+1);b.Append(c);}else{b.Append('\\',slashes);b.Append(c);}slashes=0;}
  b.Append('\\',slashes*2);b.Append('"');return b.ToString();
 }
 internal sealed class Result {public int exit;public string stdout;public string stderr;public bool hidden;}
 internal static Result Run(string script,List<string> args,Action<string> progress){
  string windows=Environment.GetFolderPath(Environment.SpecialFolder.Windows);
  string ps=Path.Combine(windows,"Sysnative\\WindowsPowerShell\\v1.0\\powershell.exe");
  if(!File.Exists(ps))ps=Path.Combine(windows,"System32\\WindowsPowerShell\\v1.0\\powershell.exe");
  List<string> all=new List<string>(new string[]{"-NoProfile","-NonInteractive","-STA","-ExecutionPolicy","Bypass","-File",script});all.AddRange(args);
  StringBuilder command=new StringBuilder();foreach(string a in all){if(command.Length>0)command.Append(' ');command.Append(Quote(a));}
  ProcessStartInfo info=new ProcessStartInfo(ps,command.ToString());info.WorkingDirectory=Root;info.UseShellExecute=false;info.CreateNoWindow=true;info.WindowStyle=ProcessWindowStyle.Hidden;info.RedirectStandardOutput=true;info.RedirectStandardError=true;info.StandardOutputEncoding=Encoding.UTF8;info.StandardErrorEncoding=Encoding.UTF8;
  // PowerShell 7 can leave its module paths in the parent environment. Use the
  // Windows PowerShell modules for this Windows 10/11 backend, without profiles.
  info.EnvironmentVariables["PSModulePath"]=Path.Combine(windows,"System32\\WindowsPowerShell\\v1.0\\Modules")+";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"WindowsPowerShell\\Modules")+";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"WindowsPowerShell\\Modules");
  StringBuilder output=new StringBuilder(),error=new StringBuilder();
  using(Process p=new Process()){
   p.StartInfo=info;
   p.OutputDataReceived+=delegate(object sender,DataReceivedEventArgs e){if(e.Data!=null){lock(output){output.AppendLine(e.Data);}if(progress!=null)progress(e.Data);}};
   p.ErrorDataReceived+=delegate(object sender,DataReceivedEventArgs e){if(e.Data!=null)lock(error){error.AppendLine(e.Data);}};
   p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();p.WaitForExit();
   return new Result{exit=p.ExitCode,stdout=output.ToString(),stderr=error.ToString(),hidden=info.CreateNoWindow && info.WindowStyle==ProcessWindowStyle.Hidden && !info.UseShellExecute};
  }
 }
 internal static List<string> RequestArgs(Dictionary<string,object> request){
  List<string> args=new List<string>();
  foreach(string key in new string[]{"Game","GeneralsPath","ZeroHourPath","InstallBase","Action","MigrateGeneralsFrom","MigrateZeroHourFrom","RegistryPath"}){string v=Value(request,key);if(v.Length>0){args.Add("-"+key);args.Add(v);}}
  foreach(string key in new string[]{"NoShortcuts","SkipPrerequisite","Discover","RemoveSaves"})if(request.ContainsKey(key) && Convert.ToBoolean(request[key]))args.Add("-"+key);
  return args;
 }
 [STAThread] static void Main(string[] args){
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  try{
#if LAUNCHER
   Dictionary<string,object> state=Map(Json.DeserializeObject(File.ReadAllText(Path.Combine(Root,"experimental-install-state.json"))));
   string game=Value(state,"game");if(game!="Generals" && game!="ZeroHour")throw new Exception("Fluid installation is incomplete. Run FluidSetup.exe to repair it.");
   List<string> options=new List<string>(new string[]{"-Game",game});
   bool inspect=args.Length==2 && args[0]=="--inspect";
   if(inspect)options.Add("-Inspect");else options.AddRange(args);
   Result result=Run(Path.Combine(Root,"Launch-Fluid.ps1"),options,null);
   if(inspect){File.WriteAllText(args[1],Json.Serialize(new {exit=result.exit,stdout=result.stdout,stderr=result.stderr,hidden=result.hidden}));return;}
   if(result.exit!=0)throw new Exception(result.stderr.Length>0?result.stderr:"Launch failed. Run FluidSetup.exe to repair it.");
#else
   if(args.Length==3 && args[0]=="--verify"){
    Dictionary<string,object> request=Map(Json.DeserializeObject(File.ReadAllText(args[1])));
    Result result=Run(Path.Combine(Root,"Setup.ps1"),RequestArgs(request),null);
    File.WriteAllText(args[2],Json.Serialize(new {exit=result.exit,stdout=result.stdout,stderr=result.stderr,hidden=result.hidden}));return;
   }
   if(args.Length==2 && args[0]=="--discover"){
    Result result=Run(Path.Combine(Root,"Setup.ps1"),new List<string>(new string[]{"-Discover"}),null);
    File.WriteAllText(args[1],Json.Serialize(new {exit=result.exit,stdout=result.stdout,stderr=result.stderr,hidden=result.hidden}));return;
   }
   SetupForm form=new SetupForm();
   if(args.Length==2 && args[0]=="--render-preview"){
    form.CreateControl();using(Bitmap bmp=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bmp,new Rectangle(0,0,bmp.Width,bmp.Height));bmp.Save(args[1]);}form.Dispose();return;
   }
   Application.Run(form);
#endif
  }catch(Exception e){MessageBox.Show(e.Message,"Fluid",MessageBoxButtons.OK,MessageBoxIcon.Error);}
 }
}

#if !LAUNCHER
sealed class SetupForm:Form {
 readonly ComboBox game=new ComboBox();readonly TextBox install=new TextBox(),generals=new TextBox(),zeroHour=new TextBox();
 readonly Label installed=new Label();readonly TextBox status=new TextBox();readonly Button update=new Button(),repair=new Button(),reset=new Button(),uninstall=new Button();readonly CheckBox keepSaves=new CheckBox();
 readonly ProgressBar progress=new ProgressBar();Dictionary<string,object> found=new Dictionary<string,object>();bool busy,maintenance;string edition="Both";
 internal SetupForm(){
  Text="Fluid Setup 1.0.2";ClientSize=new Size(720,510);Font=new Font("Segoe UI",9);AutoScaleMode=AutoScaleMode.Font;FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;
  string config=Path.Combine(Bootstrap.Root,"setup-config.json");if(!File.Exists(config))config=Path.Combine(Bootstrap.Root,"maintenance-config.json");if(File.Exists(config)){Dictionary<string,object> settings=Bootstrap.Map(Bootstrap.Json.DeserializeObject(File.ReadAllText(config)));edition=Bootstrap.Value(settings,"game");maintenance=settings.ContainsKey("maintenance") && Convert.ToBoolean(settings["maintenance"]);}
  AddLabel("Fluid Generals / Fluid Generals Zero Hour",16,12,650,24).Font=new Font(Font,FontStyle.Bold);
  AddLabel(maintenance?"Uninstall selected Fluid games. Original games remain unchanged.":"Install permanently. Downloaded setup files can be deleted afterward.",16,40,680,22);
  AddLabel("Game",16,78,140,22);game.SetBounds(165,75,260,26);game.DropDownStyle=ComboBoxStyle.DropDownList;
  if(edition=="Both"){game.Items.AddRange(new object[]{"Both games","Generals","Zero Hour"});game.SelectedIndex=0;}else{game.Items.Add(edition=="Generals"?"Generals":"Zero Hour");game.SelectedIndex=0;game.Enabled=false;}
  Controls.Add(game);
  install.Text=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs\\Fluid");
  if(maintenance){Text="Fluid Uninstall";install.Text=Bootstrap.Root;install.ReadOnly=true;}
  AddFolder("Install location",install,112);AddFolder("Generals game folder",generals,151);AddFolder("Zero Hour game folder",zeroHour,190);
  installed.SetBounds(16,230,682,36);installed.Text="Detecting games and existing Fluid installations...";Controls.Add(installed);
  AddLabel("Install/Update and Repair keep saves/settings. Reset makes a backup first.",16,273,685,23);
  keepSaves.Text="Keep saves/settings when uninstalling";keepSaves.Checked=true;keepSaves.SetBounds(16,299,450,24);Controls.Add(keepSaves);
  update.Text="Install / Update";repair.Text="Repair";reset.Text="Reset";uninstall.Text="Uninstall";Button[] buttons={update,repair,reset,uninstall};
  for(int i=0;i<buttons.Length;i++){buttons[i].SetBounds(35+i*160,337,145,32);Controls.Add(buttons[i]);}
  update.Click+=delegate{Start("Install");};repair.Click+=delegate{Start("Repair");};reset.Click+=delegate{if(MessageBox.Show(this,"Reset Fluid saves/settings to defaults? The previous files will be kept in backups.","Reset Fluid",MessageBoxButtons.YesNo,MessageBoxIcon.Question,MessageBoxDefaultButton.Button2)==DialogResult.Yes)Start("Reset");};
  uninstall.Click+=delegate{string message="Uninstall selected Fluid games? "+(keepSaves.Checked?"Saves/settings will be kept.":"Saves/settings and backups will also be removed.");if(MessageBox.Show(this,message,"Uninstall Fluid",MessageBoxButtons.YesNo,MessageBoxIcon.Warning,MessageBoxDefaultButton.Button2)==DialogResult.Yes)Start("Uninstall");};
  progress.SetBounds(16,385,682,10);Controls.Add(progress);status.SetBounds(16,407,682,78);status.Multiline=true;status.ReadOnly=true;status.ScrollBars=ScrollBars.Vertical;Controls.Add(status);
  game.SelectedIndexChanged+=delegate{RefreshSelection();};RefreshSelection();
  FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy){e.Cancel=true;MessageBox.Show(this,"Wait for setup to finish.","Fluid Setup");}};
  Shown+=delegate{Detect();};
 }
 Label AddLabel(string text,int x,int y,int w,int h){Label label=new Label();label.Text=text;label.SetBounds(x,y,w,h);Controls.Add(label);return label;}
 void AddFolder(string label,TextBox field,int y){
  AddLabel(label,16,y+3,145,25);field.SetBounds(165,y,440,27);Controls.Add(field);Button browse=new Button();browse.Text="Browse...";browse.SetBounds(615,y-1,84,28);Controls.Add(browse);
  browse.Click+=delegate{using(FolderBrowserDialog dialog=new FolderBrowserDialog()){dialog.Description=label;dialog.ShowNewFolderButton=field==install;string p=field.Text;if(Directory.Exists(p))dialog.SelectedPath=p;if(dialog.ShowDialog(this)==DialogResult.OK)field.Text=dialog.SelectedPath;}};
 }
 string Which(){if(edition!="Both")return edition;return game.SelectedIndex==0?"Both":game.SelectedIndex==1?"Generals":"ZeroHour";}
 void RefreshSelection(){string selected=Which();generals.Enabled=!maintenance && selected!="ZeroHour";zeroHour.Enabled=!maintenance && selected!="Generals";if(found.Count>0){List<string> labels=new List<string>();foreach(string key in new string[]{"Generals","ZeroHour"})if(selected=="Both" || selected==key){Dictionary<string,object> item=found.ContainsKey(key)?Bootstrap.Map(found[key]):new Dictionary<string,object>();string text=item.Count==0?"not installed":Bootstrap.Value(item,"status");if(text=="Installed")text="Fluid "+Bootstrap.Value(item,"version");labels.Add(key+": "+text);}installed.Text=String.Join("   |   ",labels.ToArray());}}
 void Log(string line){if(IsDisposed)return;BeginInvoke((Action)delegate{status.AppendText(line+Environment.NewLine);});}
 void SetBusy(bool value){busy=value;update.Enabled=!value && !maintenance;repair.Enabled=!value && !maintenance;reset.Enabled=!value && !maintenance;uninstall.Enabled=!value;game.Enabled=!value && edition=="Both";install.Enabled=!value && !maintenance;generals.Enabled=!value && !maintenance && Which()!="ZeroHour";zeroHour.Enabled=!value && !maintenance && Which()!="Generals";keepSaves.Enabled=!value;foreach(Control c in Controls)if(c is Button && c.Text=="Browse...")c.Enabled=!value && !maintenance;progress.Style=value?ProgressBarStyle.Marquee:ProgressBarStyle.Blocks;}
 void Detect(){
  SetBusy(true);ThreadPool.QueueUserWorkItem(delegate{
   try{
    List<string> detectArgs=new List<string>(new string[]{"-Discover"});if(maintenance){detectArgs.Add("-InstallBase");detectArgs.Add(Bootstrap.Root);}
    Bootstrap.Result result=Bootstrap.Run(Path.Combine(Bootstrap.Root,"Setup.ps1"),detectArgs,null);
    if(result.exit!=0)throw new Exception(result.stderr);
    Dictionary<string,object> data=Bootstrap.Map(Bootstrap.Json.DeserializeObject(result.stdout));
    BeginInvoke((Action)delegate{
     Dictionary<string,object> games=Bootstrap.Map(data["games"]);found=Bootstrap.Map(data["installed"]);generals.Text=Bootstrap.Value(games,"Generals");zeroHour.Text=Bootstrap.Value(games,"ZeroHour");
     foreach(string key in new string[]{"Generals","ZeroHour"})if(!maintenance && found.ContainsKey(key)){Dictionary<string,object> item=Bootstrap.Map(found[key]);if(item.ContainsKey("permanent") && Convert.ToBoolean(item["permanent"]) && (Which()=="Both" || Which()==key)){install.Text=Path.GetDirectoryName(Bootstrap.Value(item,"root"));break;}}
     RefreshSelection();SetBusy(false);status.Text=maintenance?"Choose the game(s) to uninstall.":"Choose the install location, then Install / Update.";
    });
   }catch(Exception e){BeginInvoke((Action)delegate{SetBusy(false);status.Text="Select the game folders manually. "+e.Message;});}
  });
 }
 void Start(string action){
  string selected=Which();
  if(String.IsNullOrWhiteSpace(install.Text) || (action!="Uninstall" && ((selected!="ZeroHour" && String.IsNullOrWhiteSpace(generals.Text)) || (selected!="Generals" && String.IsNullOrWhiteSpace(zeroHour.Text))))){MessageBox.Show(this,"Choose the install location and the selected game's folder(s).","Fluid Setup");return;}
  Dictionary<string,object> request=new Dictionary<string,object>();request["Game"]=selected;request["InstallBase"]=install.Text;request["GeneralsPath"]=generals.Text;request["ZeroHourPath"]=zeroHour.Text;request["Action"]=action;
  if(action=="Uninstall")request["RemoveSaves"]=!keepSaves.Checked;
  foreach(string key in new string[]{"Generals","ZeroHour"})if(found.ContainsKey(key))request["Migrate"+key+"From"]=Bootstrap.Value(Bootstrap.Map(found[key]),"root");
  status.Clear();SetBusy(true);ThreadPool.QueueUserWorkItem(delegate{
   try{
    Bootstrap.Result result=Bootstrap.Run(Path.Combine(Bootstrap.Root,"Setup.ps1"),Bootstrap.RequestArgs(request),Log);
    BeginInvoke((Action)delegate{SetBusy(false);if(result.exit!=0){status.AppendText(result.stderr);MessageBox.Show(this,result.stderr,"Setup could not finish",MessageBoxButtons.OK,MessageBoxIcon.Error);}else{MessageBox.Show(this,action=="Uninstall"?"Uninstalled. Original games unchanged.":"Ready. Use the Fluid desktop shortcuts. You can delete the downloaded setup files.","Fluid",MessageBoxButtons.OK,MessageBoxIcon.Information);Detect();}});
   }catch(Exception e){BeginInvoke((Action)delegate{SetBusy(false);MessageBox.Show(this,e.Message,"Setup could not finish",MessageBoxButtons.OK,MessageBoxIcon.Error);});}
  });
 }
}
#endif

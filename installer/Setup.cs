// SPDX-License-Identifier: GPL-2.0-or-later
// .NET Framework 4.8, available on the supported Windows 10/11 systems.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32;

static class Setup {
    static string L(string key,params object[] args){return InstallerText.Get(key,args);}
    const string Product = "NEXUS Reactive Echo";
    const string Plugin = "nexus-reactive-echo";
    const string RegistryPath = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\NEXUS Reactive Echo";
    static string Version { get { return Assembly.GetExecutingAssembly().GetName().Version.ToString(3); } }
    static string OwnExe { get { return Assembly.GetExecutingAssembly().Location; } }
    static bool HasPayload { get { return Assembly.GetExecutingAssembly().GetManifestResourceInfo("payload.zip") != null; } }
    static string DefaultObs { get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "obs-studio"); } }
    sealed class NexusButton : Button {
        public bool Primary;
        public NexusButton() { SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer,true); Cursor=Cursors.Hand; }
        protected override void OnPaint(PaintEventArgs e) {
            e.Graphics.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
            var rect=new Rectangle(1,1,Width-3,Height-3);
            using(var path=new System.Drawing.Drawing2D.GraphicsPath()) {
                const int diameter=16;
                path.AddArc(rect.Left,rect.Top,diameter,diameter,180,90);
                path.AddArc(rect.Right-diameter,rect.Top,diameter,diameter,270,90);
                path.AddArc(rect.Right-diameter,rect.Bottom-diameter,diameter,diameter,0,90);
                path.AddArc(rect.Left,rect.Bottom-diameter,diameter,diameter,90,90);path.CloseFigure();
                var first=Primary?Color.FromArgb(151,74,255):Color.FromArgb(31,37,49);
                var last=Primary?Color.FromArgb(96,55,244):Color.FromArgb(31,37,49);
                if(!Enabled)first=last=Color.FromArgb(48,49,65);
                using(var brush=new System.Drawing.Drawing2D.LinearGradientBrush(rect,first,last,0f))e.Graphics.FillPath(brush,path);
                using(var pen=new Pen(Primary?Color.FromArgb(170,113,255):Color.FromArgb(100,113,140)))e.Graphics.DrawPath(pen,path);
            }
            TextRenderer.DrawText(e.Graphics,Text,Font,rect,Enabled?Color.White:Color.FromArgb(130,135,150),TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);
            if(Focused)ControlPaint.DrawFocusRectangle(e.Graphics,new Rectangle(6,6,Width-12,Height-12),Color.White,BackColor);
        }
    }
    sealed class Target {
        public string Obs, Root, Metadata, Manifest; public bool Portable;
        public Target(string obs, bool portable) {
            Obs = Path.GetFullPath(obs).TrimEnd(Path.DirectorySeparatorChar); Portable = portable;
            string exe = Path.Combine(Obs, @"bin\64bit\obs64.exe");
            if (!File.Exists(exe)) throw new IOException(L("Setup.SelectObs"));
            CheckLinks(exe);
            using (var input = new BinaryReader(File.OpenRead(exe))) {
                if (input.ReadUInt16() != 0x5a4d) throw new IOException(L("Setup.InvalidExe"));
                input.BaseStream.Position = 0x3c; int offset = input.ReadInt32();
                input.BaseStream.Position = offset;
                if (input.ReadUInt32() != 0x4550 || input.ReadUInt16() != 0x8664) throw new IOException(L("Setup.Needs64"));
            }
            var match = Regex.Match(FileVersionInfo.GetVersionInfo(exe).ProductVersion ?? "", @"^\d+\.\d+\.\d+");
            if (!match.Success || new Version(match.Value) < new Version(32, 2, 2)) throw new IOException(L("Setup.NeedsVersion"));
            if (portable && !File.Exists(Path.Combine(Obs, "portable_mode.txt"))) throw new IOException(L("Setup.NotPortable"));
            if (!portable && File.Exists(Path.Combine(Obs, "portable_mode.txt"))) throw new IOException(L("Setup.SelectPortable"));
            Root = portable ? Obs : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), @"obs-studio\plugins", Plugin);
            Metadata = portable ? Join(Root, "data/obs-plugins/" + Plugin + "/setup") : Join(Root, "setup");
            Manifest = Join(Metadata, "installed.tsv"); CheckLinks(Root); CheckLinks(Metadata);
        }
        public string Map(string name) {
            if (name == "bin/64bit/" + Plugin + ".dll") return Portable ? "obs-plugins/64bit/" + Plugin + ".dll" : name;
            if (name == "data/README.txt" || name == "data/COPYING.txt" || IsLocale(name)) return Portable ? "data/obs-plugins/" + Plugin + "/" + name.Substring(5) : name;
            throw new IOException(L("Setup.UnexpectedFile",name));
        }
        static bool IsLocale(string name) {
            return name=="data/locale/es-ES.ini" || name=="data/locale/en-US.ini" || name=="data/locale/de-DE.ini" || name=="data/locale/fr-FR.ini" || name=="data/locale/it-IT.ini";
        }
        public bool Owns(string name) {
            foreach(string language in new[]{"es-ES","en-US","de-DE","fr-FR","it-IT"})if(name==Map("data/locale/"+language+".ini"))return true;
            foreach(string file in new[]{"bin/64bit/"+Plugin+".dll","data/README.txt","data/COPYING.txt"})
                if(name==Map(file))return true;
            return name==(Portable?"data/obs-plugins/"+Plugin+"/setup/uninstall.exe":"setup/uninstall.exe");
        }
    }

    static void CheckLinks(string path) {
        var current = Path.GetFullPath(path);
        while (!String.IsNullOrEmpty(current)) {
            if ((File.Exists(current) || Directory.Exists(current)) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                throw new IOException(L("Setup.LinkedPath"));
            current = Path.GetDirectoryName(current);
        }
    }
    static string Join(string root, string name) {
        if (String.IsNullOrEmpty(name) || name.IndexOf('\\') >= 0 || name.IndexOf(':') >= 0 || name.IndexOf('\t') >= 0 || name.IndexOf('\n') >= 0 || Path.IsPathRooted(name)) throw new IOException(L("Setup.InvalidPath"));
        foreach (string part in name.Split('/')) if (part == ".." || part == "." || part.Length == 0) throw new IOException(L("Setup.InvalidPath"));
        string full = Path.GetFullPath(Path.Combine(root, name.Replace('/', Path.DirectorySeparatorChar)));
        if (!full.StartsWith(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new IOException(L("Setup.OutsideTarget"));
        CheckLinks(full); return full;
    }
    static string Digest(string path) { using (var stream = File.OpenRead(path)) using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", ""); }
    static Dictionary<string, string> Parse(string text) {
        var files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (string line in text.Split('\n')) {
            if (String.IsNullOrWhiteSpace(line) || line.StartsWith("#")) continue;
            string[] fields = line.TrimEnd('\r').Split('\t');
            if (fields.Length != 2 || !Regex.IsMatch(fields[0], "^[A-Fa-f0-9]{64}$") || files.ContainsKey(fields[1])) throw new IOException(L("Setup.InvalidManifest"));
            files.Add(fields[1], fields[0].ToUpperInvariant());
        }
        if (files.Count == 0 || files.Count > 10000) throw new IOException(L("Setup.ManifestSize"));
        return files;
    }
    static string Serialize(Dictionary<string,string> files) {
        var result = new StringBuilder("# " + Product + " " + Version + "\n");
        foreach (var file in files) result.Append(file.Value).Append('\t').Append(file.Key).Append('\n');
        return result.ToString();
    }
    static Dictionary<string,string> Installed(Target target) {
        if (!File.Exists(target.Manifest)) return new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        var files = Parse(File.ReadAllText(target.Manifest));
        foreach (var item in files) { if (!target.Owns(item.Key)) throw new IOException(L("Setup.ForeignFiles")); Join(target.Root, item.Key); }
        return files;
    }
    static void EnsureClosed(Target target) {
        foreach (var process in Process.GetProcessesByName("obs64")) using (process) {
            string path;
            try { path = process.MainModule.FileName; } catch { throw new IOException(L("Setup.CheckClosed")); }
            if (!target.Portable || String.Equals(Path.GetFullPath(path), Path.Combine(target.Obs, @"bin\64bit\obs64.exe"), StringComparison.OrdinalIgnoreCase))
                throw new IOException(L("Setup.CloseObs",target.Portable?L("Setup.ThisPortable"):"OBS"));
        }
    }
    static void Install(Target target) {
        if (!HasPayload) throw new IOException(L("Setup.FullInstaller"));
        EnsureClosed(target); var owned = Installed(target);
        string oldDll = Join(target.Root, target.Map("bin/64bit/" + Plugin + ".dll"));
        if (File.Exists(oldDll)) {
            var installedVersion = FileVersionInfo.GetVersionInfo(oldDll).ProductVersion ?? "0.0.0";
            var match = Regex.Match(installedVersion, @"^\d+\.\d+\.\d+");
            if (match.Success && new Version(match.Value) > new Version(Version)) throw new IOException(L("Setup.NewerVersion"));
        }
        string stage = Path.Combine(Path.GetTempPath(), "nre-setup-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stage);
        var changes = new List<KeyValuePair<string,string>>(); bool keepStage = false;
        try {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip"))
            using (var archive = new ZipArchive(stream, ZipArchiveMode.Read)) {
                var entry = archive.GetEntry("payload.tsv"); if (entry == null) throw new IOException(L("Setup.Incomplete"));
                string text; using (var reader = new StreamReader(entry.Open())) text = reader.ReadToEnd();
                var payload = Parse(text); long total = 0;
                foreach (var item in payload) {
                    target.Map(item.Key); var source = archive.GetEntry(item.Key); if (source == null || source.Length > 512L * 1024 * 1024 || (total += source.Length) > 2L * 1024 * 1024 * 1024) throw new IOException(L("Setup.PackageSize"));
                    string extracted = Join(stage, "new/" + item.Key); Directory.CreateDirectory(Path.GetDirectoryName(extracted));
                    using (var from = source.Open()) using (var to = File.Create(extracted)) from.CopyTo(to);
                    if (Digest(extracted) != item.Value) throw new IOException(L("Setup.Damaged",item.Key));
                }
                // All bytes and paths have been verified before replacing the first file.
                EnsureClosed(target);
                foreach (var item in payload) { string relative = target.Map(item.Key); ReplaceFile(Join(stage, "new/" + item.Key), Join(target.Root, relative), stage, changes); owned[relative] = item.Value; }
                string uninstaller = Join(stage, "uninstall.exe");
                using (var source = Assembly.GetExecutingAssembly().GetManifestResourceStream("uninstall.exe")) {
                    if (source == null) throw new IOException(L("Setup.MissingUninstaller"));
                    using (var output = File.Create(uninstaller)) source.CopyTo(output);
                }
                string uninstallerName = target.Portable ? "data/obs-plugins/" + Plugin + "/setup/uninstall.exe" : "setup/uninstall.exe";
                ReplaceFile(uninstaller, Join(target.Root, uninstallerName), stage, changes); owned[uninstallerName] = Digest(uninstaller);
                string marker = Join(stage, "installed.tsv"); File.WriteAllText(marker, Serialize(owned), new UTF8Encoding(false));
                ReplaceFile(marker, target.Manifest, stage, changes);
            }
            if (!target.Portable) Register(target);
        } catch {
            for (int i = changes.Count - 1; i >= 0; --i) {
                var change = changes[i];
                try { if (File.Exists(change.Key)) File.Delete(change.Key); if (change.Value != null) File.Copy(change.Value, change.Key); }
                catch { keepStage = true; throw new IOException(L("Setup.RecoveryKeep",stage)); }
            }
            throw;
        } finally {
            // Only the fresh, locally generated staging directory is removed.
            if (!keepStage && Directory.Exists(stage) && stage.StartsWith(Path.Combine(Path.GetTempPath(), "nre-setup-"), StringComparison.OrdinalIgnoreCase)) Directory.Delete(stage, true);
        }
    }
    static void ReplaceFile(string source, string target, string stage, List<KeyValuePair<string,string>> changes) {
        CheckLinks(target); Directory.CreateDirectory(Path.GetDirectoryName(target));
        string backup = null; if (File.Exists(target)) { backup = Path.Combine(stage, "backup-" + changes.Count); File.Copy(target, backup); }
        string temporary = target + ".nre-new";
        if (File.Exists(temporary) || Directory.Exists(temporary)) throw new IOException(L("Setup.PendingInstall",temporary));
        try { File.Copy(source, temporary); if (File.Exists(target)) File.Replace(temporary, target, null); else File.Move(temporary, target); changes.Add(new KeyValuePair<string,string>(target, backup)); }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
    static void Register(Target target) {
        using (var key = Registry.LocalMachine.CreateSubKey(RegistryPath)) {
            key.SetValue("DisplayName", Product); key.SetValue("DisplayVersion", Version);
            key.SetValue("Publisher", "DjEmiMas"); key.SetValue("InstallLocation", target.Root);
            key.SetValue("DisplayIcon", "\"" + Join(target.Metadata, "uninstall.exe") + "\",0");
            key.SetValue("UninstallString", "\"" + Join(target.Metadata, "uninstall.exe") + "\" --gui-uninstall --standard --obs \"" + target.Obs + "\"");
            key.SetValue("NoModify", 1); key.SetValue("NoRepair", 1);
        }
    }
    static string Uninstall(Target target) {
        EnsureClosed(target); var files = Installed(target); if (files.Count == 0) return L("Setup.NoManagedInstall");
        var preserved = new List<string>();
        foreach (var item in files) {
            string path = Join(target.Root, item.Key);
            if (File.Exists(path) && Digest(path) != item.Value) {
                if (item.Key.EndsWith("/" + Plugin + ".dll", StringComparison.Ordinal)) throw new IOException(L("Setup.ModifiedPlugin"));
                preserved.Add(item.Key);
            }
        }
        string stage=Path.Combine(Path.GetTempPath(),"nre-setup-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stage);
        var removed=new List<KeyValuePair<string,string>>();bool keepStage=false;
        try {
            foreach(var item in files) {
                string path=Join(target.Root,item.Key);
                if(preserved.Contains(item.Key) || String.Equals(path,OwnExe,StringComparison.OrdinalIgnoreCase) || !File.Exists(path))continue;
                string backup=Path.Combine(stage,"removed-"+removed.Count);File.Copy(path,backup);File.Delete(path);removed.Add(new KeyValuePair<string,string>(path,backup));
            }
            string markerBackup=Path.Combine(stage,"manifest");File.Copy(target.Manifest,markerBackup);File.Delete(target.Manifest);removed.Add(new KeyValuePair<string,string>(target.Manifest,markerBackup));
            if(!target.Portable)Registry.LocalMachine.DeleteSubKeyTree(RegistryPath,false);
        } catch {
            for(int i=removed.Count-1;i>=0;--i)try{File.Copy(removed[i].Value,removed[i].Key,true);}catch{keepStage=true;throw new IOException(L("Setup.Recovery",stage));}
            throw;
        } finally { if(!keepStage && Directory.Exists(stage))Directory.Delete(stage,true); }
        string boundary=target.Portable?Join(target.Root,"data/obs-plugins/"+Plugin):target.Root;
        var directories=new List<string>();foreach(var item in files){string dir=Path.GetDirectoryName(Join(target.Root,item.Key));while(dir.StartsWith(boundary,StringComparison.OrdinalIgnoreCase)){if(!directories.Contains(dir))directories.Add(dir);if(dir==boundary)break;dir=Path.GetDirectoryName(dir);}}
        directories.Sort(delegate(string a,string b){return b.Length.CompareTo(a.Length);});
        foreach(string dir in directories)if(Directory.Exists(dir) && Directory.GetFileSystemEntries(dir).Length==0)Directory.Delete(dir);
        return L("Setup.RemovedProtected") + (preserved.Count>0?L("Setup.ModifiedPreserved",preserved.Count):"");
    }
    static int Operate(string[] args) {
        string action = null, obs = DefaultObs; bool portable = false;
        for (int i = 0; i < args.Length; ++i) {
            if (args[i] == "--install" || args[i] == "--uninstall" || args[i] == "--inspect") action = args[i];
            else if (args[i] == "--portable" && i + 1 < args.Length) { portable = true; obs = args[++i]; }
            else if (args[i] == "--standard") portable = false;
            else if (args[i] == "--obs" && i + 1 < args.Length) obs = args[++i];
            else throw new ArgumentException(L("Setup.UnknownArgument",args[i]));
        }
        if (action == null) throw new ArgumentException(L("Setup.MissingAction"));
        if (!portable && action != "--inspect" && !new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator)) throw new IOException(L("Setup.AdminRequired"));
        var target = new Target(obs, portable);
        if (action == "--install") Install(target); else if (action == "--uninstall") Uninstall(target); else Installed(target);
        return 0;
    }
    [STAThread] public static int Main(string[] args) {
        // Run the small uninstaller from a fresh temporary copy so its installed
        // executable can be removed without shell commands or a reboot.
        if (!HasPayload && Path.GetFileName(OwnExe).Equals("uninstall.exe", StringComparison.OrdinalIgnoreCase)) {
            if (args.Length == 0) {
                var candidate = new DirectoryInfo(Path.GetDirectoryName(OwnExe));
                for (int i = 0; i < 4 && candidate != null; ++i) candidate = candidate.Parent;
                if (candidate != null && File.Exists(Path.Combine(candidate.FullName, "portable_mode.txt")))
                    args = new[] { "--gui-uninstall", "--portable", candidate.FullName };
            }
            string temporary = Path.Combine(Path.GetTempPath(), "nre-uninstall-" + Guid.NewGuid().ToString("N") + ".exe");
            try { File.Copy(OwnExe, temporary); var arguments=new StringBuilder();foreach(string arg in args)arguments.Append('"').Append(arg.Replace("\"", "")).Append("\" "); Process.Start(new ProcessStartInfo(temporary, arguments.ToString()){UseShellExecute=true,WindowStyle=ProcessWindowStyle.Normal});return 0; }
            catch(Exception e){MessageBox.Show(e.Message,Product,MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
        }
        if (args.Length > 0 && args[0] != "--gui-uninstall" && args[0] != "--preview") {
            try { return Operate(args); } catch (Exception e) { Console.Error.WriteLine(e.Message); return 1; }
        }
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        var background = Color.FromArgb(25, 29, 38);
        var muted = Color.FromArgb(165, 174, 193);
        var form = new Form { Text = Product + " " + Version, ClientSize = new Size(960,570), FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false, StartPosition = FormStartPosition.CenterScreen, BackColor = background, ForeColor = Color.White, Font = new Font("Segoe UI",10), AutoScaleMode = AutoScaleMode.Dpi };
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("nexus-icon.ico")) {
            if(stream != null)using(var original=new Icon(stream))form.Icon=(Icon)original.Clone();
        }
        var branding = new PictureBox { Left=0,Top=0,Width=310,Height=570,SizeMode=PictureBoxSizeMode.Zoom,TabStop=false };
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("nexus-branding.png")) {
            if(stream != null) using(var original=Image.FromStream(stream)) branding.Image=new Bitmap(original);
        }
        form.FormClosed += delegate { if(branding.Image != null)branding.Image.Dispose(); if(form.Icon != null)form.Icon.Dispose(); };
        var header = new Label { Text=Product+"  "+Version,Left=346,Top=28,Width=576,Height=28,ForeColor=muted };
        var title = new Label { Text = L("Setup.Welcome",Product), Left=346,Top=78,Width=578,Height=96,Font=new Font("Segoe UI",24,FontStyle.Bold) };
        var help = new Label { Text=L("Setup.Help"),Left=348,Top=176,Width=574,Height=66,ForeColor=muted };
        var folderLabel = new Label { Text=L("Setup.Folder"),Left=348,Top=251,Width=570,Height=25,ForeColor=muted };
        var folder = new TextBox { Text=DefaultObs,Left=348,Top=285,Width=426,Height=32,BackColor=Color.FromArgb(33,38,49),ForeColor=Color.White,BorderStyle=BorderStyle.FixedSingle };
        var browse = new NexusButton { Text=L("Setup.Browse"),Left=788,Top=281,Width=134,Height=38 };
        var portable = new CheckBox { Text=L("Setup.Portable"),Left=348,Top=340,Width=574,Height=30,ForeColor=Color.White };
        var status = new Label { Text=L("Setup.ConnectionLater"),Left=348,Top=389,Width=574,Height=64,ForeColor=muted };
        var divider = new Panel { Left=348,Top=468,Width=574,Height=1,BackColor=Color.FromArgb(55,62,78) };
        var install = new NexusButton { Text=L("Setup.Install"),Left=690,Top=491,Width=232,Height=48,Enabled=HasPayload,Primary=true };
        var uninstall = new NexusButton { Text=L("Setup.Uninstall"),Left=348,Top=491,Width=155,Height=48 };
        var close = new NexusButton { Text=L("Setup.Close"),Left=517,Top=491,Width=155,Height=48 };
        form.AcceptButton=install;form.CancelButton=close;
        if (args.Length > 0) { for(int i=1;i<args.Length;++i) { if(args[i]=="--obs" && i+1<args.Length)folder.Text=args[++i]; else if(args[i]=="--portable" && i+1<args.Length){portable.Checked=true;folder.Text=args[++i];} } }
        browse.Click += delegate { using(var dialog=new FolderBrowserDialog { Description=L("Setup.FolderHelp"),SelectedPath=folder.Text }) if(dialog.ShowDialog(form)==DialogResult.OK)folder.Text=dialog.SelectedPath; };
        Action<bool> run = delegate(bool removing) {
            try {
                var target = new Target(folder.Text,portable.Checked); EnsureClosed(target);
                if (removing && MessageBox.Show(form,L("Setup.ConfirmUninstall",target.Obs),Product,MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes) return;
                install.Enabled=uninstall.Enabled=browse.Enabled=close.Enabled=false;status.Text=removing?L("Setup.Removing"):L("Setup.Installing");form.Refresh();
                if(!portable.Checked && !new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator)) {
                    var start=new ProcessStartInfo(OwnExe,(removing?"--uninstall":"--install")+" --standard --obs \""+target.Obs+"\"") { UseShellExecute=true,Verb="runas",WindowStyle=ProcessWindowStyle.Hidden };
                    using(var process=Process.Start(start)){process.WaitForExit();if(process.ExitCode!=0)throw new IOException(L("Setup.OperationFailed"));}
                    status.Text=removing?L("Setup.Removed"):L("Setup.Installed");
                } else { if(removing)status.Text=Uninstall(target);else {Install(target);status.Text=L("Setup.Installed");} }
            } catch(Exception e) { status.Text=L("Setup.NotCompleted");MessageBox.Show(form,e.Message,Product,MessageBoxButtons.OK,MessageBoxIcon.Error); }
            finally { install.Enabled=HasPayload;uninstall.Enabled=browse.Enabled=close.Enabled=true; }
        };
        install.Click+=delegate{run(false);};uninstall.Click+=delegate{run(true);};close.Click+=delegate{form.Close();};
        form.Controls.AddRange(new Control[]{branding,header,title,help,folderLabel,portable,folder,browse,status,divider,install,uninstall,close});
        if(args.Length==2 && args[0]=="--preview") {
            form.Show();form.Refresh();Application.DoEvents();
            using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(0,0,image.Width,image.Height));image.Save(Path.GetFullPath(args[1]),System.Drawing.Imaging.ImageFormat.Png);}
            form.Close();form.Dispose();return 0;
        }
        Application.Run(form);return 0;
    }
}

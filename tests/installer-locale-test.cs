using System;
using System.Globalization;
using System.Reflection;
using System.Collections.Generic;
class LocaleInstallerTest {
 [STAThread] static int Main(string[] args) {
  var assembly=Assembly.LoadFrom(args[0]);var type=assembly.GetType("InstallerText",true);
  var map=(Dictionary<string,string[]>)type.GetField("Texts",BindingFlags.NonPublic|BindingFlags.Static).GetValue(null);
  var method=type.GetMethod("Get",BindingFlags.Public|BindingFlags.Static);
  string[] languages={"es-ES","en-US","de-DE","fr-FR","it-IT"};int checks=0;
  for(int i=0;i<languages.Length;i++){
   CultureInfo.CurrentUICulture=new CultureInfo(languages[i]);
   foreach(var item in map){string value=(string)method.Invoke(null,new object[]{item.Key,new object[]{"OBS"}});if(String.IsNullOrEmpty(value)||value!=String.Format(item.Value[i],"OBS"))return 1;checks++;}
   if(args.Length>2 && languages[i]==args[2]){var main=assembly.EntryPoint;int result=(int)main.Invoke(null,new object[]{new[]{"--preview",System.IO.Path.Combine(args[1],"setup-"+languages[i]+".png")}});if(result!=0)return 2;}
  }
  Console.WriteLine("Installer localization: "+checks+" message checks PASS");return 0;
 }
}

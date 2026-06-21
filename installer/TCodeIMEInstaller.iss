#define MyAppName "T-Code IME"
#define MyAppPublisher "Niche App Lab"
#define SourcePath ".."

#ifndef MyAppVersion
#define MyAppVersion "0.1.1.0"
#endif

#ifndef TCodeArch
#define TCodeArch "x64"
#endif

#ifndef JreSuffix
  #define JreSuffix ""
#endif

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AppId={{F7F2AE39-9C4D-4EFA-A8B2-9F1E6A7D0D2F}}
DisableProgramGroupPage=yes
OutputDir={#SourcePath}\installer\output
OutputBaseFilename=NicheAppLab.TCodeIME_{#MyAppVersion}_{#TCodeArch}{#JreSuffix}
Compression=lzma2/ultra
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

#ifndef Config
#define Config "Release"
#endif

#ifndef TCodeIMEDllPath
#define TCodeIMEDllPath SourcePath + "\build\" + TCodeArch + "\" + Config + "\TCodeIME.dll"
#endif

#ifndef TCodeProxyBinDir
#define TCodeProxyBinDir SourcePath + "\build\" + TCodeArch + "\proxy"
#endif

#ifndef TCodeEngineDir
#define TCodeEngineDir SourcePath + "\engine"
#endif

[Files]
Source: "{#TCodeIMEDllPath}"; DestDir: "{app}"; Flags: ignoreversion regserver
Source: "{#TCodeProxyBinDir}\{#Config}\TCodeProxy.exe"; DestDir: "{app}\proxy"; Flags: ignoreversion
Source: "{#TCodeEngineDir}\*"; DestDir: "{app}\engine"; Flags: recursesubdirs createallsubdirs
#ifdef TCODEBundleJre
; Bundle the jlink'd Java runtime
Source: "{#TCODE_JRE_DIR}\*"; DestDir: "{app}\engine\jre"; Flags: recursesubdirs createallsubdirs
#endif
; License and third-party notices
Source: "{#SourcePath}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourcePath}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName} Proxy"; Filename: "{app}\proxy\TCodeProxy.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "TCodeIME Proxy"; ValueData: """{app}\proxy\TCodeProxy.exe"""; Flags: uninsdeletevalue

; [Run] section removed – registration handled by regserver flag
; No manual regsvr32 call needed
; Proxy launch can stay in a separate task if required

[UninstallRun]
Filename: "taskkill.exe"; Parameters: "/F /IM TCodeProxy.exe"; Flags: runhidden; RunOnceId: "Kill TCodeProxy"
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\TCodeIME.dll"""; Flags: runhidden shellexec; RunOnceId: "UnregisterTCodeIME"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

#define MyAppName "T-Code IME"
#define MyAppPublisher "Niche App Lab"
#define SourcePath ".."

#ifndef MyAppVersion
#define MyAppVersion "0.1.1.0"
#endif

#ifndef TCodeArch
#define TCodeArch "x64"
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
OutputBaseFilename=NicheAppLab.TCodeIME_{#MyAppVersion}_{#TCodeArch}
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

[Files]
Source: "{#TCodeIMEDllPath}"; DestDir: "{app}"; Flags: ignoreversion regserver
Source: "{#TCodeProxyBinDir}\*"; DestDir: "{app}\proxy"; Flags: recursesubdirs createallsubdirs
Source: "{#SourcePath}\engine\*"; DestDir: "{app}\engine"; Flags: recursesubdirs createallsubdirs

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
const
  faDirectory = $0010;

function GetJavaHomeFromRegistry(var JavaHome: String): Boolean;
begin
  Result := False;

  if RegQueryStringValue(HKLM, 'SOFTWARE\\JavaSoft\\Java Runtime Environment', 'CurrentVersion', JavaHome) then
  begin
    if RegQueryStringValue(HKLM, 'SOFTWARE\\JavaSoft\\Java Runtime Environment\\' + JavaHome, 'JavaHome', JavaHome) then
      Result := FileExists(ExpandConstant(JavaHome + '\\bin\\java.exe'));
  end;

  if not Result then
  begin
    if RegQueryStringValue(HKLM, 'SOFTWARE\\Wow6432Node\\JavaSoft\\Java Runtime Environment', 'CurrentVersion', JavaHome) then
    begin
      if RegQueryStringValue(HKLM, 'SOFTWARE\\Wow6432Node\\JavaSoft\\Java Runtime Environment\\' + JavaHome, 'JavaHome', JavaHome) then
        Result := FileExists(ExpandConstant(JavaHome + '\\bin\\java.exe'));
    end;
  end;
end;

function IsJavaOnPath(): Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec('java', '-version', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
end;

function FindJavaExeRecursive(const BaseDir: String; var JavaExe: String; Depth: Integer): Boolean;
begin
  Result := False;
end;

function FindJavaHomeInLocalJre(var JavaHome: String): Boolean;
var
  StartDir: String;
  JavaExe: String;
  SearchDir: String;
  Level: Integer;
begin
  Result := False;
  StartDir := ExtractFileDir(ExpandConstant('{srcexe}'));

  for Level := 0 to 4 do
  begin
    if StartDir = '' then Break;

    SearchDir := AddBackslash(StartDir) + 'jre';
    if FindJavaExeRecursive(SearchDir, JavaExe, 4) then
    begin
      JavaHome := ExtractFileDir(ExtractFileDir(JavaExe));
      Result := True;
      Exit;
    end;

    SearchDir := AddBackslash(StartDir) + 'jre-openjdk';
    if FindJavaExeRecursive(SearchDir, JavaExe, 4) then
    begin
      JavaHome := ExtractFileDir(ExtractFileDir(JavaExe));
      Result := True;
      Exit;
    end;

    SearchDir := AddBackslash(StartDir) + 'java-runtime';
    if FindJavaExeRecursive(SearchDir, JavaExe, 4) then
    begin
      JavaHome := ExtractFileDir(ExtractFileDir(JavaExe));
      Result := True;
      Exit;
    end;

    StartDir := ExtractFileDir(StartDir);
  end;
end;

function SetJavaEnvironmentVars(const JavaHome: String): Boolean;
var
  EnvPath: String;
  JavaBin: String;
begin
  Result := False;
  JavaBin := AddBackslash(JavaHome) + 'bin';

  RegWriteStringValue(HKLM, 'SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment', 'JAVA_HOME', JavaHome);

  if RegQueryStringValue(HKLM, 'SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment', 'Path', EnvPath) then
  begin
    if Pos(Uppercase(AddBackslash(JavaBin)), Uppercase(AddBackslash(EnvPath))) = 0 then
    begin
      EnvPath := EnvPath + ';' + JavaBin;
      RegWriteStringValue(HKLM, 'SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment', 'Path', EnvPath);
    end;
  end
  else
  begin
    RegWriteStringValue(HKLM, 'SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment', 'Path', JavaBin);
  end;

  Result := True;
end;

function InitializeSetup(): Boolean;
var
  JavaHome: String;
  Response: Integer;
  ResultCode: Integer;
begin
  if IsJavaOnPath() then
  begin
    if GetJavaHomeFromRegistry(JavaHome) then
    begin
      Response := MsgBox('Java Runtime is installed on PATH and has a registry-installed home at:'#13#10 +
                         JavaHome + #13#10#13#10 +
                         'Would you like the installer to register JAVA_HOME and add Java bin to PATH?',
                         mbConfirmation, MB_YESNO);
      if Response = IDYES then
        SetJavaEnvironmentVars(JavaHome);
    end;

    Result := True;
    exit;
  end;

  if GetJavaHomeFromRegistry(JavaHome) then
  begin
    Response := MsgBox('Java Runtime Environment was found at:'#13#10 +
                       JavaHome + #13#10#13#10 +
                       'Would you like the installer to register JAVA_HOME and add Java bin to PATH?',
                       mbConfirmation, MB_YESNO);
    if Response = IDYES then
      SetJavaEnvironmentVars(JavaHome);

    Result := True;
    exit;
  end;

  if FindJavaHomeInLocalJre(JavaHome) then
  begin
    Response := MsgBox('A local JRE was found at:'#13#10 +
                       JavaHome + #13#10#13#10 +
                       'Would you like the installer to register JAVA_HOME and add Java bin to PATH?',
                       mbConfirmation, MB_YESNO);
    if Response = IDYES then
      SetJavaEnvironmentVars(JavaHome);

    Result := True;
    exit;
  end;

  Response := MsgBox('Java Runtime Environment (JRE) is required for the T-Code engine.'#13#10#13#10 +
                     'Would you like the installer to automatically download and install JRE using winget?'#13#10 +
                     '(Requires internet connection. Click ''No'' to manually install from the web, or ''Cancel'' to exit.)',
                     mbConfirmation, MB_YESNOCANCEL);

  if Response = IDYES then
  begin
    MsgBox('A command window will now open to run winget. Please accept any prompt and wait for the installation to finish.', mbInformation, MB_OK);
    if Exec('cmd.exe', '/c winget install Oracle.JDK.21 --accept-source-agreements --accept-package-agreements', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0) then
    begin
      MsgBox('Java JRE has been successfully installed via winget!'#13#10#13#10 +
             'Please restart this installer so it can detect the new Java environment, configure JAVA_HOME, and add Java to your PATH.', mbInformation, MB_OK);
    end
    else
    begin
      MsgBox('winget installation failed or was cancelled (Exit code: ' + IntToStr(ResultCode) + ').'#13#10#13#10 +
             'We will now open the Oracle JRE download page for manual installation.', mbError, MB_OK);
      ShellExec('open', 'https://www.oracle.com/java/technologies/downloads/', '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    end;
    Result := False;
    exit;
  end
  else if Response = IDNO then
  begin
    Response := MsgBox('Would you like to open the Oracle JRE download page to install it manually?'#13#10#13#10 +
                       'IMPORTANT: After manual installation, make sure to set the JAVA_HOME environment variable and add %JAVA_HOME%\bin to your system PATH so that the T-Code engine can run.',
                       mbConfirmation, MB_YESNO);
    if Response = IDYES then
    begin
      ShellExec('open', 'https://www.oracle.com/java/technologies/downloads/', '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    end;
    Result := False;
    exit;
  end;

  Result := False;
end;

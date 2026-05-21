#define MyAppName "T-Code IME"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "T-Code"
#define SourcePath ".."

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AppId={{F7F2AE39-9C4D-4EFA-A8B2-9F1E6A7D0D2F}}
DisableProgramGroupPage=yes
OutputDir={#SourcePath}\installer\output
OutputBaseFilename=TCodeIMEInstaller
Compression=lzma2/ultra
SolidCompression=yes
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourcePath}\build\Release\TCodeIME.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourcePath}\proxy\TCodeProxy\bin\Release\net10.0-windows\*"; DestDir: "{app}\proxy"; Flags: recursesubdirs createallsubdirs
Source: "{#SourcePath}\engine\*"; DestDir: "{app}\engine"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName} Proxy"; Filename: "{app}\proxy\TCodeProxy.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "TCodeIME Proxy"; ValueData: """{app}\proxy\TCodeProxy.exe"""; Flags: uninsdeletevalue

[Run]
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\TCodeIME.dll"""; StatusMsg: "Registering T-Code IME..."; Flags: runhidden shellexec
Filename: "{app}\TCodeProxy.exe"; Description: "Launch T-Code IME Proxy"; Flags: nowait skipifdoesntexist postinstall

[UninstallRun]
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

  MsgBox('Java Runtime Environment is required for the T-Code engine.'#13#10#13#10 +
         'Please install a compatible JRE before running this installer.'#13#10#13#10 +
         'Recommended NuGet command:'#13#10 +
         '  nuget install Microsoft.Java.OpenJDK.17 -OutputDirectory .\jre', mbError, MB_OK);
  Result := False;
end;

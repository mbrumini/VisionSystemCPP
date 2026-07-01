#define AppVersion "0.1006"
#define PackageRoot "E:\\VisionSystemCPP\\dist\\runtime_staging\\VisionSystemCPP"

[Setup]
AppId={{7B2A5690-A781-40A2-92C9-273318C86C3D}
AppName=VisionSystemCPP Demo
AppVersion={#AppVersion}
AppPublisher=VisionSystemCPP
DefaultDirName=C:\VisionSystemCPP
DefaultGroupName=VisionSystemCPP
DisableProgramGroupPage=yes
OutputDir=E:\\VisionSystemCPP\\dist
OutputBaseFilename=VisionSystemCPP-demo-setup-0.1006
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayName=VisionSystemCPP Demo
SetupLogging=yes

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "installpython"; Description: "Installa ambiente Python per classificazione AI (richiede Python 3.11)"; GroupDescription: "Componenti aggiuntivi:"; Flags: checkedonce
Name: "installollama"; Description: "Configura assistente help locale con Ollama"; GroupDescription: "Componenti aggiuntivi:"; Flags: unchecked

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "images,dataset,datasets,training,train,models,*.gguf,*.pt,*.onnx,*.bmp,*.jpg,*.jpeg,*.tif,*.tiff,*.webp"

[Dirs]
Name: "{app}\config"
Name: "{app}\recipes"
Name: "{app}\data"
Name: "{app}\logs"

[Icons]
Name: "{group}\VisionSystemCPP"; Filename: "{app}\VisionSystemCPP.exe"; WorkingDir: "{app}"
Name: "{group}\Disinstalla VisionSystemCPP"; Filename: "{app}\Uninstall_Runtime.bat"; WorkingDir: "{app}"
Name: "{autodesktop}\VisionSystemCPP"; Filename: "{app}\VisionSystemCPP.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Install-Runtime.ps1"" -InstallDir ""{app}"" -SetupPythonOnly"; StatusMsg: "Configurazione dell'ambiente Python AI..."; Tasks: installpython; Flags: runhidden waituntilterminated
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Install-Runtime.ps1"" -InstallDir ""{app}"" -SetupPythonOnly -SkipPython -InstallOllamaModel"; StatusMsg: "Configurazione dell'assistente Ollama..."; Tasks: installollama; Flags: runhidden waituntilterminated
Filename: "{app}\VisionSystemCPP.exe"; Description: "Avvia VisionSystemCPP"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Uninstall-Runtime.ps1"" -InstallDir ""{app}"" -Quiet"; RunOnceId: "VisionSystemCPPUninstallRuntime"; Flags: runhidden waituntilterminated

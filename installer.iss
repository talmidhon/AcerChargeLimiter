[Setup]
AppId={{EBE90447-32DE-4033-8F5E-9EC9E2C71112}}
AppName=Acer Battery Optimizer
AppVersion={#AppVersion}
VersionInfoVersion={#AppVersion}
AppPublisher=talmidhon
AppPublisherURL=https://github.com/talmidhon/AcerChargeLimiter
AppSupportURL=https://github.com/talmidhon/AcerChargeLimiter/issues
AppUpdatesURL=https://github.com/talmidhon/AcerChargeLimiter/releases

UninstallDisplayName=Acer Battery Optimizer
UninstallDisplayIcon={app}\AcerChargeLimiter.exe
CreateUninstallRegKey=yes
DisableProgramGroupPage=yes

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={autopf}\AcerChargeLimiter
DefaultGroupName=Acer Battery Optimizer
Compression=lzma2
SolidCompression=yes
OutputDir=Output
OutputBaseFilename=AcerChargeLimiter_Setup
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "hebrew"; MessagesFile: "compiler:Languages\Hebrew.isl"

[CustomMessages]
english.ShortcutName=Battery Optimizer
hebrew.ShortcutName=מיטוב סוללה

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "x64\Release\AcerChargeLimiter\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs; Excludes: "*.pdb,*.lib,*.exp,*.winmd"

[Icons]
Name: "{autoprograms}\{cm:ShortcutName}"; Filename: "{app}\AcerChargeLimiter.exe"
Name: "{autodesktop}\{cm:ShortcutName}"; Filename: "{app}\AcerChargeLimiter.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AcerChargeLimiter.exe"; Description: "{cm:LaunchProgram,{cm:ShortcutName}}"; Flags: nowait postinstall runascurrentuser

[UninstallRun]
Filename: "taskkill.exe"; Parameters: "/F /IM AcerChargeLimiter.exe"; Flags: runhidden
Filename: "schtasks.exe"; Parameters: "/Delete /TN ""AcerChargeLimiter"" /F"; Flags: runhidden

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\AcerChargeLimiter"
Type: filesandordirs; Name: "{app}\*.log"
Type: filesandordirs; Name: "{app}\*.tmp"
# AllyFSE

AllyFSE is a ROG Ally / Windows handheld Full Screen Experience session manager based on AnyFSE. It lets Windows use this app as the Gaming Home application, then starts your chosen launcher such as Playnite, Steam Big Picture, BigBox, Armoury Crate SE, One Game Launcher, RetroBat, Kodi, Razer Cortex, or a custom executable.

This fork keeps the inherited AnyFSE package identity, executable names, protocol, install path, and config path for now. Visible product branding is AllyFSE, but a full identity rename needs coordinated AppX, signing, installer, updater, protocol, and Gaming Home registration changes.

## Features

- Launch Playnite, Steam Big Picture, BigBox, Armoury Crate SE, One Game Launcher, RetroBat, Kodi, Razer Cortex, native Gaming Home apps, or a custom launcher.
- Run configured startup apps before the home launcher starts.
- Trigger an existing elevated scheduled task during FSE startup.
- Wait for startup-app processes before launching the home launcher.
- Exit FSE when the home launcher exits, with optional lock-on-desktop-exit behavior.
- Reenter FSE with the `/FSE` command.
- ASUS ROG Ally button remapping/filter support inherited from AnyFSE.

## Boot Flow

```text
Windows auto-login
-> Windows enters FSE
-> Windows launches AllyFSE as Gaming Home app
-> AllyFSE launches StartupApps
-> AllyFSE triggers optional elevated scheduled tasks
-> AllyFSE waits for configured startup processes
-> AllyFSE launches Playnite, Steam, BigBox, or your selected home launcher
```

## Startup Apps

Existing AnyFSE startup-app entries remain supported:

```json
{
  "Path": "C:\\SomeApp\\App.exe",
  "Args": "",
  "Enabled": true
}
```

New startup-app entries can use these fields:

```json
{
  "Name": "Handheld Companion",
  "Enabled": true,
  "RunMode": "ElevatedTask",
  "Path": "",
  "Args": "",
  "TaskName": "Handheld Companion Elevated",
  "WaitForProcess": "HandheldCompanion.exe",
  "WaitTimeoutMs": 10000,
  "DelayAfterStartMs": 0,
  "Required": true
}
```

Supported `RunMode` values:

- `NormalExe`: launch `Path` with `Args` like classic AnyFSE startup apps.
- `ElevatedTask`: run `C:\Windows\System32\schtasks.exe /run /tn "<TaskName>"`.
- `Protocol`: launch `Path` as a URI/protocol when it contains `://`.
- `Script`: launch `.cmd`, `.bat`, or `.ps1` wrappers. PowerShell scripts are started with `-File`; AllyFSE does not add an execution-policy bypass.

After each enabled startup app is launched, AllyFSE applies `DelayAfterStartMs` and then waits for `WaitForProcess` until `WaitTimeoutMs` elapses. Timeouts are logged and do not hard-crash FSE; the home launcher still starts.

## Handheld Companion Elevated Startup

Create a scheduled task named `Handheld Companion Elevated` with:

- Run only when user is logged on
- Run with highest privileges
- Trigger can be omitted or left disabled if AllyFSE will trigger it manually
- Action: `C:\Program Files\Handheld Companion\HandheldCompanion.exe`
- Start in: `C:\Program Files\Handheld Companion`
- Allow start on battery
- Multiple instances: Ignore new

Then add this to AllyFSE startup apps:

```json
{
  "Name": "Handheld Companion",
  "Enabled": true,
  "RunMode": "ElevatedTask",
  "TaskName": "Handheld Companion Elevated",
  "WaitForProcess": "HandheldCompanion.exe",
  "WaitTimeoutMs": 10000,
  "Required": true
}
```

AllyFSE triggers the task during FSE startup. It does not create the task in this first pass.

## Lock On Desktop Exit

The existing `Extra.ExitFSEOnHomeExit` setting controls whether AllyFSE exits Full Screen Experience after the home launcher closes. The new `Exit` block controls what happens after desktop mode is detected.

```json
{
  "Extra": {
    "ExitFSEOnHomeExit": true
  },
  "Exit": {
    "OnDesktopExit": "LockWorkStation",
    "CommandPath": "",
    "CommandArgs": ""
  }
}
```

Supported `OnDesktopExit` values:

- `None`
- `LockWorkStation`
- `RunCommand`
- `LockThenRunCommand`

`LockWorkStation` calls the native Windows `LockWorkStation()` API immediately after AllyFSE confirms Windows has left FSE for desktop mode.

## Reenter FSE

Bind this command in Handheld Companion or another launcher:

```cmd
C:\Program Files\AnyFSE\AnyFSE.exe /FSE
```

If Windows is currently in desktop mode, this asks Windows to enter Gaming Full Screen Experience again. If already in FSE, it does nothing.

TODO: after a coordinated full identity rename, this command should become `C:\Program Files\AllyFSE\AllyFSE.exe /FSE`. That rename must update the AppX identity, package family name, AppUserModelId, executable names, installer/uninstaller/updater paths, signing identity, and Windows Gaming Home app registration together.

## ASUS ROG Ally Buttons

On ASUS ROG Ally devices AllyFSE can redefine the dedicated Armoury Crate, Command Center, and Library buttons, including Mode+ combinations. The inherited ACSE Filter Injector service can suppress conflicting ASUS Optimization button handling while leaving the rest of the input stack alone.

## Install And Configure

> [!NOTE]
> AllyFSE does not enable FSE support in Windows by itself. You need a supported handheld device such as ASUS ROG Ally, or another tool that enables Gaming Full Screen Experience support.

Launch `AnyFSE.Installer.exe`, wait for it to finish, then configure AllyFSE from the Start menu entry. Your launcher should be installed separately.

AllyFSE only acts as the home launcher when it is selected in Windows Settings -> Gaming -> Full screen experience.

## Splash Videos

AllyFSE may show shuffled video as a splash screen while your launcher is loading. Create a `splash` folder in `C:\ProgramData\AnyFSE` and put `.mp4` or `.webm` videos there.

You can specify a loop point in the filename, for example `splash.m4000.mp4` or `other_splash.5000.webm`:

- `m` or `M`: mute video during loop
- number: loop position in milliseconds

## Cleanup If Uninstall Was Broken

Run these commands from an elevated Command Prompt.

```cmd
powershell -Command "Get-AppxPackage *AnyFSE* | Remove-AppxPackage"
sc stop ACSEFilterInjector
sc delete ACSEFilterInjector
rmdir /s /q "%ProgramFiles%\AnyFSE"
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\AnyFSE" /f
```

## Future TODO

- Full AllyFSE package/binary/install-path identity rename.
- Plugged-in idle display behavior.

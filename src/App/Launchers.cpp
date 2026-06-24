// MIT License
//
// Copyright (c) 2025 Artem Shpynov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <string>
#include <filesystem>
#include <processenv.h>
#include "Logging/LogManager.hpp"
#include "Configuration/Config.hpp"
#include "Launchers.hpp"
#include "Tools/Process.hpp"
#include "Tools/Unicode.hpp"
#include "Tools/Packages.hpp"
#include "App/GamingExperience.hpp"


namespace AnyFSE::App::Launchers
{
    static Logger log = LogManager::GetLogger("Launchers");

    void LauncherOnBoot()
    {
        switch (Config::Launcher.Type)
        {
            case LauncherType::PlayniteDesktop:
            case LauncherType::PlayniteFullscreen:
                return PlayniteOnBoot();
        };
    }

    void LauncherOnStarted()
    {
        switch (Config::Launcher.Type)
        {
            case LauncherType::PlayniteDesktop:
            case LauncherType::PlayniteFullscreen:
                return PlayniteOnStarted();
        };
    }

    bool WaitLauncherExit()
    {
        while (HANDLE hProcess = Launchers::GetLauncherProcess())
        {
            log.Debug("Start waiting %x for %s", hProcess, Unicode::to_string(Config::Launcher.Name).c_str());

            DWORD waitResult = WAIT_TIMEOUT;
            do
            {
                waitResult = WaitForSingleObject(hProcess, 10000);
                log.Debug("Wait Result %u", waitResult);

                Config::LoadExitFSEOnHomeExit();

                if (!Config::ExitFSEOnHomeExit
                    || !App::GamingExperience::IsFullscreenMode())
                {
                    return true;
                }
            } while (waitResult == WAIT_TIMEOUT);

            CloseHandle(hProcess);
        };
        return !Launchers::GetLauncherProcess();
    }

    void PlayniteOnBoot()
    {
        if (Config::CleanupFailedStart)
        {
            log.Debug("Cleanup Playnite safe startup flag");

            namespace fs = std::filesystem;
            fs::path path = fs::path(Config::Launcher.StartCommand);

            fs::path configPath = path.parent_path();
            bool isPortable = !fs::exists(configPath.append(L"unins000.exe"));

            if (!isPortable)
            {
                log.Debug("Playnite is not portable, config is in %%APPDATA%%");

                wchar_t buffer[MAX_PATH] = {0};
                if (ExpandEnvironmentStringsW(L"%APPDATA%\\Playnite", buffer, MAX_PATH))
                {
                    configPath = fs::path(buffer);
                }
            }

            fs::path flagFile = configPath.append(L"safestart.flag");

            if(fs::exists(flagFile))
            {
                log.Debug("Safestart flag is exist at %s, deleting", flagFile.string().c_str());
                fs::remove(flagFile);
            }
        }
    }

    void PlayniteOnStarted()
    {
        log.Debug("Sending WM_DISPLAYCHANGE");

        HWND hWnd = GetLauncherWindow(true);
        if (!hWnd || !GamingExperience::IsFullscreenMode())
        {
            return;
        }
        // send WM_DISPLAYCHANGED
        HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {sizeof(MONITORINFO)};
        if (GetMonitorInfo(hMonitor, &monitorInfo))
        {
            PostMessage(HWND_BROADCAST, WM_DISPLAYCHANGE, 32,
                MAKELPARAM(
                    monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top
            ));
        }
        FocusLauncher();  // Attempt to fix activation of playnite
    }

    void StartLauncher()
    {
        log.Debug("Start Launcher: %s params: %s",
            Unicode::to_string(Config::Launcher.StartCommand).c_str(),
            Unicode::to_string(Config::Launcher.StartArg).c_str()
        );
        if (0 == Process::StartProcess(Config::Launcher.StartCommand, Config::Launcher.StartArg))
        {
            log.Error(log.APIError(), "Can't start launcher:" );
        }
    }

    bool IsLauncherActive()
    {
        const LauncherConfig& launcher = Config::Launcher;
        return GetLauncherWindow(true);
    }

    bool IsLauncherActiveOrMinimized()
    {
        const LauncherConfig& launcher = Config::Launcher;
        return GetLauncherWindow(true);
    }

    bool IsLauncherMinimized()
    {
        const LauncherConfig& launcher = Config::Launcher;
        HWND hWnd = GetLauncherWindow(true);
        return GetWindowLong(hWnd, GWL_STYLE) | WS_MINIMIZE;
    }

    void FocusLauncher()
    {
        if (!Config::Launcher.ActivationProtocol.empty())
        {
            if (Config::Launcher.ActivationProtocol[0]==L'@')
            {
                Process::StartProcess(Config::Launcher.StartCommand, Config::Launcher.StartArg);
            }
            else
            {
                Process::StartProtocol(Config::Launcher.ActivationProtocol);
            }
            return;
        }

        HWND launcherHwnd = GetLauncherWindow(true);
        if (launcherHwnd)
        {
            Process::BringWindowToForeground(launcherHwnd, SW_SHOWMAXIMIZED);
        }
    }

    HWND GetLauncherWindow(bool includeMinimized)
    {
        HWND launcherHwnd = nullptr;
        const LauncherConfig& launcher = Config::Launcher;

        if (!launcher.AppUserModelID.empty())
        {
            std::vector<DWORD> pids = Tools::Packages::GetAppProcessIds(launcher.AppUserModelID);
            std::set<DWORD> processIds(pids.begin(), pids.end());
            launcherHwnd = Process::GetWindow(processIds, 0, L"", L"", WS_VISIBLE);
        }

        if (!launcherHwnd)
        {
            launcherHwnd = Process::GetWindow(
                launcher.ProcessName, launcher.ExStyle,
                launcher.ClassName, launcher.WindowTitle,
                WS_VISIBLE, launcher.NoStyle
            );
        }

        if (!launcherHwnd && includeMinimized)
        {
            launcherHwnd = Process::GetWindow(
                launcher.ProcessName, launcher.ExStyle,
                launcher.ClassName, launcher.WindowTitle,
                WS_MINIMIZE, launcher.NoStyle
            );
        }

        if (!launcherHwnd)
        {
            launcherHwnd = Process::GetWindow(
                launcher.ProcessNameAlt, launcher.ExStyleAlt,
                launcher.ClassNameAlt, launcher.WindowTitleAlt,
                WS_VISIBLE, launcher.NoStyle
            );
        }

        if (!launcherHwnd && includeMinimized)
        {
            launcherHwnd = Process::GetWindow(
                launcher.ProcessNameAlt, launcher.ExStyleAlt,
                launcher.ClassNameAlt, launcher.WindowTitleAlt,
                WS_MINIMIZE, launcher.NoStyle
            );
        }
        return launcherHwnd;
    }

    bool HasLauncherProcess()
    {
        return (!Config::Launcher.AppUserModelID.empty() && !Tools::Packages::GetAppProcessIds(Config::Launcher.AppUserModelID).empty())
            || 0 != Process::FindFirstByName(Config::Launcher.ProcessName)
            || 0 != Process::FindFirstByName(Config::Launcher.ProcessNameAlt)
            || 0 != Process::FindFirstByExe(Config::Launcher.StartCommand)
            || IsLauncherActiveOrMinimized();
    }

    HANDLE GetLauncherProcess()
    {
        HWND hWnd = GetLauncherWindow(true);
        if (!hWnd)
        {
            return NULL;
        }

        DWORD processId;
        GetWindowThreadProcessId(hWnd, &processId);
        if (processId == 0)
        {
            return false;
        }

        // Open process with desired access
        return OpenProcess(
            SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            processId
        );
    }

    static std::wstring GetStartupAppLogName(const StartupApp &app)
    {
        if (!app.Name.empty())
        {
            return app.Name;
        }
        if (!app.TaskName.empty())
        {
            return app.TaskName;
        }
        if (!app.Path.empty())
        {
            return app.Path;
        }
        return L"Startup app";
    }

    static bool StartStartupApp(const StartupApp &app)
    {
        const std::wstring name = GetStartupAppLogName(app);
        const std::wstring runMode = StartupRunModeToString(app.RunMode);

        log.Info("Startup app launch start: %s", Unicode::to_string(name).c_str());
        log.Debug("Startup app run mode: %s", Unicode::to_string(runMode).c_str());

        switch (app.RunMode)
        {
            case StartupRunMode::ElevatedTask:
            {
                log.Debug("Trigger startup scheduled task: %s", Unicode::to_string(app.TaskName).c_str());
                DWORD result = Process::StartScheduledTask(app.TaskName);
                if (result == ERROR_SUCCESS)
                {
                    log.Info("Startup scheduled task trigger succeeded: %s", Unicode::to_string(app.TaskName).c_str());
                    return true;
                }
                log.Warn("Startup scheduled task trigger failed with code %lu: %s", result, Unicode::to_string(app.TaskName).c_str());
                return false;
            }
            case StartupRunMode::Protocol:
            {
                if (app.Path.find(L"://") == std::wstring::npos)
                {
                    log.Warn("Startup protocol path does not look like a URI: %s", Unicode::to_string(app.Path).c_str());
                    return false;
                }
                log.Debug("Launch startup protocol: %s", Unicode::to_string(app.Path).c_str());
                return Process::StartProtocol(app.Path) != 0;
            }
            case StartupRunMode::Script:
            {
                log.Debug("Launch startup script: %s %s", Unicode::to_string(app.Path).c_str(), Unicode::to_string(app.Args).c_str());
                return Process::StartScript(app.Path, app.Args) != 0;
            }
            case StartupRunMode::NormalExe:
            default:
            {
                log.Debug("Launch startup executable: %s %s", Unicode::to_string(app.Path).c_str(), Unicode::to_string(app.Args).c_str());
                return Process::StartProcess(app.Path, app.Args) != 0;
            }
        }
    }

    void LaunchStartupApps()
    {
        log.Debug("Launching Startup Applications" );
        for (const auto &app : Config::StartupApps)
        {
            if (!app.Enabled)
            {
                continue;
            }

            const std::wstring name = GetStartupAppLogName(app);
            bool started = StartStartupApp(app);
            if (!started)
            {
                if (app.Required)
                {
                    log.Error("Required startup app did not launch successfully: %s", Unicode::to_string(name).c_str());
                }
                else
                {
                    log.Warn("Startup app did not launch successfully: %s", Unicode::to_string(name).c_str());
                }
            }

            if (app.DelayAfterStartMs > 0)
            {
                log.Debug("Startup app delay after start: %lu ms for %s", app.DelayAfterStartMs, Unicode::to_string(name).c_str());
                Sleep(app.DelayAfterStartMs);
            }

            if (!app.WaitForProcess.empty())
            {
                log.Debug("Waiting for startup app process %s up to %lu ms", Unicode::to_string(app.WaitForProcess).c_str(), app.WaitTimeoutMs);
                DWORD processId = Process::WaitForProcess(app.WaitForProcess, app.WaitTimeoutMs);
                if (processId != 0)
                {
                    log.Info("Startup app process detected: %s pid=%lu", Unicode::to_string(app.WaitForProcess).c_str(), processId);
                }
                else if (app.Required)
                {
                    log.Error("Required startup app process wait timed out after %lu ms: %s", app.WaitTimeoutMs, Unicode::to_string(app.WaitForProcess).c_str());
                }
                else
                {
                    log.Warn("Startup app process wait timed out after %lu ms: %s", app.WaitTimeoutMs, Unicode::to_string(app.WaitForProcess).c_str());
                }
            }
        }
    }
    bool HasStartupApps()
    {
        for (auto app : Config::StartupApps)
        {
            if (app.Enabled)
            {
                return true;
            }
        }
        return false;
    }
}

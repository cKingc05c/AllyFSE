#include <algorithm>
#include <cwchar>
#include <string>
#include "Tools/Event.hpp"
#include "Tools/Process.hpp"
#include "AppSettings/SettingsLayout.hpp"
#include "AppSettings/SettingsDialog.hpp"
#include "AppSettings/SettingsPages/StartupEditDlg.hpp"
#include "StartupPage.hpp"
#include "Tools/Localization.hpp"


namespace AnyFSE::App::AppSettings::Settings::Page
{
    namespace
    {
        constexpr int StartupDataPath = 0;
        constexpr int StartupDataArgs = 1;
        constexpr int StartupDataName = 2;
        constexpr int StartupDataRunMode = 3;
        constexpr int StartupDataTaskName = 4;
        constexpr int StartupDataWaitForProcess = 5;
        constexpr int StartupDataWaitTimeoutMs = 6;
        constexpr int StartupDataDelayAfterStartMs = 7;
        constexpr int StartupDataRequired = 8;

        std::wstring BoolToString(bool value)
        {
            return value ? L"true" : L"false";
        }

        bool BoolFromString(const std::wstring &value)
        {
            return value == L"1" || _wcsicmp(value.c_str(), L"true") == 0;
        }

        DWORD DwordFromString(const std::wstring &value, DWORD defaultValue)
        {
            if (value.empty())
            {
                return defaultValue;
            }
            try
            {
                return static_cast<DWORD>(std::stoul(value));
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        std::wstring GetStartupAppDisplayName(const StartupApp &app)
        {
            if (!app.Name.empty())
            {
                return app.Name;
            }
            if (!app.Path.empty())
            {
                return Config::GetApplicationName(app.Path);
            }
            if (!app.TaskName.empty())
            {
                return app.TaskName;
            }
            return L"Startup app";
        }

        std::wstring GetStartupAppDescription(const StartupApp &app)
        {
            std::wstring description = StartupRunModeToString(app.RunMode) + L": ";
            if (app.RunMode == StartupRunMode::ElevatedTask)
            {
                description += app.TaskName.empty() ? L"<missing task name>" : app.TaskName;
            }
            else
            {
                description += app.Path;
                if (!app.Args.empty())
                {
                    description += L" " + app.Args;
                }
            }

            if (!app.WaitForProcess.empty())
            {
                description += L" | wait " + app.WaitForProcess + L" " + std::to_wstring(app.WaitTimeoutMs) + L" ms";
                if (app.Required)
                {
                    description += L" required";
                }
            }
            return description;
        }
    }

    void StartupPage::AddPage(std::list<SettingsLine>& settingPageList, ULONG &top)
    {
        ULONG pageTop = 0;
        SettingsLine &startupAppLinkLine = m_dialog.AddSettingsLine(m_pageLinesList, pageTop,
            Translate(L"settingsNativeStartupSettings"),
            Translate(L"settingsNativeStartupSettingsDescription"),
            Layout::LineHeight, Layout::LinePadding, 0);

        startupAppLinkLine.SetState(FluentDesign::SettingsLine::Link);
        startupAppLinkLine.SetIcon(L'\xE18C');
        startupAppLinkLine.OnChanged += delegate(OpenMSSettingsStartupApps);

        m_pStartupPageAppsHeader = &m_dialog.AddSettingsLine(m_pageLinesList, pageTop,
            Translate(L"settingsAdditionalStartupApplications"),
            Translate(L"settingsAdditionalStartupApplicationsDescription"),
            Layout::LineHeight, 0, 0
        );

        m_pStartupPageAppsHeader->SetState(FluentDesign::SettingsLine::Caption);

        SettingsLine & line = m_dialog.AddSettingsLine(m_pageLinesList, pageTop,
            L"",
            L"",
            m_startupAddButton,
            Layout::LineHeightSmall, Layout::LinePadding, Layout::LineSmallMargin,
            Layout::StartupAddWidth, Layout::StartupAddHeight
        );

        m_startupAddButton.SetText(Translate(L"addBtn"));
        m_startupAddButton.OnChanged = delegate(OnStartupAdd);
        line.SetState(FluentDesign::SettingsLine::Caption);
    }

    void StartupPage::LoadControls()
    {
        for (const auto &app : Config::StartupApps)
        {
            AddStartupAppLine(app);
        }
    }

    void StartupPage::SaveControls()
    {
        Config::StartupApps.clear();

        for (auto pLine : m_pStartupAppLines)
        {
            Toggle *toggle = GetStartupLineToggle(pLine);
            StartupApp app = GetStartupAppLine(pLine);
            app.Enabled = toggle ? toggle->GetCheck() : true;
            Config::StartupApps.push_back(app);
        }
    }

    void StartupPage::OpenMSSettingsStartupApps()
    {
        Process::StartProtocol(L"ms-settings:startupapps");
    }

    void StartupPage::AddStartupAppLine(const StartupApp &app)
    {
        ULONG top = 0;
        m_startupToggles.emplace_back(m_theme);
        Toggle &startToggle = m_startupToggles.back();

        SettingsLine & line = m_dialog.AddSettingsLine(m_pageLinesList, top,
            L"",
            L"",
            startToggle,
            Layout::LineHeight, Layout::LinePaddingSmall, 0,
            Layout::StartupMenuButtonWidth, Layout::StartupMenuButtonHeight
        );

        SetStartupAppLine(&line, app);

        line.SetMenu(
            std::vector<FluentDesign::Popup::PopupItem>
            {
                FluentDesign::Popup::PopupItem(L"\xE13E", Translate(L"modifyBtn"),[This = this, pLine = &line](){This->OnStartupModify(pLine);}),
                FluentDesign::Popup::PopupItem(L"\xE107", Translate(L"deleteBtn"),[This = this, pLine = &line](){This->OnStartupDelete(pLine);})
            }
        );
        startToggle.SetCheck(app.Enabled);
        m_pStartupPageAppsHeader->AddGroupItem(&line);

        m_pStartupAppLines.push_back(&line);
    }

    Toggle * StartupPage::GetStartupLineToggle(SettingsLine * pLine)
    {
        auto it = std::find_if(m_startupToggles.begin(), m_startupToggles.end(),
            [pLine](const Toggle& obj)
            {
                return obj.GetHwnd() == pLine->GetChildControl();
            }
        );

        return it != m_startupToggles.end() ? &(*it) : nullptr;
    }

    StartupApp StartupPage::GetStartupAppLine(SettingsLine *pLine)
    {
        StartupApp app;
        app.Path = pLine->GetData(StartupDataPath);
        app.Args = pLine->GetData(StartupDataArgs);
        app.Name = pLine->GetData(StartupDataName);
        app.RunMode = StartupRunModeFromString(pLine->GetData(StartupDataRunMode), StartupRunMode::NormalExe);
        app.TaskName = pLine->GetData(StartupDataTaskName);
        app.WaitForProcess = pLine->GetData(StartupDataWaitForProcess);
        app.WaitTimeoutMs = DwordFromString(pLine->GetData(StartupDataWaitTimeoutMs), app.WaitTimeoutMs);
        app.DelayAfterStartMs = DwordFromString(pLine->GetData(StartupDataDelayAfterStartMs), app.DelayAfterStartMs);
        app.Required = BoolFromString(pLine->GetData(StartupDataRequired));
        return app;
    }

    void StartupPage::SetStartupAppLine(SettingsLine *pLine, const StartupApp &app)
    {
        pLine->SetData(StartupDataPath, app.Path);
        pLine->SetData(StartupDataArgs, app.Args);
        pLine->SetData(StartupDataName, app.Name);
        pLine->SetData(StartupDataRunMode, StartupRunModeToString(app.RunMode));
        pLine->SetData(StartupDataTaskName, app.TaskName);
        pLine->SetData(StartupDataWaitForProcess, app.WaitForProcess);
        pLine->SetData(StartupDataWaitTimeoutMs, std::to_wstring(app.WaitTimeoutMs));
        pLine->SetData(StartupDataDelayAfterStartMs, std::to_wstring(app.DelayAfterStartMs));
        pLine->SetData(StartupDataRequired, BoolToString(app.Required));

        pLine->SetName(GetStartupAppDisplayName(app));
        pLine->SetDescription(GetStartupAppDescription(app));

        if (!app.Path.empty())
        {
            pLine->SetIcon(app.Path);
        }
    }

    void StartupPage::OnStartupAdd()
    {
        OnStartupModify(NULL);
    }

    void StartupPage::OnStartupModify(SettingsLine * pLine)
    {
        StartupApp app = pLine ? GetStartupAppLine(pLine) : StartupApp();
        std::wstring path = app.Path;
        std::wstring args = app.Args;
        if (IDOK == StartupEditDlg::EditApp(m_dialog.GetHwnd(), path, args))
        {
            app.Path = path;
            app.Args = args;
            if (pLine)
            {
                SetStartupAppLine(pLine, app);
            }
            else
            {
                app.Enabled = true;
                AddStartupAppLine(app);
            }
            m_dialog.UpdateLayout();
        }
    }

    void StartupPage::OnStartupDelete(SettingsLine * pLine)
    {
        if (pLine->GetGroupHeader())
        {
            pLine->GetGroupHeader()->DeleteGroupItem(pLine);
        }
        m_startupToggles.remove_if([pLine](const Toggle& obj) {return obj.GetHwnd() == pLine->GetChildControl();});
        m_pStartupAppLines.remove_if([pLine](const SettingsLine* obj) {return obj == pLine;});
        m_pageLinesList.remove_if([pLine](const SettingsLine& obj) {return &obj == pLine;});
        m_dialog.UpdateLayout();
    }

};

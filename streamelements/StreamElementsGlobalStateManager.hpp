#pragma once

#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsMenuManager.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsBandwidthTestManager.hpp"
#include "StreamElementsOutputSettingsManager.hpp"
#include "StreamElementsWorkerManager.hpp"
#include "StreamElementsHotkeyManager.hpp"
#include "StreamElementsPerformanceHistoryTracker.hpp"
#include "StreamElementsAnalyticsEventsManager.hpp"

class StreamElementsGlobalStateManager :
	public StreamElementsObsAppMonitor
{
private:
	StreamElementsGlobalStateManager();
	virtual ~StreamElementsGlobalStateManager();

public:
	static StreamElementsGlobalStateManager* GetInstance();

public:
	void Initialize(QMainWindow* obs_main_window);
	void Shutdown();

	void Reset(bool deleteAllCookies = true, bool forceOnboarding = false);
	void DeleteCookies();
	void StartOnBoardingUI(bool forceOnboarding);
	void StopOnBoardingUI();
	void SwitchToOBSStudio();

	void PersistState();
	void RestoreState();

	StreamElementsBrowserWidgetManager* GetWidgetManager() { return m_widgetManager; }
	StreamElementsMenuManager* GetMenuManager() { return m_menuManager; }
	StreamElementsBandwidthTestManager* GetBandwidthTestManager() { return m_bwTestManager; }
	StreamElementsOutputSettingsManager* GetOutputSettingsManager() { return m_outputSettingsManager; }
	StreamElementsWorkerManager* GetWorkerManager() { return m_workerManager; }
	StreamElementsHotkeyManager* GetHotkeyManager() { return m_hotkeyManager; }
	StreamElementsPerformanceHistoryTracker* GetPerformanceHistoryTracker() { return m_performanceHistoryTracker; }
	StreamElementsAnalyticsEventsManager* GetAnalyticsEventsManager() { return m_analyticsEventsManager; }
	QMainWindow* mainWindow() { return m_mainWindow; }

public:
	bool DeserializeStatusBarTemporaryMessage(CefRefPtr<CefValue> input);
	bool DeserializePopupWindow(CefRefPtr<CefValue> input);
	bool DeserializeModalDialog(CefRefPtr<CefValue> input, CefRefPtr<CefValue>& output);

	void ReportIssue();
	void UninstallPlugin();

protected:
	virtual void OnObsExit() override;

private:
	bool m_persistStateEnabled = false;
	bool m_initialized = false;
	QMainWindow* m_mainWindow = nullptr;
	StreamElementsBrowserWidgetManager* m_widgetManager = nullptr;
	StreamElementsMenuManager* m_menuManager = nullptr;
	StreamElementsBandwidthTestManager* m_bwTestManager = nullptr;
	StreamElementsOutputSettingsManager* m_outputSettingsManager = nullptr;
	StreamElementsWorkerManager* m_workerManager = nullptr;
	StreamElementsHotkeyManager* m_hotkeyManager = nullptr;
	StreamElementsPerformanceHistoryTracker* m_performanceHistoryTracker = nullptr;
	StreamElementsAnalyticsEventsManager* m_analyticsEventsManager = nullptr;

private:
	static StreamElementsGlobalStateManager* s_instance;

private:
	class ThemeChangeListener :
		public QDockWidget
	{
	public:
		ThemeChangeListener();

	protected:
		virtual void changeEvent(QEvent* event) override;

		std::string m_currentTheme;
	};

	class ApplicationStateListener :
		public QObject
	{
	public:
		ApplicationStateListener();
		~ApplicationStateListener();

	protected:
		void applicationStateChanged();

	private:
		QTimer m_timer;
	};

	QDockWidget* m_themeChangeListener;
	ApplicationStateListener* m_appStateListener;
};

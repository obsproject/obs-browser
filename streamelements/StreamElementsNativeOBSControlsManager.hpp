#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QObject>
#include <QTimer>

#include "obs.h"
#include "obs-hotkey.h"

#include <mutex>
#include <algorithm>

#include "cef-headers.hpp"

class StreamElementsNativeOBSControlsManager : QObject
{
	Q_OBJECT

public:
	enum start_streaming_mode_t {
		start = 0,
		request = 1
	};

public:
	static StreamElementsNativeOBSControlsManager* GetInstance();

private:
	StreamElementsNativeOBSControlsManager(QMainWindow* mainWindow);
	virtual ~StreamElementsNativeOBSControlsManager();

public:
	QMainWindow* mainWindow() { return m_mainWindow; }

	void SetStartStreamingMode(start_streaming_mode_t mode) { m_start_streaming_mode = mode; }
	start_streaming_mode_t GetStartStreamingMode() { return m_start_streaming_mode; }

	void SetStartStreamingAckTimeoutSeconds(int seconds) { m_startStreamingRequestAcknowledgeTimeoutSeconds = std::max<int>(seconds, 1); }
	int GetStartStreamingAckTimeoutSeconds() { return m_startStreamingRequestAcknowledgeTimeoutSeconds; }

	bool DeserializeStartStreamingUIHandlerProperties(CefRefPtr<CefValue> input);

	void AdviseRequestStartStreamingAccepted();
	void AdviseRequestStartStreamingRejected();

private:
	void SetStreamingInitialState();
	void SetStreamingActiveState();
	void SetStreamingStoppedState();
	void SetStreamingTransitionState();
	void SetStreamingTransitionStartingState();
	void SetStreamingTransitionStoppingState();
	void SetStreamingRequestedState();

	void SetStreamingStyle(bool streaming);

	void InitHotkeys();
	void ShutdownHotkeys();

	void StartTimeoutTracker();
	void StopTimeoutTracker();

protected:
	void BeginStartStreaming();

private slots:
	void OnStartStopStreamingButtonClicked();
	void OnStartStopStreamingButtonUpdate();

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event, void* data);

	static bool hotkey_enum_callback(void* data, obs_hotkey_id id, obs_hotkey_t* key);
	static void hotkey_change_handler(void* data, calldata_t* param);
	static void hotkey_routing_func(void* data, obs_hotkey_id id, bool pressed);

private:
	QMainWindow* m_mainWindow = nullptr;
	QPushButton* m_startStopStreamingButton = nullptr;
	QPushButton* m_nativeStartStopStreamingButton = nullptr;
	obs_hotkey_id m_startStopStreamingHotkeyId = OBS_INVALID_HOTKEY_ID;
	start_streaming_mode_t m_start_streaming_mode = start;
	int m_startStreamingRequestAcknowledgeTimeoutSeconds = 5;
	QTimer* m_timeoutTimer = nullptr;
	std::recursive_mutex m_timeoutTimerMutex;
};

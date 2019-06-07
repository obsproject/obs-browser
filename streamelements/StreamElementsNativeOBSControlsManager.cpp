#include "StreamElementsNativeOBSControlsManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"
#include <QDockWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMetaObject>
#include <QMessageBox>
#include <obs-module.h>
#include <obs-frontend-api.h>

StreamElementsNativeOBSControlsManager::StreamElementsNativeOBSControlsManager(QMainWindow* mainWindow) :
	m_mainWindow(mainWindow)
{
	QDockWidget* controlsDock = (QDockWidget*)m_mainWindow->findChild<QDockWidget*>("controlsDock");

	m_nativeStartStopStreamingButton = (QPushButton*)controlsDock->findChild<QPushButton*>("streamButton");

	if (m_nativeStartStopStreamingButton) {
		m_nativeStartStopStreamingButton->setVisible(false);
	}

	QVBoxLayout* buttonsVLayout = (QVBoxLayout*)controlsDock->findChild<QVBoxLayout*>("buttonsVLayout");

	if (buttonsVLayout) {
		m_startStopStreamingButton = new QPushButton();
		m_startStopStreamingButton->setFixedHeight(28);
		buttonsVLayout->insertWidget(0, m_startStopStreamingButton);

		SetStreamingInitialState();

		connect(m_startStopStreamingButton, SIGNAL(clicked()),
			this, SLOT(OnStartStopStreamingButtonClicked()));

		InitHotkeys();

	}

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);
}

StreamElementsNativeOBSControlsManager::~StreamElementsNativeOBSControlsManager()
{
	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	if (m_startStopStreamingButton) {
		ShutdownHotkeys();

		// OBS Main window owns the button and might have destroyed it by now
		//
		// m_startStopStreamingButton->setVisible(false);
		// m_startStopStreamingButton->deleteLater();

		m_startStopStreamingButton = nullptr;
	}

	if (m_nativeStartStopStreamingButton) {
		// OBS Main window owns the button and might have destroyed it by now
		//
		// m_nativeStartStopStreamingButton->setVisible(true);

		m_nativeStartStopStreamingButton = nullptr;
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingInitialState()
{
	if (!m_startStopStreamingButton) return;

	if (obs_frontend_streaming_active()) {
		SetStreamingActiveState();
	}
	else {
		SetStreamingStoppedState();
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingActiveState()
{
	if (!m_startStopStreamingButton) return;

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StopStreaming"));
		m_startStopStreamingButton->setEnabled(true);

		SetStreamingStyle(true);
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingStoppedState()
{
	if (!m_startStopStreamingButton) return;

	StopTimeoutTracker();

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StartStreaming"));
		m_startStopStreamingButton->setEnabled(true);

		SetStreamingStyle(false);
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingTransitionState()
{
	if (!m_startStopStreamingButton) return;

	if (obs_frontend_streaming_active()) {
		SetStreamingTransitionStoppingState();
	}
	else {
		SetStreamingTransitionStartingState();
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingTransitionStartingState()
{
	if (!m_startStopStreamingButton) return;

	StopTimeoutTracker();

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StartStreaming.InProgress"));
		m_startStopStreamingButton->setEnabled(false);

		SetStreamingStyle(true);
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingRequestedState()
{
	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StartStreaming.RequestInProgress"));
		m_startStopStreamingButton->setEnabled(false);

		SetStreamingStyle(true);

		StartTimeoutTracker();
	});
}

void StreamElementsNativeOBSControlsManager::SetStreamingTransitionStoppingState()
{
	if (!m_startStopStreamingButton) return;

	StopTimeoutTracker();

	QtExecSync([&] {
		m_startStopStreamingButton->setText(obs_module_text("StreamElements.Action.StopStreaming.InProgress"));
		m_startStopStreamingButton->setEnabled(false);

		SetStreamingStyle(false);
	});
}

void StreamElementsNativeOBSControlsManager::OnStartStopStreamingButtonClicked()
{
	if (obs_frontend_streaming_active()) {
		blog(LOG_INFO, "obs-browser: streaming stop requested by UI control");

		obs_frontend_streaming_stop();
	}
	else {
		blog(LOG_INFO, "obs-browser: streaming start requested by UI control");

		BeginStartStreaming();
	}
}

void StreamElementsNativeOBSControlsManager::handle_obs_frontend_event(enum obs_frontend_event event, void* data)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		self->SetStreamingTransitionStartingState();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		self->SetStreamingActiveState();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		self->SetStreamingTransitionStoppingState();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		self->SetStreamingStoppedState();
		break;
	}
}

void StreamElementsNativeOBSControlsManager::SetStreamingStyle(bool streaming)
{
	if (streaming) {
		m_startStopStreamingButton->setStyleSheet(QString(
			"QPushButton { background-color: #823929; color: #ffffff; font-weight:600; } "
			"QPushButton:hover { background-color: #dc6046; color: #000000; } "
			"QPushButton:pressed { background-color: #e68d7a; color: #000000; } "
			"QPushButton:disabled { background-color: #555555; color: #000000; } "
		));
	}
	else {
		m_startStopStreamingButton->setStyleSheet(QString(
			"QPushButton { background-color: #29826a; color: #ffffff; font-weight:600; } "
			"QPushButton:hover { background-color: #46dcb3; color: #000000; } "
			"QPushButton:pressed { background-color: #7ae6c8; color: #000000; } "
			"QPushButton:disabled { background-color: #555555; color: #000000; } "
		));
	}
}

static const char* HOTKEY_NAME_START_STREAMING = "OBSBasic.StartStreaming";

bool StreamElementsNativeOBSControlsManager::hotkey_enum_callback(void* data, obs_hotkey_id id, obs_hotkey_t* key)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	if (obs_hotkey_get_registerer_type(key) == OBS_HOTKEY_REGISTERER_FRONTEND) {
		if (strcmp(obs_hotkey_get_name(key), HOTKEY_NAME_START_STREAMING) == 0) {
			self->m_startStopStreamingHotkeyId = id;

			return false; // stop enumerating
		}
	}

	return true; // continue enumerating
}

void StreamElementsNativeOBSControlsManager::hotkey_routing_func(void* data, obs_hotkey_id id, bool pressed)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	if (id == self->m_startStopStreamingHotkeyId) {
		if (pressed &&
			!obs_frontend_streaming_active() &&
			self->m_startStopStreamingButton->isEnabled()) {
			blog(LOG_INFO, "obs-browser: streaming start requested by hotkey");

			self->BeginStartStreaming();
		}
	}
	else {
		QtPostTask(
			[id, pressed]() {
				obs_hotkey_trigger_routed_callback(id, pressed);
			});
	}
}

void StreamElementsNativeOBSControlsManager::hotkey_change_handler(void* data, calldata_t* param)
{
	StreamElementsNativeOBSControlsManager* self = (StreamElementsNativeOBSControlsManager*)data;

	auto key = static_cast<obs_hotkey_t*>(calldata_ptr(param, "key"));

	if (obs_hotkey_get_registerer_type(key) == OBS_HOTKEY_REGISTERER_FRONTEND) {
		if (strcmp(obs_hotkey_get_name(key), HOTKEY_NAME_START_STREAMING) == 0) {
			self->m_startStopStreamingHotkeyId = obs_hotkey_get_id(key);
		}
	}
}

void StreamElementsNativeOBSControlsManager::InitHotkeys()
{
	obs_enum_hotkeys(hotkey_enum_callback, this);

	signal_handler_connect(
		obs_get_signal_handler(),
		"hotkey_register",
		hotkey_change_handler,
		this
	);

	obs_hotkey_set_callback_routing_func(hotkey_routing_func, this);
	obs_hotkey_enable_callback_rerouting(true);
}

void StreamElementsNativeOBSControlsManager::ShutdownHotkeys()
{
	signal_handler_disconnect(
		obs_get_signal_handler(),
		"hotkey_register",
		hotkey_change_handler,
		this
	);
}

void StreamElementsNativeOBSControlsManager::BeginStartStreaming()
{
	switch (m_start_streaming_mode) {
	case start:
		obs_frontend_streaming_start();
		break;

	case request:
		SetStreamingRequestedState();

		StreamElementsCefClient::DispatchJSEvent("hostStreamingStartRequested", "null");
		break;
	}
}

bool StreamElementsNativeOBSControlsManager::DeserializeStartStreamingUIHandlerProperties(CefRefPtr<CefValue> input)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d->HasKey("autoStart") && d->GetType("autoStart") == VTYPE_BOOL) {
		if (d->GetBool("autoStart")) {
			SetStartStreamingMode(start);
		}
		else {
			SetStartStreamingMode(request);
		}
	}

	if (d->HasKey("requestAcknowledgeTimeoutSeconds") && d->GetType("requestAcknowledgeTimeoutSeconds") == VTYPE_INT) {
		int v = d->GetInt("requestAcknowledgeTimeoutSeconds");

		if (v > 0) {
			SetStartStreamingAckTimeoutSeconds((int64_t)v);
		}
	}

	return true;
}

void StreamElementsNativeOBSControlsManager::StartTimeoutTracker()
{
	std::lock_guard<std::recursive_mutex> guard(m_timeoutTimerMutex);

	StopTimeoutTracker();

	m_timeoutTimer = new QTimer();
	m_timeoutTimer->moveToThread(qApp->thread());
	m_timeoutTimer->setSingleShot(true);

	QObject::connect(m_timeoutTimer, &QTimer::timeout, [this]() {
		std::lock_guard<std::recursive_mutex> guard(m_timeoutTimerMutex);

		m_timeoutTimer->deleteLater();
		m_timeoutTimer = nullptr;

		QMessageBox::warning(
			m_mainWindow,
			QString(obs_module_text("StreamElements.Action.StartStreaming.Timeout.Title")),
			QString(obs_module_text("StreamElements.Action.StartStreaming.Timeout.Text")));

		SetStreamingInitialState();

		obs_frontend_streaming_start();
	});

	QMetaObject::invokeMethod(m_timeoutTimer, "start", Qt::QueuedConnection, Q_ARG(int, m_startStreamingRequestAcknowledgeTimeoutSeconds * 1000));
}

void StreamElementsNativeOBSControlsManager::StopTimeoutTracker()
{
	std::lock_guard<std::recursive_mutex> guard(m_timeoutTimerMutex);

	if (m_timeoutTimer) {
		if (QThread::currentThread() == qApp->thread()) {
			m_timeoutTimer->stop();
		}
		else {
			QMetaObject::invokeMethod(m_timeoutTimer, "stop", Qt::BlockingQueuedConnection);
		}

		m_timeoutTimer->deleteLater();
		m_timeoutTimer = nullptr;
	}
}

void StreamElementsNativeOBSControlsManager::AdviseRequestStartStreamingAccepted()
{
	StopTimeoutTracker();
}

void StreamElementsNativeOBSControlsManager::AdviseRequestStartStreamingRejected()
{
	StopTimeoutTracker();

	SetStreamingInitialState();
}

#include "StreamElementsApiMessageHandler.hpp"

#include "../cef-headers.hpp"

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include "Version.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsMessageBus.hpp"

#include <QDesktopServices>
#include <QUrl>

/* Incoming messages from renderer process */
const char* MSG_ON_CONTEXT_CREATED = "CefRenderProcessHandler::OnContextCreated";
const char* MSG_INCOMING_API_CALL = "StreamElementsApiMessageHandler::OnIncomingApiCall";

/* Outgoing messages to renderer process */
const char* MSG_BIND_JAVASCRIPT_FUNCTIONS = "CefRenderProcessHandler::BindJavaScriptFunctions";
const char* MSG_BIND_JAVASCRIPT_PROPS = "CefRenderProcessHandler::BindJavaScriptProperties";

bool StreamElementsApiMessageHandler::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId /*source_process*/,
	CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();

	if (name == MSG_ON_CONTEXT_CREATED) {
		RegisterIncomingApiCallHandlersInternal(browser);
		RegisterApiPropsInternal(browser);
		DispatchHostReadyEventInternal(browser);

		return true;
	}
	else if (name == MSG_INCOMING_API_CALL) {
		CefRefPtr<CefValue> result = CefValue::Create();
		result->SetBool(false);

		CefRefPtr<CefListValue> args = message->GetArgumentList();

		const int headerSize = args->GetInt(0);
		std::string id = args->GetString(2).ToString();

		id = id.substr(id.find_last_of('.') + 1); // window.host.XXX -> XXX

		if (m_apiCallHandlers.count(id)) {
			CefRefPtr<CefListValue> callArgs = CefListValue::Create();

			for (int i = headerSize; i < args->GetSize() - 1; ++i) {
				CefRefPtr<CefValue> parsedValue =
					CefParseJSON(
						args->GetString(i),
						JSON_PARSER_ALLOW_TRAILING_COMMAS);

				callArgs->SetValue(
					callArgs->GetSize(),
					parsedValue);
			}

			struct local_context {
				StreamElementsApiMessageHandler* self;
				std::string id;
				CefRefPtr<CefBrowser> browser;
				CefRefPtr<CefProcessMessage> message;
				CefRefPtr<CefListValue> callArgs;
				CefRefPtr<CefValue> result;
				void (*complete)(void*);
				int cef_app_callback_id;
			};

			local_context* context = new local_context();

			context->self = this;
			context->id = id;
			context->browser = browser;
			context->message = message;
			context->callArgs = callArgs;
			context->result = result;
			context->cef_app_callback_id = message->GetArgumentList()->GetInt(message->GetArgumentList()->GetSize() - 1);
			context->complete = [] (void* data) {
				local_context* context = (local_context*)data;

				blog(LOG_INFO, "obs-browser: API: completed call to '%s', callback id %d", context->id.c_str(), context->cef_app_callback_id);

				if (context->cef_app_callback_id != -1) {
					// Invoke result callback
					CefRefPtr<CefProcessMessage> msg =
						CefProcessMessage::Create("executeCallback");

					CefRefPtr<CefListValue> callbackArgs = msg->GetArgumentList();
					callbackArgs->SetInt(0, context->cef_app_callback_id);
					callbackArgs->SetString(1, CefWriteJSON(context->result, JSON_WRITER_DEFAULT));

					context->browser->SendProcessMessage(PID_RENDERER, msg);
				}
			};

			{
				CefRefPtr<CefValue> callArgsValue = CefValue::Create();
				callArgsValue->SetList(context->callArgs);
				blog(LOG_INFO, "obs-browser: API: posting call to '%s', callback id %d, args: %s", context->id.c_str(), context->cef_app_callback_id, CefWriteJSON(callArgsValue, JSON_WRITER_DEFAULT).ToString().c_str());
			}

			QtPostTask([](void* data)
			{
				local_context* context = (local_context*)data;

				blog(LOG_INFO, "obs-browser: API: performing call to '%s', callback id %d", context->id.c_str(), context->cef_app_callback_id);

				context->self->m_apiCallHandlers[context->id](
					context->self,
					context->message,
					context->callArgs,
					context->result,
					context->browser,
					context->complete,
					context);

				delete context;
			}, context);
		}

		return true;
	}

	return false;
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlersInternal(CefRefPtr<CefBrowser> browser)
{
	RegisterIncomingApiCallHandlers();

	// Context created, request creation of window.host object
	// with API methods
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

	for (auto apiCallHandler : m_apiCallHandlers) {
		CefRefPtr<CefValue> val = CefValue::Create();

		CefRefPtr<CefDictionaryValue> function = CefDictionaryValue::Create();

		function->SetString("message", MSG_INCOMING_API_CALL);

		val->SetDictionary(function);

		rootDictionary->SetValue(apiCallHandler.first, val);
	}

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	// Send request to renderer process
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(MSG_BIND_JAVASCRIPT_FUNCTIONS);
	msg->GetArgumentList()->SetString(0, "host");
	msg->GetArgumentList()->SetString(1, jsonString);
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::RegisterApiPropsInternal(CefRefPtr<CefBrowser> browser)
{
	// Context created, request creation of window.host object
	// with API methods
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

	rootDictionary->SetBool("hostReady", true);
	rootDictionary->SetInt("apiMajorVersion", HOST_API_VERSION_MAJOR);
	rootDictionary->SetInt("apiMinorVersion", HOST_API_VERSION_MINOR);

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	// Send request to renderer process
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(MSG_BIND_JAVASCRIPT_PROPS);
	msg->GetArgumentList()->SetString(0, "host");
	msg->GetArgumentList()->SetString(1, jsonString);
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::DispatchHostReadyEventInternal(CefRefPtr<CefBrowser> browser)
{
	DispatchEventInternal(browser, "hostReady", "null");
}

void StreamElementsApiMessageHandler::DispatchEventInternal(CefRefPtr<CefBrowser> browser, std::string event, std::string eventArgsJson)
{
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, eventArgsJson);
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandler(std::string id, incoming_call_handler_t handler)
{
	m_apiCallHandlers[id] = handler;
}

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name) RegisterIncomingApiCallHandler(name, [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result, CefRefPtr<CefBrowser> browser, void (*complete_callback)(void*), void* complete_context) { std::lock_guard<std::recursive_mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END() complete_callback(complete_context); });

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	//RegisterIncomingApiCallHandler("getWidgets", [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result) {
	//	result->SetBool(true);
	//});

	API_HANDLER_BEGIN("getStartupFlags");
	{
		result->SetInt(StreamElementsConfig::GetInstance()->GetStartupFlags());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStartupFlags");
	{
		if (args->GetSize()) {
			StreamElementsConfig::GetInstance()->SetStartupFlags(args->GetValue(0)->GetInt());

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("deleteAllCookies");
	{
		StreamElementsGlobalStateManager::GetInstance()->DeleteCookies();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openDefaultBrowser");
	{
		if (args->GetSize()) {
			CefString url = args->GetValue(0)->GetString();

			QUrl navigate_url = QUrl(url.ToString().c_str(), QUrl::TolerantMode);
			QDesktopServices::openUrl(navigate_url);

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showNotificationBar");
	{
		if (args->GetSize()) {
			CefRefPtr<CefValue> barInfo = args->GetValue(0);

			StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->DeserializeNotificationBar(barInfo);

			StreamElementsGlobalStateManager::GetInstance()->PersistState();

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideNotificationBar");
	{
		StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->HideNotificationBar();

		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showCentralWidget");
	{
		if (args->GetSize()) {
			CefRefPtr<CefDictionaryValue> rootDictionary = args->GetValue(0)->GetDictionary();

			if (rootDictionary.get() && rootDictionary->HasKey("url")) {
				// Remove all central widgets
				while (StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->DestroyCurrentCentralBrowserWidget())
				{
				}

				std::string executeJavaScriptCodeOnLoad;

				if (rootDictionary->HasKey("executeJavaScriptCodeOnLoad")) {
					executeJavaScriptCodeOnLoad = rootDictionary->GetString("executeJavaScriptCodeOnLoad").ToString();
				}

				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->PushCentralBrowserWidget(
					rootDictionary->GetString("url").ToString().c_str(),
					executeJavaScriptCodeOnLoad.c_str());

				StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
				StreamElementsGlobalStateManager::GetInstance()->PersistState();

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideCentralWidget");
	{
		while (StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->DestroyCurrentCentralBrowserWidget())
		{
		}

		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addDockingWidget");
	{
		if (args->GetSize()) {
			CefRefPtr<CefValue> widgetInfo = args->GetValue(0);

			std::string id =
				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->AddDockBrowserWidget(widgetInfo);

			QDockWidget* dock =
				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->GetDockWidget(id.c_str());

			if (dock) {
				StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
				StreamElementsGlobalStateManager::GetInstance()->PersistState();

				result->SetString(id);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeDockingWidgetsByIds");
	{
		if (args->GetSize()) {
			CefRefPtr<CefListValue> list = args->GetList(0);

			if (list.get()) {
				for (int i = 0; i < list->GetSize(); ++i) {
					CefString id = list->GetString(i);

					StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->RemoveDockWidget(id.ToString().c_str());
				}

				StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
				StreamElementsGlobalStateManager::GetInstance()->PersistState();

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllDockingWidgets");
	{
		StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->SerializeDockingWidgets(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("toggleDockingWidgetFloatingById")
		if (args->GetSize() && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->ToggleWidgetFloatingStateById(
					args->GetString(0).ToString().c_str()));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
				StreamElementsGlobalStateManager::GetInstance()->PersistState();
			}
		}
	API_HANDLER_END()

	API_HANDLER_BEGIN("setDockingWidgetDimensionsById")
		if (args->GetSize() >= 2 &&
		    args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_DICTIONARY) {
			int width = -1;
			int height = -1;

			CefRefPtr<CefDictionaryValue> d = args->GetDictionary(1);

			if (d->HasKey("width") && d->GetType("width") == VTYPE_INT) {
				width = d->GetInt("width");
			}

			if (d->HasKey("height") && d->GetType("height") == VTYPE_INT) {
				height = d->GetInt("height");
			}

			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->SetWidgetDimensionsById(
					args->GetString(0).ToString().c_str(),
					width,
					height));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
				StreamElementsGlobalStateManager::GetInstance()->PersistState();
			}
		}
	API_HANDLER_END()

	API_HANDLER_BEGIN("setDockingWidgetPositionById")
		if (args->GetSize() >= 2 &&
			args->GetType(0) == VTYPE_STRING &&
			args->GetType(1) == VTYPE_DICTIONARY) {
			int left = -1;
			int top = -1;

			CefRefPtr<CefDictionaryValue> d = args->GetDictionary(1);

			if (d->HasKey("left") && d->GetType("left") == VTYPE_INT) {
				left = d->GetInt("left");
			}

			if (d->HasKey("top") && d->GetType("top") == VTYPE_INT) {
				top = d->GetInt("top");
			}

			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->SetWidgetPositionById(
					args->GetString(0).ToString().c_str(),
					left,
					top));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
				StreamElementsGlobalStateManager::GetInstance()->PersistState();
			}
		}
	API_HANDLER_END()

	API_HANDLER_BEGIN("beginOnBoarding");
		StreamElementsGlobalStateManager::GetInstance()->Reset();

		result->SetBool(true);
	API_HANDLER_END();

	API_HANDLER_BEGIN("completeOnBoarding");
		while (StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->DestroyCurrentCentralBrowserWidget())
		{ }

		StreamElementsConfig::GetInstance()->SetStartupFlags(
			StreamElementsConfig::GetInstance()->GetStartupFlags() &
			~StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);

		//
		// Once on-boarding is complete, we assume our state is signed-in.
		//
		// This is not enough. There are edge cases where the user is signed-in
		// but not yet has completed on-boarding.
		//
		// For those cases, we provide the adviseSignedIn() API call.
		//
		StreamElementsConfig::GetInstance()->SetStartupFlags(
			StreamElementsConfig::GetInstance()->GetStartupFlags() |
			StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	API_HANDLER_END();

	API_HANDLER_BEGIN("showStatusBarTemporaryMessage");
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->DeserializeStatusBarTemporaryMessage(args->GetValue(0)));
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestBegin");
		if (args->GetSize() >= 2) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetBandwidthTestManager()->BeginBandwidthTest(
					args->GetValue(0),
					args->GetValue(1),
					browser));
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestEnd");
		CefRefPtr<CefValue> options;

		if (args->GetSize()) {
			options = args->GetValue(0);
		}
		result->SetDictionary(
			StreamElementsGlobalStateManager::GetInstance()->GetBandwidthTestManager()->EndBandwidthTest(options));
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestGetStatus");
		result->SetDictionary(
			StreamElementsGlobalStateManager::GetInstance()->GetBandwidthTestManager()->GetBandwidthTestStatus());
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableEncoders");
		StreamElementsGlobalStateManager::GetInstance()->GetOutputSettingsManager()->GetAvailableEncoders(result, nullptr);
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableVideoEncoders");
		obs_encoder_type type = OBS_ENCODER_VIDEO;

		StreamElementsGlobalStateManager::GetInstance()->GetOutputSettingsManager()->GetAvailableEncoders(result, &type);
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableAudioEncoders");
		obs_encoder_type type = OBS_ENCODER_AUDIO;

		StreamElementsGlobalStateManager::GetInstance()->GetOutputSettingsManager()->GetAvailableEncoders(result, &type);
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStreamingSettings");
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetOutputSettingsManager()->SetStreamingSettings(args->GetValue(0)));
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setEncodingSettings");
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetOutputSettingsManager()->SetEncodingSettings(args->GetValue(0)));
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentContainerProperties");
		CefRefPtr<StreamElementsCefClient> client =
			static_cast<StreamElementsCefClient*>(browser->GetHost()->GetClient().get());

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		result->SetDictionary(d);

		d->SetString("id", client->GetContainerId());
		d->SetString("area", client->GetLocationArea());
		d->SetString("url", browser->GetMainFrame()->GetURL().ToString());
		d->SetString("theme", GetCurrentThemeName());
	API_HANDLER_END();

	API_HANDLER_BEGIN("openPopupWindow");
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->DeserializePopupWindow(args->GetValue(0)));
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addBackgroundWorker");
		if (args->GetSize()) {
			result->SetString(
				StreamElementsGlobalStateManager::GetInstance()->GetWorkerManager()->DeserializeOne(args->GetValue(0)));

			StreamElementsGlobalStateManager::GetInstance()->PersistState();
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllBackgroundWorkers");
		StreamElementsGlobalStateManager::GetInstance()->GetWorkerManager()->Serialize(result);
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeBackgroundWorkersByIds");
		if (args->GetSize()) {
			CefRefPtr<CefListValue> list = args->GetList(0);

			if (list.get()) {
				for (int i = 0; i < list->GetSize(); ++i) {
					CefString id = list->GetString(i);

					StreamElementsGlobalStateManager::GetInstance()->GetWorkerManager()->Remove(id);
				}

				StreamElementsGlobalStateManager::GetInstance()->PersistState();

				result->SetBool(true);
			}
		}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showModalDialog")
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()->DeserializeModalDialog(
				args->GetValue(0),
				result);
		}
	API_HANDLER_END()

	API_HANDLER_BEGIN("getSystemCPUUsageTimes")
		SerializeSystemTimes(result);
	API_HANDLER_END()

	API_HANDLER_BEGIN("getSystemMemoryUsage")
		SerializeSystemMemoryUsage(result);
	API_HANDLER_END()

	API_HANDLER_BEGIN("getSystemHardwareProperties")
		SerializeSystemHardwareProperties(result);
	API_HANDLER_END()

	API_HANDLER_BEGIN("getAvailableInputSourceTypes")
		SerializeAvailableInputSourceTypes(result);
	API_HANDLER_END()

	API_HANDLER_BEGIN("getAllHotkeyBindings")
		StreamElementsGlobalStateManager::GetInstance()->GetHotkeyManager()->SerializeHotkeyBindings(result);
	API_HANDLER_END()

	API_HANDLER_BEGIN("getAllManagedHotkeyBindings")
		StreamElementsGlobalStateManager::GetInstance()->GetHotkeyManager()->SerializeHotkeyBindings(result, true);
	API_HANDLER_END()

	API_HANDLER_BEGIN("addHotkeyBinding")
		if (args->GetSize()) {
			obs_hotkey_id id =
				StreamElementsGlobalStateManager::GetInstance()->GetHotkeyManager()->DeserializeSingleHotkeyBinding(
					args->GetValue(0));

			if (id != OBS_INVALID_HOTKEY_ID) {
				result->SetInt((int)id);
			}
			else {
				result->SetNull();
			}
		}
		else result->SetNull();
	API_HANDLER_END()

	API_HANDLER_BEGIN("removeHotkeyBindingById")
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->GetHotkeyManager()->RemoveHotkeyBindingById(
					args->GetInt(0)));
		}
	API_HANDLER_END()

	API_HANDLER_BEGIN("getHostProperties")
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("obsVersionString", obs_get_version_string());
		d->SetString("cefVersionString", GetCefVersionString());
		d->SetString("cefPlatformApiHash", GetCefPlatformApiHash());
		d->SetString("cefUniversalApiHash", GetCefUniversalApiHash());
		d->SetString("hostPluginVersionString", GetStreamElementsPluginVersionString());
		d->SetString("hostApiVersionString", GetStreamElementsApiVersionString());
		d->SetString("hostMachineUniqueId", StreamElementsGlobalStateManager::GetInstance()->GetAnalyticsEventsManager()->identity());
		d->SetString("hostSessionUniqueId", StreamElementsGlobalStateManager::GetInstance()->GetAnalyticsEventsManager()->sessionId());

#ifdef _WIN32
		d->SetString("platform", "windows");
#elif APPLE
		d->SetString("platform", "macos");
#elif LINUX
		d->SetString("platform", "linux");
#else
		d->SetString("platform", "other");
#endif

#ifdef _WIN64
		d->SetString("platformArch", "64bit");
#else
		d->SetString("platformArch", "32bit");
#endif

		result->SetDictionary(d);
	API_HANDLER_END()

	API_HANDLER_BEGIN("adviseSignedIn")
		StreamElementsConfig* config = StreamElementsConfig::GetInstance();

		config->SetStartupFlags(
			config->GetStartupFlags() | StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	API_HANDLER_END()

	API_HANDLER_BEGIN("adviseSignedOut")
		StreamElementsConfig* config = StreamElementsConfig::GetInstance();

		config->SetStartupFlags(
			config->GetStartupFlags() & ~StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	API_HANDLER_END()

	API_HANDLER_BEGIN("broadcastMessage")
		if (args->GetSize()) {
			StreamElementsMessageBus::GetInstance()->BroadcastMessageToBrowsers(
				StreamElementsMessageBus::DEST_ALL,
				StreamElementsMessageBus::SOURCE_WEB,
				browser->GetMainFrame()->GetURL().ToString(),
				args->GetValue(0));

			result->SetBool(true);
		}
	API_HANDLER_END()

	API_HANDLER_BEGIN("crashProgram")
		// Crash
		*((int*)nullptr) = 12345; // exception

		UNUSED_PARAMETER(result);
	API_HANDLER_END()
}

#include "StreamElementsApiMessageHandler.hpp"

#include "../cef-headers.hpp"

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include "Version.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"

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

				// Invoke result callback
				CefRefPtr<CefProcessMessage> msg =
					CefProcessMessage::Create("executeCallback");

				CefRefPtr<CefListValue> callbackArgs = msg->GetArgumentList();
				callbackArgs->SetInt(0, context->cef_app_callback_id);
				callbackArgs->SetString(1, CefWriteJSON(context->result, JSON_WRITER_DEFAULT));

				context->browser->SendProcessMessage(PID_RENDERER, msg);
			};

			QtPostTask([](void* data)
			{
				local_context* context = (local_context*)data;

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
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, "hostReady");
	args->SetString(1, "null");
	browser->SendProcessMessage(PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandler(std::string id, incoming_call_handler_t handler)
{
	m_apiCallHandlers[id] = handler;
}

#define API_HANDLER_BEGIN(name) RegisterIncomingApiCallHandler(name, [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result, CefRefPtr<CefBrowser> browser, void (*complete_callback)(void*), void* complete_context) {
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
		if (!CefCookieManager::GetGlobalManager(NULL)->DeleteCookies(
			CefString(""), // URL
			CefString(""), // Cookie name
			nullptr)) {    // On-complete callback
			blog(LOG_ERROR, "CefCookieManager::GetGlobalManager(NULL)->DeleteCookies() failed.");
		}

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
	API_HANDLER_END();

	API_HANDLER_BEGIN("openPopupWindow");
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()->DeserializePopupWindow(args->GetValue(0)));
		}
	API_HANDLER_END();
}

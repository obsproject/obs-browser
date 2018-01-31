#include "wcui-browser-dialog.hpp"
#include "ui_wcui-browser-dialog.h"
#include "browser-manager.hpp"
#include "browser-obs-bridge-base.hpp"

#include "include/cef_parser.h"		// CefParseJSON, CefWriteJSON

#include "fmt/format.h"

#include "obs.h"
#include "obs-encoder.h"

#include <QWidget>
#include <QFrame>

WCUIBrowserDialog::WCUIBrowserDialog(QWidget* parent, std::string obs_module_path, std::string cache_path) :
	QDialog(parent, Qt::Dialog),
	ui(new Ui::WCUIBrowserDialog),
	m_obs_module_path(obs_module_path),
	m_cache_path(cache_path)
{
	setAttribute(Qt::WA_NativeWindow);

	ui->setupUi(this);

	// Remove help question mark from title bar
	setWindowFlags(windowFlags() &= ~Qt::WindowContextHelpButtonHint);
}

WCUIBrowserDialog::~WCUIBrowserDialog()
{
	delete ui;
}

void WCUIBrowserDialog::ShowModal()
{
	// Get window handle of CEF container frame
	auto frame = findChild<QFrame*>("frame");
	m_window_handle = (cef_window_handle_t)frame->winId();

	// Spawn CEF initialization in new thread.
	//
	// The window handle must be obtained in the QT UI thread and CEF initialization must be performed in a
	// separate thread, otherwise a dead lock occurs and everything just hangs.
	//
	//pthread_t thread;
	//pthread_create(&thread, nullptr, InitBrowserThreadEntryPoint, this);

	m_task_queue.Enqueue([&]
	{
		InitBrowser();
	});

	// Start modal dialog
	exec();
}

// CEF initialization thread entry point.
//
void* WCUIBrowserDialog::InitBrowserThreadEntryPoint(void* arg)
{
	WCUIBrowserDialog* self = (WCUIBrowserDialog*)arg;

	self->InitBrowser();

	return nullptr;
}

// Initialize CEF.
//
// This function is called in a separate thread by InitBrowserThreadEntryPoint() above.
//
// DO NOT call this function from the QT UI thread: it will lead to a dead lock.
//
void WCUIBrowserDialog::InitBrowser()
{
	std::string absoluteHtmlFilePath;

	// Get initial UI HTML file full path
	std::string parentPath(
		m_obs_module_path.substr(0, m_obs_module_path.find_last_of('/') + 1));

	// Launcher local HTML page path
	std::string htmlPartialPath = parentPath + "/obs-browser-wcui-browser-dialog.html";

#ifdef WIN32
	char htmlFullPath[MAX_PATH + 1];
	::GetFullPathNameA(htmlPartialPath.c_str(), MAX_PATH, htmlFullPath, NULL);

	absoluteHtmlFilePath = htmlFullPath;
#else
	char* htmlFullPath = realpath(htmlPartialPath.c_str(), NULL);

	absoluteHtmlFilePath = htmlFullPath;

	free(htmlFullPath);
#endif

	// BrowserManager installs a custom http scheme URL handler to access local files.
	//
	// We don't need this on Windows, perhaps MacOS / UNIX users need this?
	//
	CefString url = "http://absolute/" + absoluteHtmlFilePath;

	if (m_browser_handle == BROWSER_HANDLE_NONE)
	{
		// Browser has not been created yet

		// Client area rectangle
		RECT clientRect;

		clientRect.left = 0;
		clientRect.top = 0;
		clientRect.right = width();
		clientRect.bottom = height();

		// CefClient
		CefRefPtr<BrowserClient> client(new BrowserClient(NULL, NULL, new BrowserOBSBridgeBase(), this));
		
		// Window info
		CefWindowInfo window_info;

		CefBrowserSettings settings;

		settings.Reset();

		// Don't allow JavaScript to close the browser window
		settings.javascript_close_windows = STATE_DISABLED;

		window_info.SetAsChild(m_window_handle, clientRect);

		m_browser_handle = BrowserManager::Instance()->CreateBrowser(window_info, client, url, settings, nullptr);
	}
	else
	{
		// Reset URL for browser which has already been created

		BrowserManager::Instance()->LoadURL(m_browser_handle, url);
	}
}

void WCUIBrowserDialog::OnProcessMessageReceivedSendExecuteCallbackMessage(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message,
	CefRefPtr<CefValue> callback_arg)
{
	// Get caller callback ID
	int callbackID = message->GetArgumentList()->GetInt(0);

	// Convert callback argument to JSON
	CefString jsonString =
		CefWriteJSON(callback_arg, JSON_WRITER_DEFAULT);

	// Create "executeCallback" message for the renderer process
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("executeCallback");

	// Get callback arguments collection and set callback arguments
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetInt(0, callbackID);	// Callback identifier in renderer process callbackMap

	args->SetString(1, jsonString);	// Callback argument JSON string which will be converted back to CefV8Value in
					// the renderer process

	// Send message to the renderer process
	browser->SendProcessMessage(source_process, msg); // source_process = PID_RENDERER
}

void WCUIBrowserDialog::OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message,
	obs_encoder_type encoder_type)
{
	// Response root object
	CefRefPtr<CefValue> root = CefValue::Create();

	// Response codec collection (array)
	CefRefPtr<CefListValue> codec_list = CefListValue::Create();

	// Response codec collection is our root object
	root->SetList(codec_list);

	// Iterate over each available codec
	bool continue_iteration = true;
	for (size_t idx = 0; continue_iteration; ++idx)
	{
		// Set by obs_enum_encoder_types() call below
		const char* encoderId = NULL;

		// Get next encoder ID
		continue_iteration = obs_enum_encoder_types(idx, &encoderId);

		// If obs_enum_encoder_types() returned a result
		if (continue_iteration)
		{
			// If encoder is of correct type (AUDIO / VIDEO)
			if (obs_get_encoder_type(encoderId) == encoder_type)
			{
				CefRefPtr<CefDictionaryValue> codec = CefDictionaryValue::Create();

				// Set codec dictionary properties
				codec->SetString("id", encoderId);
				codec->SetString("codec", obs_get_encoder_codec(encoderId));
				codec->SetString("name", obs_encoder_get_display_name(encoderId));

				// Append dictionary to codec list
				codec_list->SetDictionary(
					codec_list->GetSize(),
					codec);
			}
		}
	}

	// Send message to call calling web page JS callback with codec info as an argument
	OnProcessMessageReceivedSendExecuteCallbackMessage(
		browser,
		source_process,
		message,
		root);
}

// Called when a new message is received from a different process. Return true
// if the message was handled or false otherwise. Do not keep a reference to
// or attempt to access the message outside of this callback.
//
/*--cef()--*/
bool WCUIBrowserDialog::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	// Get message name
	CefString name = message->GetName();

	// Get message arguments
	CefRefPtr<CefListValue> args = message->GetArgumentList();

	if (name == "setupEnvironment" && args->GetSize() > 0)
	{
		// window.obsstudio.setupEnvironment(config) JS call

		CefString config_json_string = args->GetValue(0)->GetString();
		CefRefPtr<CefValue> config =
			CefParseJSON(config_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS);

		if (config->GetDictionary() != NULL)
		{
			auto input = config->GetDictionary()->GetValue("input");
			auto output = config->GetDictionary()->GetValue("output");

		}

		// ObsRemoveAllScenes();
		ObsAddScene(fmt::format("New scene {}", os_gettime_ns()).c_str(), true);

		/*
		QMetaObject::invokeMethod(
			this,
			"ObsAddScene",
			Q_ARG(const char*, fmt::format("New scene {}", os_gettime_ns()).c_str()),
			Q_ARG(bool, true));
		*/

		/*
		obs_enum_sources(
			[](void* data, obs_source_t* source) {
				::MessageBoxA(0, obs_source_get_name(source), obs_source_get_name(source), 0);
				return true;
			},
			NULL);*/

		return true;
	}
	else if (name == "videoEncoders")
	{
		// window.obsstudio.videoEncoders(callback) JS call

		OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
			browser,
			source_process,
			message,
			OBS_ENCODER_VIDEO);

		return true;
	}
	else if (name == "audioEncoders")
	{
		// window.obsstudio.audioEncoders(callback) JS call

		OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
			browser,
			source_process,
			message,
			OBS_ENCODER_AUDIO);

		return true;
	}
	else if (name == "videoInputSources")
	{
		// window.obsstudio.videoInputSources(callback) JS call

		// Response root object
		CefRefPtr<CefValue> root = CefValue::Create();

		// Response codec collection (array)
		CefRefPtr<CefListValue> list = CefListValue::Create();

		// Response codec collection is our root object
		root->SetList(list);

		// Iterate over all input sources
		bool continue_iteration = true;
		for (size_t idx = 0; continue_iteration; ++idx)
		{
			// Filled by obs_enum_input_types() call below
			const char* sourceId;

			// Get next input source type, obs_enum_input_types() returns true as long as
			// there is data at the specified index
			continue_iteration = obs_enum_input_types(idx, &sourceId);

			if (continue_iteration)
			{
				// Get source caps
				uint32_t sourceCaps = obs_get_source_output_flags(sourceId);

				// If source has video
				if (sourceCaps & OBS_SOURCE_VIDEO == OBS_SOURCE_VIDEO)
				{
					// Create source response dictionary
					CefRefPtr<CefDictionaryValue> dic = CefDictionaryValue::Create();

					// Set codec dictionary properties
					dic->SetString("id", sourceId);
					dic->SetString("name", obs_source_get_display_name(sourceId));
					dic->SetBool("hasVideo", sourceCaps & OBS_SOURCE_VIDEO == OBS_SOURCE_VIDEO);
					dic->SetBool("hasAudio", sourceCaps & OBS_SOURCE_AUDIO == OBS_SOURCE_AUDIO);

					// Compare sourceId to known video capture devices
					dic->SetBool("isVideoCaptureDevice",
						strcmp(sourceId, "dshow_input") == 0 ||
						strcmp(sourceId, "decklink-input") == 0);

					// Compare sourceId to known game capture source
					dic->SetBool("isGameCaptureDevice",
						strcmp(sourceId, "game_capture") == 0);

					// Compare sourceId to known browser source
					dic->SetBool("isBrowserSource",
						strcmp(sourceId, "browser_source") == 0);

					// Append dictionary to response list
					list->SetDictionary(
						list->GetSize(),
						dic);
				}
			}
		}

		// Send message to call calling web page JS callback with codec info as an argument
		OnProcessMessageReceivedSendExecuteCallbackMessage(
			browser,
			source_process,
			message,
			root);

		return true;
	}

	return false;
}

void WCUIBrowserDialog::ObsRemoveAllScenes()
{
	struct obs_frontend_source_list transitions = {};

	obs_frontend_get_transitions(&transitions);

	struct obs_frontend_source_list scenes = {};

	// For each transition
	for (size_t idx = 0; idx < transitions.sources.num; ++idx)
	{
		// Get the scene (a transition is a source)
		obs_source_t* transition = transitions.sources.array[idx];

		// Remove the transition
		obs_source_remove(transition);
	}

	// Free list of transitions.
	// This also calls obs_release_scene() for each scene in the list.
	obs_frontend_source_list_free(&transitions);



	// Get list of scenes
	obs_frontend_get_scenes(&scenes);

	// For each scene
	for (size_t idx = 0; idx < scenes.sources.num; ++idx)
	{
		// Get the scene (a scene is a source)
		obs_source_t* scene = scenes.sources.array[idx];

		// Remove the scene
		obs_source_remove(scene);
	}

	// Free list of scenes.
	// This also calls obs_release_scene() for each scene in the list.
	obs_frontend_source_list_free(&scenes);
}

void WCUIBrowserDialog::ObsAddScene(const char* name, bool setCurrent)
{
	// Create scene, this will also trigger UI update
	obs_scene_t* scene = obs_scene_create(name);

	if (setCurrent)
	{
		// If setCurrent requested, set the new scene as current scene
		obs_frontend_set_current_scene(obs_scene_get_source(scene));
	}

	// Release reference to new scene
	obs_scene_release(scene);
}

void WCUIBrowserDialog::ObsAddSource(
	obs_source_t* parentScene,
	const char* sourceId,
	const char* sourceName,
	obs_data_t* sourceSettings,
	obs_data_t* sourceHotkeyData)
{
	bool releaseParentScene = false;

	if (parentScene == NULL)
	{
		parentScene = obs_frontend_get_current_scene();

		releaseParentScene = true;
	}

	obs_source_t* source = obs_source_create(sourceId, sourceName, sourceSettings, sourceHotkeyData);

	if (source != NULL)
	{
		// Does not increment refcount. No obs_scene_release() call is necessary.
		obs_scene_t* scene = obs_scene_from_source(parentScene);

		obs_enter_graphics();
		obs_scene_atomic_update(
			scene,
			[](void *data, obs_scene_t *scene)
			{
				obs_source_t* source = (obs_source_t*)data;

				obs_sceneitem_t* sceneitem = obs_scene_add(scene, source);
				obs_sceneitem_set_visible(sceneitem, true);

				// obs_sceneitem_release??
			},
			source);
		obs_leave_graphics();

		obs_source_release(source);
	}


	if (releaseParentScene)
	{
		obs_source_release(parentScene);
	}
}

void WCUIBrowserDialog::ObsAddSourceBrowser(
	obs_source_t* parentScene,
	const char* name,
	const long long width,
	const long long height,
	const long long fps,
	const char* url,
	const bool shutdownWhenInactive,
	const char* css)
{
	struct args_t
	{
		WCUIBrowserDialog* self;
		char* name;
		obs_data_t* settings;
	};

	args_t* args = new args_t();

	args->self = this;
	args->name = strdup(name);
	args->settings = obs_data_create();

	obs_data_set_bool(args->settings, "is_local_file", false);
	obs_data_set_string(args->settings, "url", url);
	obs_data_set_string(args->settings, "css", css);
	obs_data_set_int(args->settings, "width", 1920);
	obs_data_set_int(args->settings, "height", 1080);
	obs_data_set_int(args->settings, "fps", 25);
	obs_data_set_bool(args->settings, "shutdown", true);

	pthread_t thread;
	pthread_create(
		&thread,
		nullptr,
		[](void* arg)
	{
		args_t* args = (args_t*)arg;

		args->self->ObsAddSource(NULL, "browser_source", args->name, args->settings, NULL);

		obs_data_release(args->settings);

		free(args->name);
		delete args;

		return (void*)NULL;
	},
	args);
}

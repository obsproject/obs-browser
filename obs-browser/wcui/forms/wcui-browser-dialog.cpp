#include <util/config-file.h>

#include "wcui-browser-dialog.hpp"
#include "ui_wcui-browser-dialog.h"
#include "browser-manager.hpp"
#include "browser-obs-bridge-base.hpp"
#include "browser-render-handler.hpp"

#include "include/cef_parser.h"		// CefParseJSON, CefWriteJSON

#include "fmt/format.h"

#include "obs.h"
#include "obs-encoder.h"

#include <QWindow>
#include <QWidget>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

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

	ObsDisableMainWindow();

	// Spawn CEF initialization in new thread.
	//
	// The window handle must be obtained in the QT UI thread and CEF initialization must be performed in a
	// separate thread, otherwise a dead lock occurs and everything just hangs.
	//
	m_task_queue.Enqueue(
		[](void* args)
		{
			WCUIBrowserDialog* self = (WCUIBrowserDialog*)args;

			self->InitBrowser();
		},
		this);

	// Start modal dialog
	exec();

	ObsEnableMainWindow();
}

// Initialize CEF.
//
// This function is called in a separate thread.
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

#ifdef _WIN32
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
		CefRefPtr<BrowserClient> client(new BrowserClient(NULL, NULL, new BrowserOBSBridgeBase(), this, true));
		
		// Window info
		CefWindowInfo window_info;

		CefBrowserSettings settings;

		settings.Reset();

		// Don't allow JavaScript to close the browser window
		settings.javascript_close_windows = STATE_DISABLED;

		// Disable local storage
		settings.local_storage = STATE_DISABLED;

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
		CefRefPtr<CefValue> config_value =
			CefParseJSON(config_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS);

		struct async_args
		{
			WCUIBrowserDialog* dialog;
			c_config_container* config;
		};

		async_args* task_args = new async_args();

		task_args->dialog = this;
		task_args->config = new c_config_container(config_value); // parse configuration input

		m_task_queue.Enqueue(
			[](void* arg)
			{
				async_args* task_args = (async_args*)arg;

				WCUIBrowserDialog* self = task_args->dialog;
				c_config_container* config = task_args->config;

				///////////////////////////////////////////////
				// Setup scenes
				///////////////////////////////////////////////

				// Get current scene count, we'll use it later to determine
				// whether we added any new scenes or not.
				size_t initSceneCount = self->ObsScenesGetCount();

				// Store current scene list to be removed later
				obs_frontend_source_list stored_scenes = self->ObsStoreScenes();

				// Globally prevent OnDraw calls in browser render handlers.
				// This is an extra safety to prevent a nasty race condition in
				// BrowserRenderHandler::OnPaint when removing browser sources.
				//
				// This is augmented by checking whether the browser source has been destroyed
				// before accessing the browser source in BrowserSource::Impl::Listener::OnDraw
				// and setting CefBrowserHost::WasHidden(true) (which SHOULD stop OnPaint
				// events to be sent), before calling CefBrowserHost::CloseWindow()
				//
				// Must be followed by BrowserRenderHandler::SetPreventDraw(false)
				//
				BrowserRenderHandler::SetPreventDraw(true);

				// Suspend saving in frontend
				obs_frontend_save_suspend();

				self->ObsScenesRemoveAllSources(stored_scenes);

				///////////////////////////////////////////////
				// Setup audio sources
				///////////////////////////////////////////////		

				// Unique scene name -> Requested scene name map, will be used
				// later to rename generated unique scene names to scene names
				// requested in the API call
				std::map<std::string, std::string> sceneRenameMap;

				// For each requested scene
				for (size_t scene_idx = 0; scene_idx < config->input.scenes.items.size(); ++scene_idx)
				{
					c_scene_container* scene = &config->input.scenes.items[scene_idx];

					// Make sure scene name is unique. This is necessary since
					// OBS may already contain scene with the same name
					std::string unique_name = self->ObsGetUniqueSourceName(scene->name);

					if (unique_name != scene->name)
					{
						// Unique scene name was generated: put it into
						// the scene rename map to be renamed back after
						// previously existing scenes are removed
						sceneRenameMap[unique_name] = scene->name;
					}

					// Add scene
					self->ObsAddScene(unique_name.c_str(), true);

					// For each requested source in the scene
					for (size_t source_idx = 0; source_idx < scene->sources.items.size(); ++source_idx)
					{
						c_values_container* source = &scene->sources.items[source_idx];

						std::string source_type =
							source->string("type", "");

						std::string source_name =
							self->ObsGetUniqueSourceName(source->string("name", source_type));

						if (source_type == "video_capture")
						{
							self->ObsAddSourceVideoCapture(
								NULL,
								source_name.c_str(),
								source->integer("left", 0),
								source->integer("top", 0),
								source->integer("width", 320),
								source->integer("height", 240));
						}
						else if (source_type == "browser")
						{
							self->ObsAddSourceBrowser(
								NULL,
								source_name.c_str(),
								config->output.video.integer("width", 1920),
								config->output.video.integer("height", 1080),
								config->output.video.integer("framesPerSecond", 25),
								source->string("url", "http://obsproject.com/").c_str(),
								source->boolean("shutdownWhenInactive", true),
								"", // css
								source->boolean("suspendElementsWhenInactive", false));
						}
						else if (source_type == "game")
						{
							self->ObsAddSourceGame(
								NULL,
								source_name.c_str(),
								source->boolean("multiGpuCompatibility", true),
								source->boolean("allowTransparency", false),
								source->boolean("limitFramerate", false),
								source->boolean("captureCursor", false),
								source->boolean("antiCheatHook", true),
								source->boolean("captureOverlays", true));
						}
						else if (source_type == "monitor_capture")
						{
							self->ObsAddSource(
								NULL,
								"monitor_capture",
								source_name.c_str(),
								NULL,
								NULL,
								false);
						}
					}
				}

				// Count scenes again
				size_t endSceneCount = self->ObsScenesGetCount();

				// If scenes previously existed and we added new scenes
				if (initSceneCount > 0 && initSceneCount < endSceneCount)
				{
					// Remove scenes stored before adding new scenes
					self->ObsRemoveStoredScenes(stored_scenes);
				}

				// Release stored (and removed!) scenes
				self->ObsReleaseStoredScenes(stored_scenes);

				// Rename unique scene names back to requested scene names
				for (const auto& sceneRenamePair : sceneRenameMap)
				{
					// Get scene by it's unique name
					obs_source_t* sceneSource =
						obs_get_source_by_name(sceneRenamePair.first.c_str());

					if (sceneSource != NULL)
					{
						// Rename scene back to requested scene name
						obs_source_set_name(sceneSource, sceneRenamePair.second.c_str());

						// Release reference to renamed scene
						obs_source_release(sceneSource);
					}
				}

				///////////////////////////////////////////////
				// Setup output
				///////////////////////////////////////////////		

				config_t* basicConfig = obs_frontend_get_profile_config(); // does not increase refcount

				self->ObsSetProfileOutputConfiguration(
					config->output.stream.string("serverUrl", "").c_str(),
					config->output.stream.string("streamKey", "").c_str(),
					config->output.stream.boolean("useAuth", false),
					config->output.stream.string("username", "").c_str(),
					config->output.stream.string("password", "").c_str(),
					config->output.video.string("encoder", "obs_x264").c_str(),
					config->output.video.integer("bitRate", 2500),
					config->output.video.integer("width", 1920),
					config->output.video.integer("height", 1080),
					config->output.video.integer("framesPerSecond", 25),
					config->output.audio.integer("bitRate", 128),
					config->output.video.integer("keyframeIntervalSeconds", 2),
					config->output.audio.integer("sampleRate", 44100),
					config->output.audio.integer("channels", 2),
					config->output.stream.boolean("reconnectOnFailure", config_get_bool(basicConfig, "Output", "Reconnect")),
					config->output.stream.integer("reconnectRetryDelaySeconds", (int)config_get_uint(basicConfig, "Output", "RetryDelay")),
					config->output.stream.integer("reconnectRetryMaxAttempts", (int)config_get_uint(basicConfig, "Output", "MaxRetries"))
				);

				// If default scene was requested
				if (config->output.defaultScene.size() > 0)
				{
					// Get the default scene
					obs_source_t* sceneSource =
						obs_get_source_by_name(config->output.defaultScene.c_str());

					if (sceneSource != NULL)
					{
						// If matching scene found, set it as current scene
						obs_frontend_set_current_scene(sceneSource);

						// And release reference to the scene
						obs_source_release(sceneSource);
					}
				}

				// Start streaming if autoStart requested
				if (config->output.stream.boolean("autoStart", false))
					obs_frontend_streaming_start();

				// We're done!
				delete config;
				delete task_args;

				// Resume saving in frontend
				obs_frontend_save_resume();

				// Globally allow OnDraw calls in browser render handlers
				BrowserRenderHandler::SetPreventDraw(false);

				// Close modal dialog
				self->CloseModalDialog();
			},
			task_args);

		return true;
	}
	else if (name == "createSceneCollectionCopy")
	{
		// 1st arg is the callback ID, 2nd arg is the requested
		// scene collection name
		CefString requestedSceneCollectionsName =
			args->GetValue(1)->GetString();

		std::string uniqueSceneCollectionName =
			ObsGetUniqueSceneCollectionName(requestedSceneCollectionsName.ToString());

		obs_frontend_add_scene_collection(uniqueSceneCollectionName.c_str());

		// Response root object
		CefRefPtr<CefValue> root = CefValue::Create();

		// Set result value
		root->SetString(CefString(uniqueSceneCollectionName));

		// Send message to call calling web page JS callback with string value
		// as argument indicating actual created scene collection name
		OnProcessMessageReceivedSendExecuteCallbackMessage(
			browser,
			source_process,
			message,
			root);
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
	else if (name == "deleteAllCookies")
	{
		// window.obsstudio.deleteAllCookies(callback) JS call

		// Delete all cookies
		bool result =
			BrowserManager::Instance()->DeleteCookies(
				CefString(""),	// empty value = all URLs
				CefString("")	// empty value = all cookie names
			);

		// Response root object
		CefRefPtr<CefValue> root = CefValue::Create();

		// Set result value
		root->SetBool(result);

		// Send message to call calling web page JS callback with boolean value
		// as argument indicating success / failure
		OnProcessMessageReceivedSendExecuteCallbackMessage(
			browser,
			source_process,
			message,
			root);
	}
	else if (name == "openDefaultBrowser")
	{
		// window.obsstudio.openDefaultBrowser(url) JS call

		CefString url = args->GetValue(0)->GetString();

		QUrl navigate_url = QUrl(url.ToString().c_str(), QUrl::TolerantMode);
		QDesktopServices::openUrl(navigate_url);
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

obs_frontend_source_list& WCUIBrowserDialog::ObsStoreScenes()
{
	struct obs_frontend_source_list scenes = {};

	// Get list of scenes
	obs_frontend_get_scenes(&scenes);

	return scenes;
}

void WCUIBrowserDialog::ObsScenesRemoveAllSources(obs_frontend_source_list& scenes)
{
	// For each scene
	for (size_t idx = 0; idx < scenes.sources.num; ++idx)
	{
		// Get the scene (a scene is a source)
		obs_source_t* source = scenes.sources.array[idx];

		// Get scene handle
		obs_scene_t* scene = obs_scene_from_source(source); // does not increment refcount

		// For each scene item
		obs_scene_enum_items(
			scene,
			[](obs_scene_t* scene, obs_sceneitem_t* sceneitem, void* param)
			{
				obs_source_t* source =
					obs_sceneitem_get_source(sceneitem); // does not increas refcount

				// Remove the scene item
				obs_sceneitem_remove(sceneitem);

				// Continue iteration
				return true;
			},
			NULL);
	}
}

void WCUIBrowserDialog::ObsRemoveStoredScenes(obs_frontend_source_list& scenes)
{
	// For each scene
	for (size_t idx = 0; idx < scenes.sources.num; ++idx)
	{
		// Get the scene (a scene is a source)
		obs_source_t* scene = scenes.sources.array[idx];

		// Remove the scene
		obs_source_remove(scene);
	}
}

void WCUIBrowserDialog::ObsReleaseStoredScenes(obs_frontend_source_list& scenes)
{
	// Free list of scenes.
	// This also calls obs_release_scene() for each scene in the list.
	obs_frontend_source_list_free(&scenes);
}

std::string WCUIBrowserDialog::ObsGetUniqueSceneCollectionName(std::string name)
{
	char** sceneCollectionNames =
		obs_frontend_get_scene_collections();

	std::string result(name);

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique)
	{
		isUnique = true;

		// Look for result in sceneCollectionNames
		for (int index = 0; sceneCollectionNames[index] && isUnique; ++index)
		{
			if (stricmp(sceneCollectionNames[index], result.c_str()) == 0)
			{
				isUnique = false;
			}
		}

		if (!isUnique)
		{
			++sequence;

			result = fmt::format("{} ({})", name.c_str(), sequence);
		}
	}

	bfree(sceneCollectionNames);

	return result;
}

std::string WCUIBrowserDialog::ObsGetUniqueSourceName(std::string name)
{
	std::string result(name);

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique)
	{
		isUnique = true;

		obs_source_t* source = obs_get_source_by_name(result.c_str());
		if (source != NULL)
		{
			obs_source_release(source);

			isUnique = false;
		}

		if (!isUnique)
		{
			++sequence;

			result = fmt::format("{} ({})", name.c_str(), sequence);
		}
	}

	return result;
}

size_t WCUIBrowserDialog::ObsScenesGetCount()
{
	size_t result = 0;

	struct obs_frontend_source_list scenes = {};

	// Get list of scenes
	obs_frontend_get_scenes(&scenes);

	result = scenes.sources.num;

	// Free list of scenes.
	// This also calls obs_release_scene() for each scene in the list.
	obs_frontend_source_list_free(&scenes);

	return result;
}

void WCUIBrowserDialog::ObsAddScene(const char* name, bool setCurrent)
{
	// Create scene, this will also trigger UI update
	obs_scene_t* scene = obs_scene_create(name);
	QApplication::processEvents();

	if (setCurrent)
	{
		// If setCurrent requested, set the new scene as current scene
		obs_frontend_set_current_scene(obs_scene_get_source(scene));
		QApplication::processEvents();
	}

	// Release reference to new scene
	obs_scene_release(scene);
}

void WCUIBrowserDialog::ObsAddSource(
	obs_source_t* parentScene,
	const char* sourceId,
	const char* sourceName,
	obs_data_t* sourceSettings,
	obs_data_t* sourceHotkeyData,
	bool preferExistingSource,
	obs_source_t** output_source,
	obs_sceneitem_t** output_sceneitem)
{
	bool releaseParentScene = false;

	if (parentScene == NULL)
	{
		parentScene = obs_frontend_get_current_scene();

		releaseParentScene = true;
	}

	obs_source_t* source = NULL;

	if (preferExistingSource)
	{
		// Try locating existing source of the same type for reuse
		//
		// This is especially relevant for video capture sources
		//

		struct enum_sources_args {
			const char* id;
			obs_source_t* result;
		};

		enum_sources_args enum_args = {};
		enum_args.id = sourceId;
		enum_args.result = NULL;

		obs_enum_sources(
			[](void* arg, obs_source_t* iterator)
			{
				enum_sources_args* args = (enum_sources_args*)arg;

				const char* id = obs_source_get_id(iterator);

				if (strcmp(id, args->id) == 0)
				{
					args->result = obs_source_get_ref(iterator);

					return false;
				}

				return true;
			},
			&enum_args);

		source = enum_args.result;
	}

	if (source == NULL)
	{
		// Not reusing an existing source, create a new one
		source = obs_source_create(sourceId, sourceName, NULL, sourceHotkeyData);

		QApplication::processEvents();

		obs_source_update(source, sourceSettings);
	}

	if (source != NULL)
	{
		// Does not increment refcount. No obs_scene_release() call is necessary.
		obs_scene_t* scene = obs_scene_from_source(parentScene);

		struct atomic_update_args {
			obs_source_t* source;
			obs_sceneitem_t* sceneitem;
		};

		atomic_update_args args = {};

		args.source = source;
		args.sceneitem = NULL;

		obs_enter_graphics();
		obs_scene_atomic_update(
			scene,
			[](void *data, obs_scene_t *scene)
			{
				atomic_update_args* args = (atomic_update_args*)data;

				args->sceneitem = obs_scene_add(scene, args->source);
				obs_sceneitem_set_visible(args->sceneitem, true);

				// obs_sceneitem_release??
			},
			&args);
		obs_leave_graphics();

		if (output_sceneitem != NULL)
		{
			obs_sceneitem_addref(args.sceneitem);

			*output_sceneitem = args.sceneitem;
		}

		if (output_source != NULL)
			*output_source = source;
		else
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
	const char* css,
	const bool suspendElementsWhenInactive)
{
	obs_data_t* settings = obs_data_create();

	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "css", css);
	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_int(settings, "fps", fps);
	obs_data_set_bool(settings, "shutdown", shutdownWhenInactive);
	obs_data_set_bool(settings, "suspend_elements_when_inactive", suspendElementsWhenInactive);

	ObsAddSource(parentScene, "browser_source", name, settings, NULL, false);

	obs_data_release(settings);
}

void WCUIBrowserDialog::ObsAddSourceVideoCapture(
	obs_source_t* parentScene,
	const char* name,
	const int left,
	const int top,
	const int maxWidth,
	const int maxHeight)
{
#ifdef _WIN32
	const char* VIDEO_DEVICE_ID = "video_device_id";

	const char* sourceId = "dshow_input";
#else // APPLE / LINUX
	const char* VIDEO_DEVICE_ID = "device";

	const char* sourceId = "av_capture_input";
#endif

	// Get default settings
	obs_data_t* settings = obs_get_source_defaults(sourceId);

	if (settings != NULL)
	{
		// Get source props
		obs_properties_t* props = obs_get_source_properties(sourceId);

		if (props != NULL)
		{
			// Set first available video_device_id value
			obs_property_t* prop_video_device_id = obs_properties_get(props, VIDEO_DEVICE_ID);

			size_t count_video_device_id = obs_property_list_item_count(prop_video_device_id);
			if (count_video_device_id > 0)
			{
#ifdef _WIN32
				const size_t idx = 0;
#else
				const size_t idx = count_video_device_id - 1;
#endif
				obs_data_set_string(
					settings,
					VIDEO_DEVICE_ID,
					obs_property_list_item_string(prop_video_device_id, idx));
			}

			// Will be filled by ObsAddSource
			obs_source_t* source = NULL;
			obs_sceneitem_t* sceneitem = NULL;

			// Create source with default video_device_id
			ObsAddSource(parentScene, sourceId, name, settings, NULL, true, &source, &sceneitem);

			// Wait for dimensions
			for (int i = 0; i < 50 && obs_source_get_width(source) == 0; ++i)
				os_sleep_ms(100);

			size_t src_width = obs_source_get_width(source);
			size_t src_height = obs_source_get_height(source);

			vec2 pos = {};
			pos.x = left;
			pos.y = top;

			vec2 scale = {};
			scale.x = 1;
			scale.y = 1;

			if (maxWidth > 0 && src_width > 0 && maxWidth != src_width)
				scale.x = (float)maxWidth / (float)src_width;

			if (maxHeight > 0 && src_height > 0 && maxHeight != src_height)
				scale.y = (float)maxHeight / (float)src_height;

			if (scale.x != scale.y)
			{
				// Correct aspect ratio
				scale.x = min(scale.x, scale.y);
				scale.y = min(scale.x, scale.y);

				int r_width = (int)((float)src_width * scale.x);
				int r_height = (int)((float)src_height * scale.y);

				pos.x = pos.x + (maxWidth / 2) - (r_width / 2);
				pos.y = pos.y + (maxHeight / 2) - (r_height / 2);
			}

			obs_sceneitem_set_pos(sceneitem, &pos);
			obs_sceneitem_set_scale(sceneitem, &scale);

			// Release references
			obs_sceneitem_release(sceneitem);
			obs_source_release(source);

			// Destroy source props
			obs_properties_destroy(props);
		}
	}

	obs_data_release(settings);
}

void WCUIBrowserDialog::ObsAddSourceGame(
	obs_source_t* parentScene,
	const char* name,
	bool multiGpuCompatibility,
	bool allowTransparency,
	bool limitFramerate,
	bool captureCursor,
	bool antiCheatHook,
	bool captureOverlays)
{
	const char* sourceId = "game_capture";

	// Get default settings
	obs_data_t* settings = obs_get_source_defaults(sourceId);

	// Override default settings
	obs_data_set_bool(settings, "sli_compatibility", multiGpuCompatibility);
	obs_data_set_bool(settings, "allow_transparency", allowTransparency);
	obs_data_set_bool(settings, "limit_framerate", limitFramerate);
	obs_data_set_bool(settings, "capture_cursor", captureCursor);
	obs_data_set_bool(settings, "anti_cheat_hook", antiCheatHook);
	obs_data_set_bool(settings, "capture_overlays", captureOverlays);

	// Add game capture source
	ObsAddSource(parentScene, sourceId, name, settings, NULL, false);

	// Release ref
	obs_data_release(settings);
}

void WCUIBrowserDialog::ObsSetProfileOutputConfiguration(
	const char* streamingServiceServerUrl,
	const char* streamingServiceStreamKey,
	const bool streamingServiceUseAuth,
	const char* streamingServiceAuthUsername,
	const char* streamingServiceAuthPassword,
	const char* videoEncoderId,
	uint videoBitrate,
	uint videoWidth,
	uint videoHeight,
	uint videoFramesPerSecond,
	uint audioBitrate,
	uint videoKeyframeIntervalSeconds,
	uint audioSampleRate,
	uint audioChannels,
	bool reconnectOnFailure,
	uint reconnectRetryDelaySeconds,
	uint reconnectRetryMaxAttempts)
{
	if (obs_frontend_replay_buffer_active())
	{
		obs_frontend_replay_buffer_stop();
	}

	if (obs_frontend_streaming_active())
	{
		obs_frontend_streaming_stop();
	}

	if (obs_frontend_recording_active())
	{
		obs_frontend_recording_stop();
	}

	// Wait for streaming to stop
	while (obs_frontend_streaming_active())
	{
		os_sleep_ms(10);
	}

	//const char* recordFormat = "mp4";

	config_t* basicConfig = obs_frontend_get_profile_config(); // does not increase refcount

	config_set_string(basicConfig, "Output", "Mode", "Advanced"); // Advanced???
	config_set_uint(basicConfig, "SimpleOutput", "VBitrate", videoBitrate);
	config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", "x264");
	config_set_uint(basicConfig, "SimpleOutput", "ABitrate", audioBitrate);
	config_set_bool(basicConfig, "SimpleOutput", "UseAdvanced", true); // Advanced???
	config_set_bool(basicConfig, "SimpleOutput", "EnforceBitrate", true);
	config_set_string(basicConfig, "SimpleOutput", "Preset", "veryfast");

	// Recording
	//config_set_string(basicConfig, "SimpleOutput", "RecFormat", recordFormat);
	config_set_string(basicConfig, "SimpleOutput", "RecQuality", "Stream");
	config_set_string(basicConfig, "SimpleOutput", "RecEncoder", "x264");
	//config_set_bool(basicConfig, "SimpleOutput", "RecRB", false);
	//config_set_int(basicConfig, "SimpleOutput", "RecRBTime", 20);
	//config_set_int(basicConfig, "SimpleOutput", "RecRBSize", 512);
	//config_set_string(basicConfig, "SimpleOutput", "RecRBPrefix", "Replay");

	config_set_bool(basicConfig, "AdvOut", "ApplyServiceSettings", true);
	config_set_bool(basicConfig, "AdvOut", "UseRescale", false);
	config_set_uint(basicConfig, "AdvOut", "TrackIndex", 1);

	if (videoEncoderId != NULL)
		config_set_string(basicConfig, "AdvOut", "Encoder", videoEncoderId);

	config_set_string(basicConfig, "AdvOut", "RecType", "Standard");

	//config_set_string(basicConfig, "AdvOut", "RecFilePath", GetDefaultVideoSavePath().c_str());
	//config_set_string(basicConfig, "AdvOut", "RecFormat", "mp4");
	//config_set_bool(basicConfig, "AdvOut", "RecUseRescale", false);
	//config_set_uint(basicConfig, "AdvOut", "RecTracks", (1 << 0));
	//config_set_string(basicConfig, "AdvOut", "RecEncoder", "none");

	//config_set_bool(basicConfig, "AdvOut", "FFOutputToFile", false);
	// config_set_string(basicConfig, "AdvOut", "FFFilePath", GetDefaultVideoSavePath().c_str());
	//config_set_string(basicConfig, "AdvOut", "FFExtension", recordFormat);
	config_set_uint(basicConfig, "AdvOut", "FFVBitrate", videoBitrate);
	config_set_uint(basicConfig, "AdvOut", "FFVGOPSize", videoKeyframeIntervalSeconds * videoFramesPerSecond);
	config_set_bool(basicConfig, "AdvOut", "FFUseRescale", false);
	config_set_bool(basicConfig, "AdvOut", "FFIgnoreCompat", false);
	config_set_uint(basicConfig, "AdvOut", "FFABitrate", audioBitrate);
	config_set_uint(basicConfig, "AdvOut", "FFAudioTrack", 1);

	config_set_uint(basicConfig, "AdvOut", "Track1Bitrate", audioBitrate);
	config_set_uint(basicConfig, "AdvOut", "Track2Bitrate", audioBitrate);
	config_set_uint(basicConfig, "AdvOut", "Track3Bitrate", audioBitrate);
	config_set_uint(basicConfig, "AdvOut", "Track4Bitrate", audioBitrate);
	config_set_uint(basicConfig, "AdvOut", "Track5Bitrate", audioBitrate);
	config_set_uint(basicConfig, "AdvOut", "Track6Bitrate", audioBitrate);

	// Replay buffer
	/*
	config_set_bool(basicConfig, "AdvOut", "RecRB", false);
	config_set_uint(basicConfig, "AdvOut", "RecRBTime", 20);
	config_set_int(basicConfig, "AdvOut", "RecRBSize", 512);
	*/
	config_set_uint(basicConfig, "Video", "BaseCX", videoWidth);
	config_set_uint(basicConfig, "Video", "BaseCY", videoHeight);

	// Output settings
	/*
	config_set_string(basicConfig, "Output", "FilenameFormatting", "%CCYY-%MM-%DD %hh-%mm-%ss");

	config_set_bool(basicConfig, "Output", "DelayEnable", false);
	config_set_uint(basicConfig, "Output", "DelaySec", 20);
	config_set_bool(basicConfig, "Output", "DelayPreserve", true);
	*/
	config_set_bool(basicConfig, "Output", "Reconnect", reconnectOnFailure);
	config_set_uint(basicConfig, "Output", "RetryDelay", reconnectRetryDelaySeconds);
	config_set_uint(basicConfig, "Output", "MaxRetries", reconnectRetryMaxAttempts);

	/*
	config_set_string(basicConfig, "Output", "BindIP", "default");
	config_set_bool(basicConfig, "Output", "NewSocketLoopEnable", false);
	config_set_bool(basicConfig, "Output", "LowLatencyEnable", false);
	*/

	config_set_uint(basicConfig, "Video", "OutputCX", videoWidth);
	config_set_uint(basicConfig, "Video", "OutputCY", videoHeight);

	config_set_uint(basicConfig, "Video", "FPSType", 1);
	config_set_string(basicConfig, "Video", "FPSCommon", fmt::format("{}", videoFramesPerSecond).c_str());
	config_set_uint(basicConfig, "Video", "FPSInt", videoFramesPerSecond);
	config_set_uint(basicConfig, "Video", "FPSNum", videoFramesPerSecond);
	config_set_uint(basicConfig, "Video", "FPSDen", 1);
	/*
	config_set_string(basicConfig, "Video", "ScaleType", "bicubic");
	config_set_string(basicConfig, "Video", "ColorFormat", "NV12");
	config_set_string(basicConfig, "Video", "ColorSpace", "601");
	config_set_string(basicConfig, "Video", "ColorRange", "Partial");
	*/

	// Audio settings
	//config_set_string(basicConfig, "Audio", "MonitoringDeviceId", "default");
	//config_set_string(basicConfig, "Audio", "MonitoringDeviceName", Str("Basic.Settings.Advanced.Audio.MonitoringDevice.Default"));
	config_set_uint(basicConfig, "Audio", "SampleRate", audioSampleRate);

	switch (audioChannels)
	{
	case 1: config_set_string(basicConfig, "Audio", "ChannelSetup", "Mono"); break;
	case 2: config_set_string(basicConfig, "Audio", "ChannelSetup", "Stereo"); break;
	case 3: config_set_string(basicConfig, "Audio", "ChannelSetup", "2.1"); break;
	case 4: config_set_string(basicConfig, "Audio", "ChannelSetup", "4.0"); break;
	case 5: config_set_string(basicConfig, "Audio", "ChannelSetup", "4.1"); break;
	case 6: config_set_string(basicConfig, "Audio", "ChannelSetup", "5.1"); break;

	case 7: config_set_string(basicConfig, "Audio", "ChannelSetup", "7.1"); break;
	case 8: config_set_string(basicConfig, "Audio", "ChannelSetup", "7.1"); break;

	default: config_set_string(basicConfig, "Audio", "ChannelSetup", "Stereo"); break;
	}

	// Streaming service
	obs_service_t* service = obs_service_create("rtmp_custom", "default_service", NULL, NULL);
	obs_data_t* service_settings = obs_service_get_settings(service);

	if (streamingServiceServerUrl != NULL)
		obs_data_set_string(service_settings, "server", streamingServiceServerUrl);

	if (streamingServiceStreamKey != NULL)
		obs_data_set_string(service_settings, "key", streamingServiceStreamKey);

	obs_data_set_bool(service_settings, "use_auth", streamingServiceUseAuth);
	if (streamingServiceUseAuth)
	{
		obs_data_set_string(service_settings, "username",
			streamingServiceAuthUsername != NULL ? streamingServiceAuthUsername : "");

		obs_data_set_string(service_settings, "password",
			streamingServiceAuthPassword != NULL ? streamingServiceAuthPassword : "");
	}

	obs_service_update(service, service_settings);

	obs_data_release(service_settings);

	// Set streaming service used by UI
	obs_frontend_set_streaming_service(service);

	obs_output_set_service(obs_frontend_get_streaming_output(), service);

	// Save streaming service used by UI
	obs_frontend_save_streaming_service();

	// Release service reference
	obs_service_release(service);

	// Save UI configuration
	obs_frontend_save();
}

void WCUIBrowserDialog::ObsEnableMainWindow()
{
	// Enable main window

	QMetaObject::invokeMethod(
		(QWidget*)obs_frontend_get_main_window(),
		"setEnabled",
		Q_ARG(bool, true));

	QApplication::processEvents();
}

void WCUIBrowserDialog::ObsDisableMainWindow()
{
	// Disable main window

	QMetaObject::invokeMethod(
		(QWidget*)obs_frontend_get_main_window(),
		"setEnabled",
		Q_ARG(bool, false));

	QApplication::processEvents();
}

void WCUIBrowserDialog::CloseModalDialog()
{
	QMetaObject::invokeMethod(
		this,
		"accept");
}

#ifndef WCUI_BROWSER_DIALOG_H
#define WCUI_BROWSER_DIALOG_H

#include <obs-frontend-api.h>

#include <QDialog>
#include <QWidget>

#include <util/platform.h>
#include <util/threading.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>

#include <pthread.h>

#include <vector>
#include "fmt/format.h"

#include "shared/browser-client.hpp"

#include "../wcui-async-task-queue.hpp"

namespace Ui {
	class WCUIBrowserDialog;
}

///
// Web Configuration UI dialog.
//
// Loads obs-browser-wcui-browser-dialog.html and responds to API call
// messages from CEF runtime vis CefClient::OnProcessMessageReceived.
//
class WCUIBrowserDialog:
	public QDialog,
	public CefClient
{
	Q_OBJECT

private:
	// Default browser handle when browser is not yet initialized
	const int BROWSER_HANDLE_NONE = -1;

public:
	///
	// Class constructor
	//
	// @param obs_module_path	path to obs-browser.dll, used to locate obs-browser-wcui-browser-dialog.html
	// @param cache_path		CEF cache path
	//
	explicit WCUIBrowserDialog(
		QWidget* parent,
		std::string obs_module_path,
		std::string cache_path);

	///
	// Class destructor
	//
	~WCUIBrowserDialog();

public:
	///
	// Disable OBS main window, initialize browser, show modal dialog, enable OBS main window when done
	//
	void ShowModal();

private:
	///
	// Browser initialization
	//
	// Create browser or navigate back to home page (obs-browser-wcui-browser-dialog.html)
	//
	void InitBrowser();

private:
	// Path to obs-browser.dll, used to locate obs-browser-wcui-browser-dialog.html
	std::string m_obs_module_path;

	// CEF cache path
	std::string m_cache_path;

	// QT generated UI
	Ui::WCUIBrowserDialog* ui;

	// Dialog window handle to embed CEF browser in
	cef_window_handle_t m_window_handle;

	// BrowserManager CEF browser handle
	int m_browser_handle = BROWSER_HANDLE_NONE;

	// Task queue for asynchronous tasks
	WCUIAsyncTaskQueue m_task_queue;

private slots: // OBS operations

	///
	// End modal dialog
	//
	void CloseModalDialog();

	///
	// Enable main OBS window
	//
	void ObsEnableMainWindow();

	///
	// Disable main OBS window
	//
	void ObsDisableMainWindow();

	///
	// Generate unique OBS scene collection name
	//
	// @param name	scene collection name to generate unique name from
	// @returns	unique scene collection name
	//
	std::string ObsGetUniqueSceneCollectionName(std::string name);

	///
	// Generate unique OBS source name
	//
	// @param name	source name to generate unique source from
	// @returns	unique source name
	//
	std::string ObsGetUniqueSourceName(std::string name);

	///
	// Get current number of OBS scenes
	size_t ObsScenesGetCount();

	///
	// Get current OBS scenes list
	//
	// @returns	OBS scenes list to be released with ObsReleaseStoredScenes()
	//
	obs_frontend_source_list& ObsStoreScenes();

	///
	// Remove all sources from supplied OBS scenes list
	//
	// @param scenes	OBS scenes list obtained via ObsStoreScenes()
	//
	void ObsScenesRemoveAllSources(obs_frontend_source_list& scenes);

	///
	// Remove all scenes from supplied scenes list
	//
	// @param scenes	OBS scenes list obtained via ObsStoreScenes()
	//
	void ObsRemoveStoredScenes(obs_frontend_source_list& scenes);

	///
	// Release stored OBS scenes list obtained via ObsStoreScenes()
	//
	// @param scenes	OBS scenes list obtained via ObsStoreScenes()
	//
	void ObsReleaseStoredScenes(obs_frontend_source_list& scenes);

	///
	// Add OBS scene
	//
	// @param name		OBS scene name
	// @param setCurrent	if true, the new scene will be set as current scene
	//
	void ObsAddScene(
		const char* name,
		bool setCurrent = true);

	///
	// Add OBS source to scene
	//
	// @param parentScene		OBS scene to add source to. If NULL, the current OBS scene will be used as parent
	// @param sourceId		OBS source type ID
	// @param sourceName		New source name
	// @param sourceSettings	New source settings
	// @param sourceHotkeyData	New source hotkey data
	// @param preferExistingSource	If true and a source of the same type exists, the existing source will be used instead of creating a new one
	// @param output_source		If not NULL, the new obs_source_t* reference will not be released and the source will be returned via this param
	// @param output_sceneitem	If not NULL, the new obs_sceneitem_t* reference will not be released and the scene item will be returned via this param
	//
	void ObsAddSource(
		obs_source_t* parentScene,
		const char* sourceId,
		const char* sourceName,
		obs_data_t* sourceSettings = NULL,
		obs_data_t* sourceHotkeyData = NULL,
		bool preferExistingSource = false,
		obs_source_t** output_source = NULL,
		obs_sceneitem_t** output_sceneitem = NULL);

	///
	// Add browser source via ObsAddSource()
	//
	// @param parentScene			OBS scene to add source to. If NULL, the current OBS scene will be used as parent
	// @param name				New source name
	// @param width				Browser width
	// @param height			Browser height
	// @param fps				Frames Per Second
	// @param url				Page URL to navigate to
	// @param shutdownWhenInactive		Shutdown browser source when not visible
	// @param css				Additional CSS
	// @param suspendElementsWhenInactive	Suspend active elements on page (video) when not visible
	//
	void ObsAddSourceBrowser(
		obs_source_t* parentScene,
		const char* name,
		const long long width,
		const long long height,
		const long long fps,
		const char* url,
		const bool shutdownWhenInactive = true,
		const char* css = "",
		const bool suspendElementsWhenInactive = false);

	///
	// Add browser source via ObsAddSource()
	//
	// @param parentScene			OBS scene to add source to. If NULL, the current OBS scene will be used as parent
	// @param name				New source name
	// @param left				Capture source position left
	// @param top				Capture source position top
	// @param maxWidth			Capture source maximum width.
	//					We will keep the video source aspect ratio and may reduce this value actual width to accomodate.
	//					When reduced, the source will be center aligned around the original coordinates center.
	// @param maxHeight			Capture source maximum height.
	//					We will keep the video source aspect ratio and may reduce this value actual width to accomodate.
	//					When reduced, the source will be center aligned around the original coordinates center.
	//
	void ObsAddSourceVideoCapture(
		obs_source_t* parentScene,
		const char* name,
		const int left,
		const int top,
		const int maxWidth,
		const int maxHeight);

	///
	// Add game capture source via ObsAddSource()
	//
	// @param parentScene			OBS scene to add source to. If NULL, the current OBS scene will be used as parent
	// @param name				New source name
	// @param multiGpuCompatibility		Support multiple GPUs
	// @param allowTransparency		Allow transparency
	// @param limitFramerate		Limit capture frame rate
	// @param captureCursor			Capture cursor
	// @param antiCheatHook			Install anti-cheat hook
	// @param captureOverlays		Capture third-party game overlays (such as steam)
	//
	void ObsAddSourceGame(
		obs_source_t* parentScene,
		const char* name,
		bool multiGpuCompatibility = true,
		bool allowTransparency = false,
		bool limitFramerate = false,
		bool captureCursor = false,
		bool antiCheatHook = true,
		bool captureOverlays = true);

	///
	// Setup output configuration (audio, video, streaming)
	//
	// @param streamingServiceServerUrl		RTMP server URL (rtmp://server/app)
	// @param streamingServiceStreamKey		RTMP server stream key
	// @param streamingServiceUseAuth		Use authentication?
	// @param streamingServiceAuthUsername		If streamingServiceUseAuth is true, specifies authentication username
	// @param streamingServiceAuthPassword		If streamingServiceUseAuth is true, specifies authentication password
	// @param videoEncoderId			Video encoder type ID
	// @param videoBitrate				Video bitrate (Kbps)
	// @param videoWidth				Video width
	// @param videoHeight				Video height
	// @param videoFramesPerSecond			Video frames per second
	// @param audioBitrate				Audio bitrate (Kbps)
	// @param videoKeyframeIntervalSeconds		Video keyframe interval
	// @param audioSampleRate			Audio sample rate
	// @param audioChannels				Number of audio channels
	// @param reconnectOnFailure			Reconnect to server on failure?
	// @param reconnectRetryDelaySeconds		Delay between server reconnect attempts
	// @param reconnectRetryMaxAttempts		Maximum number of server reconnect attempts
	//
	void ObsSetProfileOutputConfiguration(
		const char* streamingServiceServerUrl, // "rtmp://localhost/app1",
		const char* streamingServiceStreamKey, // "stream1",
		const bool streamingServiceUseAuth = false,
		const char* streamingServiceAuthUsername = "",
		const char* streamingServiceAuthPassword = "",
		const char* videoEncoderId = "obs_x264",
		// const char* audioEncoderId = "aac", // OBS auto-detects audio encoder from obs_output_t
		uint videoBitrate = 2500,
		uint videoWidth = 1920,
		uint videoHeight = 1080,
		uint videoFramesPerSecond = 25,
		uint audioBitrate = 128,
		uint videoKeyframeIntervalSeconds = 2,
		uint audioSampleRate = 44100,
		uint audioChannels = 2,
		bool reconnectOnFailure = true,
		uint reconnectRetryDelaySeconds = 10,
		uint reconnectRetryMaxAttempts = 20
	);

public: // CefClient implementation

	// Called when a new message is received from a different process. Return true
	// if the message was handled or false otherwise. Do not keep a reference to
	// or attempt to access the message outside of this callback.
	///
	/*--cef()--*/
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message);

private:

	///
	// To be called ONLY by OnProcessMessageReceived
	//
	void OnProcessMessageReceivedSendExecuteCallbackMessage(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefValue> callback_arg);

	///
	// To be called ONLY by OnProcessMessageReceived
	//
	void OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message,
		obs_encoder_type encoder_type);

public:
	IMPLEMENT_REFCOUNTING(WCUIBrowserDialog);

private:
	////////////////////////////////////////////////////////////////////////////
	//
	// Parse setupEnvironment request Javascript object content
	//
	// See obs-browser-wcui-browser-dialog.html setupEnvironment() call example
	// for more information.
	//
	////////////////////////////////////////////////////////////////////////////
	class c_values_container
	{
	public:
		c_values_container() { }

		c_values_container(c_values_container& other):
			strings(other.strings),
			integers(other.integers),
			booleans(other.booleans),
			decimals(other.decimals)
		{
		}

		c_values_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefDictionaryValue> dic =
				root->GetDictionary();

			if (dic != NULL)
			{
				CefDictionaryValue::KeyList keys;

				if (dic->GetKeys(keys))
				{
					for (size_t idx = 0; idx < keys.size(); ++idx)
					{
						CefString key = keys[idx];

						CefValueType type = dic->GetType(key);

						switch (type)
						{
						case VTYPE_STRING:
							strings[key] = dic->GetString(key);
							break;

						case VTYPE_INT:
							integers[key] = dic->GetInt(key);
							break;

						case VTYPE_BOOL:
							booleans[key] = dic->GetBool(key);
							break;

						case VTYPE_DOUBLE:
							decimals[key] = dic->GetDouble(key);
							break;
						}
					}
				}
			}
		}

	public:
		std::string string(std::string key, std::string default_value)
		{
			if (strings.count(key) > 0)
				return strings[key];
			else
				return default_value;
		}

		int integer(std::string key, int default_value)
		{
			if (integers.count(key) > 0)
				return integers[key];
			else
				return default_value;
		}

		bool boolean(std::string key, bool default_value)
		{
			if (booleans.count(key) > 0)
				return booleans[key];
			else
				return default_value;
		}

		double decimal(std::string key, double default_value)
		{
			if (decimals.count(key) > 0)
				return decimals[key];
			else
				return default_value;
		}

	public:
		std::map<std::string, std::string> strings;
		std::map<std::string, int> integers;
		std::map<std::string, bool> booleans;
		std::map<std::string, double> decimals;
	};

	class c_sources_list_container
	{
	public:
		c_sources_list_container() { }

		c_sources_list_container(c_sources_list_container& other):
			items(other.items)
		{
		}

		c_sources_list_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefListValue> list =
				root->GetList();

			if (list != NULL)
			{
				for (size_t idx = 0; idx < list->GetSize(); ++idx)
				{
					CefRefPtr<CefValue> values =
						list->GetValue(idx);

					items.push_back(c_values_container(values));
				}
			}
		}

	public:
		std::vector<c_values_container> items;
	};

	class c_scene_container
	{
	public:
		c_scene_container() { init_name(); }

		c_scene_container(c_scene_container& other) :
			name(other.name),
			sources(other.sources)
		{
		}

		c_scene_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefDictionaryValue> dic =
				root->GetDictionary();

			if (dic != NULL)
			{
				CefRefPtr<CefValue> nameVal = dic->GetValue("name");

				if (nameVal != NULL && nameVal->GetType() == VTYPE_STRING)
					name = nameVal->GetString();
				else
					init_name();

				sources = c_sources_list_container(dic->GetValue("sources"));
			}
		}

	private:
		void init_name()
		{
			name = fmt::format("Scene {}", os_gettime_ns());
		}

	public:
		std::string name;
		c_sources_list_container sources;
	};

	class c_scenes_list_container
	{
	public:
		c_scenes_list_container() { }

		c_scenes_list_container(c_scenes_list_container& other):
			items(other.items)
		{
		}

		c_scenes_list_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefListValue> list =
				root->GetList();

			if (list != NULL)
			{
				for (size_t idx = 0; idx < list->GetSize(); ++idx)
				{
					CefRefPtr<CefValue> values =
						list->GetValue(idx);

					items.push_back(c_scene_container(values));
				}
			}
		}

	public:
		std::vector<c_scene_container> items;
	};

	class c_input_container
	{
	public:
		c_input_container() { }

		c_input_container(c_input_container& other):
			scenes(other.scenes)
		{
		}

		c_input_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefDictionaryValue> dic =
				root->GetDictionary();

			if (dic != NULL)
				scenes = c_scenes_list_container(dic->GetValue("scenes"));
		}
	public:
		c_scenes_list_container scenes;
	};

	class c_output_container
	{
	public:
		c_output_container() { }

		c_output_container(c_output_container& other):
			audio(other.audio),
			video(other.video),
			stream(other.stream),
			defaultScene(other.defaultScene)
		{
		}

		c_output_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefDictionaryValue> dic =
				root->GetDictionary();

			if (dic != NULL)
			{
				CefRefPtr<CefValue> defaultSceneValue = dic->GetValue("defaultSceneName");

				if (defaultSceneValue != NULL)
					defaultScene = defaultSceneValue->GetString();

				audio = c_values_container(dic->GetValue("audio"));
				video = c_values_container(dic->GetValue("video"));
				stream = c_values_container(dic->GetValue("stream"));
			}
		}

	public:
		std::string defaultScene;
		c_values_container audio;
		c_values_container video;
		c_values_container stream;
	};

	class c_config_container
	{
	public:
		c_config_container(c_config_container& other):
			input(other.input),
			output(other.output)
		{
		}

		c_config_container(CefValue* root)
		{
			if (root == NULL) return;

			CefRefPtr<CefDictionaryValue> dic =
				root->GetDictionary();

			if (dic != NULL)
			{
				input = c_input_container(dic->GetValue("input"));
				output = c_output_container(dic->GetValue("output"));
			}
		}

	public:
		c_input_container input;
		c_output_container output;
	};
};

#endif // WCUI_BROWSER_DIALOG_H

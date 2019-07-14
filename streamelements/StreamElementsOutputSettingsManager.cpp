#include "StreamElementsOutputSettingsManager.hpp"
#include "StreamElementsUtils.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/config-file.h>

StreamElementsOutputSettingsManager::StreamElementsOutputSettingsManager()
{
}

StreamElementsOutputSettingsManager::~StreamElementsOutputSettingsManager()
{
}

void StreamElementsOutputSettingsManager::GetAvailableEncoders(CefRefPtr<CefValue>& result, obs_encoder_type* encoder_type)
{
	SYNC_ACCESS();

	// Response codec collection (array)
	CefRefPtr<CefListValue> codec_list = CefListValue::Create();

	// Response codec collection is our root object
	result->SetList(codec_list);

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
			if (!encoder_type || obs_get_encoder_type(encoderId) == *encoder_type)
			{
				CefRefPtr<CefDictionaryValue> codec = CefDictionaryValue::Create();

				// Set codec dictionary properties
				codec->SetString("id", encoderId);
				codec->SetString("codec", obs_get_encoder_codec(encoderId));
				codec->SetString("name", obs_encoder_get_display_name(encoderId));

				switch (obs_get_encoder_type(encoderId))
				{
				case OBS_ENCODER_AUDIO:
					codec->SetString("type", "audio");
					break;
				case OBS_ENCODER_VIDEO:
					codec->SetString("type", "video");
					break;
				default:
					codec->SetString("type", "unknown");
					break;
				}

				// Append dictionary to codec list
				codec_list->SetDictionary(
					codec_list->GetSize(),
					codec);
			}
		}
	}
}

void StreamElementsOutputSettingsManager::StopAllOutputs()
{
	SYNC_ACCESS();

	if (obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_stop();
	}

	if (obs_frontend_streaming_active()) {
		obs_frontend_streaming_stop();
	}

	if (obs_frontend_recording_active()) {
		obs_frontend_recording_stop();
	}

	// Wait for streaming to stop
	while (obs_frontend_streaming_active()) {
		os_sleep_ms(10);
	}
}

bool StreamElementsOutputSettingsManager::SetStreamingSettings(CefRefPtr<CefValue> input)
{
	SYNC_ACCESS();

	if (!input.get()) return false;
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d.get() || !d->HasKey("serverUrl") || !d->HasKey("streamKey")) {
		return false;
	}

	std::string serverUrl = d->GetString("serverUrl");
	std::string streamKey = d->GetString("streamKey");
	bool useAuth = d->HasKey("useAuth") && d->GetBool("useAuth");
	std::string authUsername = (useAuth && d->HasKey("authUsername")) ? d->GetString("authUsername") : "";
	std::string authPassword = (useAuth && d->HasKey("authPassword")) ? d->GetString("authPassword") : "";

	StopAllOutputs();

	// Streaming service
	obs_service_t* service = obs_service_create("rtmp_custom", "default_service", NULL, NULL);
	obs_data_t* service_settings = obs_service_get_settings(service);

	obs_data_set_string(service_settings, "server", serverUrl.c_str());
	obs_data_set_string(service_settings, "key", streamKey.c_str());

	obs_data_set_bool(service_settings, "use_auth", useAuth);
	if (useAuth)
	{
		obs_data_set_string(service_settings, "username", authUsername.c_str());
		obs_data_set_string(service_settings, "password", authPassword.c_str());
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

	return true;
}

#define SIMPLE_ENCODER_X264                    "x264"
#define SIMPLE_ENCODER_QSV                     "qsv"
#define SIMPLE_ENCODER_NVENC                   "nvenc" // Old ffmpeg-based NVEnc
#define SIMPLE_ENCODER_JIM_NVENC               "jim_nvenc"
#define SIMPLE_ENCODER_AMD                     "amd"

bool StreamElementsOutputSettingsManager::SetEncodingSettings(CefRefPtr<CefValue> input)
{
	SYNC_ACCESS();

	if (!input.get()) return false;
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d.get() ||
		!d->HasKey("videoEncoderId") ||
		!d->HasKey("videoBitsPerSecond") ||
		!d->HasKey("videoFrameWidth") ||
		!d->HasKey("videoFrameHeight") ||
		!d->HasKey("videoFramesPerSecond") ||
		!d->HasKey("videoKeyframeIntervalSeconds") ||
		// !d->HasKey("audioEncoderId") ||
		!d->HasKey("audioBitsPerSecond") ||
		!d->HasKey("audioSamplesPerSecond") ||
		!d->HasKey("audioChannels")) {
		return false;
	}

	std::string videoEncoderId = d->GetString("videoEncoderId");
	uint64_t videoBitrate = d->GetInt("videoBitsPerSecond") / 1000;
	uint64_t videoWidth = d->GetInt("videoFrameWidth");
	uint64_t videoHeight = d->GetInt("videoFrameHeight");
	uint64_t videoFramesPerSecond = d->GetInt("videoFramesPerSecond");
	uint64_t videoKeyframeIntervalSeconds = d->GetInt("videoKeyframeIntervalSeconds");
	//std::string audioEncoderId = d->GetString("audioEncoderId");
	uint64_t audioBitrate = d->GetInt("audioBitsPerSecond") / 1000;
	uint64_t audioSampleRate = d->GetInt("audioSamplesPerSecond");
	int audioChannels = d->GetInt("audioChannels");
	bool reconnectOnFailure = true;
	uint64_t reconnectRetryDelaySeconds = 10;
	uint64_t reconnectRetryMaxAttempts = 20;

	config_t* basicConfig = obs_frontend_get_profile_config(); // does not increase refcount

	config_set_string(basicConfig, "Output", "Mode", "Simple"); // Advanced???
	config_set_uint(basicConfig, "SimpleOutput", "VBitrate", videoBitrate);
	// config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", "x264");
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

	if (videoEncoderId.size()) {
		if (videoEncoderId == "ffmpeg_nvenc") {
			config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", SIMPLE_ENCODER_NVENC);
		}
		else if (videoEncoderId == "obs_qsv11") {
			config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", SIMPLE_ENCODER_QSV);
		}
		else if (videoEncoderId == "amd_amf_h264") {
			config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", SIMPLE_ENCODER_AMD);
		}
		else /* if (videoEncoderId == "obs_x264") */ {
			config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", SIMPLE_ENCODER_X264);
		}

		// obs_x264
		config_set_string(basicConfig, "AdvOut", "Encoder", videoEncoderId.c_str());
	}

	/*
	if (audioEncoderId.size()) {
		config_set_string(basicConfig, "AdvOut", "FFAEncoder", audioEncoderId.c_str());
	}
	*/

	//config_set_string(basicConfig, "AdvOut", "RecType", "Standard");

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
	config_set_string(basicConfig, "Video", "FPSCommon", FormatString("%d", videoFramesPerSecond).c_str());
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

	// Save UI configuration
	obs_frontend_save();

	// streamEncoder.json
	{
		// TODO: Find a better way to do this. This section assumes knowledge about front-end internals.
		std::string profileName = obs_frontend_get_current_profile();
		char* profileParentFolder = obs_module_get_config_path(obs_current_module(), "../../basic/profiles");

		std::string streamEncoderJsonPath = FormatString("%s/%s/streamEncoder.json", profileParentFolder, profileName.c_str());
		bfree(profileParentFolder);

		obs_data_t* settings = obs_data_create_from_json_file_safe(streamEncoderJsonPath.c_str(), "bak");
		if (!settings) {
			settings = obs_data_create();
		}

		obs_data_set_int(settings, "bitrate", videoBitrate);
		obs_data_save_json_safe(settings, streamEncoderJsonPath.c_str(), "tmp", "bak");
		obs_data_release(settings);
	}

	return true;
}

bool StreamElementsOutputSettingsManager::GetEncodingSettings(CefRefPtr<CefValue>& output)
{
	SYNC_ACCESS();

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	config_t* basicConfig = obs_frontend_get_profile_config(); // does not increase refcount
	
	if (stricmp(config_get_string(basicConfig, "Output", "Mode"), "Simple") == 0) {
		d->SetString("videoEncoderId", config_get_string(basicConfig, "SimpleOutput", "StreamEncoder"));
		d->SetInt("videoBitsPerSecond", config_get_uint(basicConfig, "SimpleOutput", "VBitrate") * 1000);
		d->SetInt("audioBitsPerSecond", config_get_uint(basicConfig, "SimpleOutput", "ABitrate") * 1000);
	}
	else
	{
		d->SetString("videoEncoderId", config_get_string(basicConfig, "AdvOut", "Encoder"));
		d->SetInt("videoBitsPerSecond", config_get_uint(basicConfig, "AdvOut", "FFVBitrate") * 1000);
		d->SetInt("audioBitsPerSecond", config_get_uint(basicConfig, "AdvOut", "FFABitrate") * 1000);

	}

	d->SetInt("videoFrameWidth", config_get_uint(basicConfig, "Video", "OutputCX"));
	d->SetInt("videoFrameHeight", config_get_uint(basicConfig, "Video", "OutputCY"));

	switch (config_get_uint(basicConfig, "Video", "FPSType")) {
		case 0: // Common
			d->SetDouble("videoFramesPerSecond", (double)atoi(config_get_string(basicConfig, "Video", "FPSCommon")));
			break;

		case 1: // Integer
			d->SetDouble("videoFramesPerSecond", config_get_uint(basicConfig, "Video", "FPSInt"));
			break;

		case 2: // Fractional
			d->SetDouble("videoFramesPerSecond",
				(double)config_get_uint(basicConfig, "Video", "FPSNum") / config_get_uint(basicConfig, "Video", "FPSDen"));
			break;
	}

	d->SetInt("audioSamplesPerSecond", config_get_uint(basicConfig, "Audio", "SampleRate"));

	output->SetDictionary(d);

	return true;
}

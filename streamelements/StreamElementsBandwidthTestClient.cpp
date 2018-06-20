#include "StreamElementsBandwidthTestClient.hpp"

#include <thread>

StreamElementsBandwidthTestClient::StreamElementsBandwidthTestClient()
	//: m_taskQueue("StreamElementsBandwidthTestClient task queue")
{
	os_event_init(&m_event_state_changed, OS_EVENT_TYPE_AUTO);
	os_event_init(&m_event_async_done, OS_EVENT_TYPE_MANUAL);

	// Initially done
	os_event_signal(m_event_async_done);
}


StreamElementsBandwidthTestClient::~StreamElementsBandwidthTestClient()
{
	CancelAll();

	os_event_destroy(m_event_async_done);
	os_event_destroy(m_event_state_changed);
}

StreamElementsBandwidthTestClient::Result StreamElementsBandwidthTestClient::TestServerBitsPerSecond(
	const char* serverUrl,
	const char* streamKey,
	const int maxBitrateBitsPerSecond,
	const char* bindToIP,
	const int durationSeconds,
	const bool useAuth,
	const char* const authUsername,
	const char* const authPassword)
{
	StreamElementsBandwidthTestClient::Result result;

	result.serverUrl = serverUrl;
	result.streamKey = streamKey;

	m_state = Running;
	os_event_reset(m_event_state_changed);

	obs_encoder_t* vencoder = obs_video_encoder_create("obs_x264", "test_x264", nullptr, nullptr);
	obs_encoder_t* aencoder = obs_audio_encoder_create("ffmpeg_aac", "test_aac", nullptr, 0, nullptr);
	obs_service_t* service = obs_service_create("rtmp_custom", "test_service", nullptr, nullptr);

	obs_data_t* service_settings = obs_data_create();

	// Configure video encoder
	obs_data_t* vencoder_settings = obs_data_create();

	obs_data_set_int(vencoder_settings, "bitrate", (int)(maxBitrateBitsPerSecond / 1000L));
	obs_data_set_string(vencoder_settings, "rate_control", "CBR");
	obs_data_set_string(vencoder_settings, "preset", "veryfast");
	obs_data_set_int(vencoder_settings, "keyint_sec", 2);

	obs_encoder_update(vencoder, vencoder_settings);

	obs_encoder_set_video(vencoder, obs_get_video());

	// Configure audio encoder
	obs_data_t* aencoder_settings = obs_data_create();

	obs_data_set_int(aencoder_settings, "bitrate", 128);

	obs_encoder_update(aencoder, aencoder_settings);

	obs_encoder_set_audio(aencoder, obs_get_audio());

	// Configure service

	obs_data_set_string(service_settings, "service", "rtmp_custom");
	obs_data_set_string(service_settings, "server", serverUrl);
	obs_data_set_string(service_settings, "key", streamKey);

	obs_data_set_bool(service_settings, "use_auth", useAuth);
	obs_data_set_string(service_settings, "username", authUsername ? authUsername : "");
	obs_data_set_string(service_settings, "password", authPassword ? authPassword : "");

	obs_service_update(service, service_settings);
	obs_service_apply_encoder_settings(service, vencoder_settings, aencoder_settings);

	// Configure output

	const char *output_type = obs_service_get_output_type(service);

	if (!output_type)
		output_type = "rtmp_output";

	obs_data_t* output_settings = obs_data_create();

	if (bindToIP) {
		obs_data_set_string(output_settings, "bind_ip", bindToIP);
	}

	obs_output_t* output = obs_output_create(output_type, "test_stream", output_settings, nullptr);

	// obs_output_update(output, output_settings);

	obs_output_set_video_encoder(output, vencoder);
	obs_output_set_audio_encoder(output, aencoder, 0);

	obs_output_set_service(output, service);

	// Setup output signal handler

	auto on_started = [](void* data, calldata_t*)
	{
		StreamElementsBandwidthTestClient* self = (StreamElementsBandwidthTestClient*)data;

		self->set_state(Running);
	};

	auto on_stopped = [](void* data, calldata_t*)
	{
		StreamElementsBandwidthTestClient* self = (StreamElementsBandwidthTestClient*)data;

		self->set_state(Stopped);
	};

	signal_handler *output_signal_handler = obs_output_get_signal_handler(output);
	signal_handler_connect(output_signal_handler, "start", on_started, this);
	signal_handler_connect(output_signal_handler, "stop", on_stopped, this);

	// Start testing

	if (obs_output_start(output))
	{
		wait_state_changed();

		if (m_state == Running)
		{
			// ignore first WARMUP_DURATION_MS due to possible buffering skewing
			// the result
			wait_state_changed(WARMUP_DURATION_MS);

			if (m_state == Running)
			{
				// Test bandwidth

				// Save start metrics
				uint64_t start_bytes = obs_output_get_total_bytes(output);
				uint64_t start_time_ns = os_gettime_ns();

				wait_state_changed((unsigned long)durationSeconds * 1000L);

				if (m_state == Running)
				{
					// Still running
					obs_output_stop(output);

					// Wait for stopped
					wait_state_changed();

					// Get end metrics
					uint64_t end_bytes = obs_output_get_total_bytes(output);
					uint64_t end_time_ns = os_gettime_ns();

					// Compute deltas
					uint64_t total_bytes = end_bytes - start_bytes;
					uint64_t total_time_ns = end_time_ns - start_time_ns;

					uint64_t total_bits = total_bytes * 8L;
					uint64_t total_time_seconds = total_time_ns / 1000000000L;

					result.connectTimeMilliseconds = obs_output_get_connect_time_ms(output);
					result.bitsPerSecond = total_bits / total_time_seconds;

					if (obs_output_get_frames_dropped(output))
					{
						// Frames were dropped, use 70% of available bandwidth
						result.bitsPerSecond =
							result.bitsPerSecond * 70L / 100L;
					}
					else
					{
						// No frames were dropped, use 100% of available bandwidth
						result.bitsPerSecond =
							result.bitsPerSecond * 100L / 100L;
					}

					// Ceiling
					if (result.bitsPerSecond > maxBitrateBitsPerSecond)
						result.bitsPerSecond = maxBitrateBitsPerSecond;

					result.success = true;
				}
			}
		}
	}

	if (!result.success)
	{
		if (m_state == Cancelled) {
			result.cancelled = true;

			obs_output_force_stop(output);

			wait_state_changed();
		}
	}

	signal_handler_disconnect(output_signal_handler, "start", on_started, this);
	signal_handler_disconnect(output_signal_handler, "stop", on_stopped, this);

	///
	// This part is copied as-is with minor modifications from
	// obs/window-basic-auto-config-test.cpp method
	// void AutoConfigTestPage::TestBandwidthThread()
	//
	// In that method, release calls are not made for reason which
	// is yet to be investigated.
	//
	// It has been impirically determined, that if release calls
	// *are* made, OBS will occasionally crash on shutdown.
	//
	// When those calls are commented out, the crash does not occur,
	// therefor we'll keep them commented out until the reason for
	// the subsequent crash is determined.
	//
	//obs_output_release(output);
	//obs_service_release(service);
	//obs_encoder_release(vencoder);
	//obs_encoder_release(aencoder);

	obs_data_release(output_settings);
	obs_data_release(service_settings);
	obs_data_release(vencoder_settings);
	obs_data_release(aencoder_settings);

	return result;
}

void StreamElementsBandwidthTestClient::TestServerBitsPerSecondAsync(
	const char* const serverUrl,
	const char* const streamKey,
	const int maxBitrateBitsPerSecond,
	const char* const bindToIP,
	const int durationSeconds,
	const bool useAuth,
	const char* const authUsername,
	const char* const authPassword,
	const TestServerBitsPerSecondAsyncCallback callback,
	void* const data)
{
	assert(!m_async_busy);
	assert(serverUrl);
	assert(streamKey);
	assert(maxBitrateBitsPerSecond > 0);
	assert(durationSeconds > 0);
	assert(callback);

	// Not done
	os_event_reset(m_event_async_done);

	struct local_context {
		StreamElementsBandwidthTestClient* self;
		std::string serverUrl;
		std::string streamKey;
		bool useAuth;
		std::string authUsername;
		std::string authPassword;
		int maxBitrateBitsPerSecond;
		std::string bindToIP;
		int durationSeconds;
		TestServerBitsPerSecondAsyncCallback callback;
		void* data;
	};

	local_context* context = new local_context();

	context->self = this;
	context->serverUrl = serverUrl;
	context->streamKey = streamKey;
	context->useAuth = useAuth;
	context->authUsername = authUsername != nullptr ? authUsername : "";
	context->authPassword = authPassword != nullptr ? authPassword : "";
	context->maxBitrateBitsPerSecond = maxBitrateBitsPerSecond;

	if (bindToIP) {
		context->bindToIP = bindToIP;
	}

	context->durationSeconds = durationSeconds;
	context->callback = callback;
	context->data = data;

	context->self->m_async_busy = true;

	std::thread thread([=]() {
		Result result = context->self->TestServerBitsPerSecond(
			context->serverUrl.c_str(),
			context->streamKey.c_str(),
			context->maxBitrateBitsPerSecond,
			context->bindToIP.empty() ? nullptr : context->bindToIP.c_str(),
			context->durationSeconds,
			context->useAuth,
			context->authUsername.c_str(),
			context->authPassword.c_str());

		context->callback(&result, context->data);

		context->self->m_async_busy = false;

		os_event_signal(context->self->m_event_async_done);

		delete context;
	});
	thread.detach();
}

void StreamElementsBandwidthTestClient::CancelAll()
{
	set_state(Cancelled);

	if (m_async_busy) {
		os_event_wait(m_event_async_done);
	}
}

void StreamElementsBandwidthTestClient::TestMultipleServersBitsPerSecondAsync(
	std::vector<Server> servers,
	const int maxBitrateBitsPerSecond,
	const char* const bindToIP,
	const int durationSeconds,
	const TestMultipleServersBitsPerSecondAsyncCallback progress_callback,
	const TestMultipleServersBitsPerSecondAsyncCallback callback,
	void* const data)
{
	struct local_context {
		StreamElementsBandwidthTestClient* self;
		TestMultipleServersBitsPerSecondAsyncCallback callback;
		void* data;
		std::vector<Server> servers;
		std::vector<Result> results;

		int maxBitrateBitsPerSecond;
		std::string bindToIP;
		int durationSeconds;
	};

	local_context* context = new local_context();

	context->self = this;
	context->callback = callback;
	context->data = data;
	context->servers = servers;

	if (bindToIP) {
		context->bindToIP = bindToIP;
	}

	context->maxBitrateBitsPerSecond = maxBitrateBitsPerSecond;
	context->durationSeconds = durationSeconds;

	context->self->m_async_busy = true;

	std::thread worker = std::thread([=]() {
		for (int i = 0; i < context->servers.size(); ++i) {
			Result testResult = TestServerBitsPerSecond(
				context->servers[i].url.c_str(),
				context->servers[i].streamKey.c_str(),
				context->maxBitrateBitsPerSecond,
				context->bindToIP.empty() ? nullptr : context->bindToIP.c_str(),
				context->durationSeconds);

			if (testResult.cancelled)
				break;

			context->results.emplace_back(testResult);

			if (progress_callback) {
				progress_callback(&context->results, context->data);
			}
		}

		context->callback(&context->results, context->data);

		context->self->m_async_busy = false;

		delete context;
	});

	worker.detach();
}

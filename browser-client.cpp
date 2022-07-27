/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "browser-client.hpp"
#include "obs-browser-source.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/platform.h>
#include <QApplication>
#include <QThread>
#include <QToolTip>
#if defined(__APPLE__) && CHROME_VERSION_BUILD > 4430
#include <IOSurface/IOSurface.h>
#endif

using namespace json11;

inline bool BrowserClient::valid() const
{
	return !!bs && !bs->destroying;
}

CefRefPtr<CefLoadHandler> BrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> BrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> BrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefContextMenuHandler> BrowserClient::GetContextMenuHandler()
{
	return this;
}

CefRefPtr<CefAudioHandler> BrowserClient::GetAudioHandler()
{
	return reroute_audio ? this : nullptr;
}

#if CHROME_VERSION_BUILD >= 4638
CefRefPtr<CefRequestHandler> BrowserClient::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefResourceRequestHandler> BrowserClient::GetResourceRequestHandler(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
	CefRefPtr<CefRequest> request, bool, bool, const CefString &, bool &)
{
	if (request->GetHeaderByName("origin") == "null") {
		return this;
	}

	return nullptr;
}

CefResourceRequestHandler::ReturnValue
BrowserClient::OnBeforeResourceLoad(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
				    CefRefPtr<CefRequest>,
				    CefRefPtr<CefCallback>)
{
	return RV_CONTINUE;
}
#endif

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
				  const CefString &, const CefString &,
				  cef_window_open_disposition_t, bool,
				  const CefPopupFeatures &, CefWindowInfo &,
				  CefRefPtr<CefClient> &, CefBrowserSettings &,
				  CefRefPtr<CefDictionaryValue> &, bool *)
{
	/* block popups */
	return true;
}

void BrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser>,
					CefRefPtr<CefFrame>,
					CefRefPtr<CefContextMenuParams>,
					CefRefPtr<CefMenuModel> model)
{
	/* remove all context menu contributions */
	model->Clear();
}

bool BrowserClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>, CefProcessId,
	CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();
	CefRefPtr<CefListValue> input_args = message->GetArgumentList();
	Json json;

	if (!valid()) {
		return false;
	}

	// Fall-through switch, so that higher levels also have lower-level rights
	switch (webpage_control_level) {
	case ControlLevel::All:
		if (name == "startRecording") {
			obs_frontend_recording_start();
		} else if (name == "stopRecording") {
			obs_frontend_recording_stop();
		} else if (name == "startStreaming") {
			obs_frontend_streaming_start();
		} else if (name == "stopStreaming") {
			obs_frontend_streaming_stop();
		} else if (name == "pauseRecording") {
			obs_frontend_recording_pause(true);
		} else if (name == "unpauseRecording") {
			obs_frontend_recording_pause(false);
		} else if (name == "startVirtualcam") {
			obs_frontend_start_virtualcam();
		} else if (name == "stopVirtualcam") {
			obs_frontend_stop_virtualcam();
		}
		[[fallthrough]];
	case ControlLevel::Advanced:
		if (name == "startReplayBuffer") {
			obs_frontend_replay_buffer_start();
		} else if (name == "stopReplayBuffer") {
			obs_frontend_replay_buffer_stop();
		} else if (name == "setCurrentScene") {
			const std::string scene_name =
				input_args->GetString(1).ToString();
			OBSSourceAutoRelease source =
				obs_get_source_by_name(scene_name.c_str());
			if (!source) {
				blog(LOG_WARNING,
				     "Browser source '%s' tried to switch to scene '%s' which doesn't exist",
				     obs_source_get_name(bs->source),
				     scene_name.c_str());
			} else if (!obs_source_is_scene(source)) {
				blog(LOG_WARNING,
				     "Browser source '%s' tried to switch to '%s' which isn't a scene",
				     obs_source_get_name(bs->source),
				     scene_name.c_str());
			} else {
				obs_frontend_set_current_scene(source);
			}
		} else if (name == "setCurrentTransition") {
			const std::string transition_name =
				input_args->GetString(1).ToString();
			obs_frontend_source_list transitions = {};
			obs_frontend_get_transitions(&transitions);

			OBSSourceAutoRelease transition;
			for (size_t i = 0; i < transitions.sources.num; i++) {
				obs_source_t *source =
					transitions.sources.array[i];
				if (obs_source_get_name(source) ==
				    transition_name) {
					transition = obs_source_get_ref(source);
					break;
				}
			}

			obs_frontend_source_list_free(&transitions);

			if (transition)
				obs_frontend_set_current_transition(transition);
			else
				blog(LOG_WARNING,
				     "Browser source '%s' tried to change the current transition to '%s' which doesn't exist",
				     obs_source_get_name(bs->source),
				     transition_name.c_str());
		}
		[[fallthrough]];
	case ControlLevel::Basic:
		if (name == "saveReplayBuffer") {
			obs_frontend_replay_buffer_save();
		}
		[[fallthrough]];
	case ControlLevel::ReadUser:
		if (name == "getScenes") {
			struct obs_frontend_source_list list = {};
			obs_frontend_get_scenes(&list);
			std::vector<const char *> scenes_vector;
			for (size_t i = 0; i < list.sources.num; i++) {
				obs_source_t *source = list.sources.array[i];
				scenes_vector.push_back(
					obs_source_get_name(source));
			}
			json = scenes_vector;
			obs_frontend_source_list_free(&list);
		} else if (name == "getCurrentScene") {
			OBSSourceAutoRelease current_scene =
				obs_frontend_get_current_scene();

			if (!current_scene)
				return false;

			const char *name = obs_source_get_name(current_scene);
			if (!name)
				return false;

			json = Json::object{
				{"name", name},
				{"width",
				 (int)obs_source_get_width(current_scene)},
				{"height",
				 (int)obs_source_get_height(current_scene)}};
		} else if (name == "getTransitions") {
			struct obs_frontend_source_list list = {};
			obs_frontend_get_transitions(&list);
			std::vector<const char *> transitions_vector;
			for (size_t i = 0; i < list.sources.num; i++) {
				obs_source_t *source = list.sources.array[i];
				transitions_vector.push_back(
					obs_source_get_name(source));
			}
			json = transitions_vector;
			obs_frontend_source_list_free(&list);
		} else if (name == "getCurrentTransition") {
			OBSSourceAutoRelease source =
				obs_frontend_get_current_transition();
			json = obs_source_get_name(source);
		}
		[[fallthrough]];
	case ControlLevel::ReadObs:
		if (name == "getStatus") {
			json = Json::object{
				{"recording", obs_frontend_recording_active()},
				{"streaming", obs_frontend_streaming_active()},
				{"recordingPaused",
				 obs_frontend_recording_paused()},
				{"replaybuffer",
				 obs_frontend_replay_buffer_active()},
				{"virtualcam",
				 obs_frontend_virtualcam_active()}};
		}
		[[fallthrough]];
	case ControlLevel::None:
		if (name == "getControlLevel") {
			json = (int)webpage_control_level;
		}
	}

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("executeCallback");

	CefRefPtr<CefListValue> execute_args = msg->GetArgumentList();
	execute_args->SetInt(0, input_args->GetInt(0));
	execute_args->SetString(1, json.dump());

	SendBrowserProcessMessage(browser, PID_RENDERER, msg);

	return true;
}

void BrowserClient::GetViewRect(CefRefPtr<CefBrowser>, CefRect &rect)
{
	if (!valid()) {
		rect.Set(0, 0, 16, 16);
		return;
	}

	rect.Set(0, 0, bs->width < 1 ? 1 : bs->width,
		 bs->height < 1 ? 1 : bs->height);
}

bool BrowserClient::OnTooltip(CefRefPtr<CefBrowser>, CefString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	std::string str_text = text;
	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(), [str_text]() {
			QToolTip::showText(QCursor::pos(), str_text.c_str());
		});
	return true;
#else
	UNUSED_PARAMETER(text);
	return false;
#endif
}

void BrowserClient::OnPaint(CefRefPtr<CefBrowser>, PaintElementType type,
			    const RectList &, const void *buffer, int width,
			    int height)
{
	if (type != PET_VIEW) {
		// TODO Overlay texture on top of bs->texture
		return;
	}

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	if (sharing_available) {
		return;
	}
#endif

	if (!valid()) {
		return;
	}

	if (bs->width != width || bs->height != height) {
		obs_enter_graphics();
		bs->DestroyTextures();
		obs_leave_graphics();
	}

	if (!bs->texture && width && height) {
		obs_enter_graphics();
		bs->texture = gs_texture_create(width, height, GS_BGRA, 1,
						(const uint8_t **)&buffer,
						GS_DYNAMIC);
		bs->width = width;
		bs->height = height;
		obs_leave_graphics();
	} else {
		obs_enter_graphics();
		gs_texture_set_image(bs->texture, (const uint8_t *)buffer,
				     width * 4, false);
		obs_leave_graphics();
	}
}

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
void BrowserClient::UpdateExtraTexture()
{
	if (bs->texture) {
		const uint32_t cx = gs_texture_get_width(bs->texture);
		const uint32_t cy = gs_texture_get_height(bs->texture);
		const gs_color_format format =
			gs_texture_get_color_format(bs->texture);
		const gs_color_format linear_format =
			gs_generalize_format(format);

		if (linear_format != format) {
			if (!bs->extra_texture ||
			    bs->last_format != linear_format ||
			    bs->last_cx != cx || bs->last_cy != cy) {
				if (bs->extra_texture) {
					gs_texture_destroy(bs->extra_texture);
					bs->extra_texture = nullptr;
				}
				bs->extra_texture = gs_texture_create(
					cx, cy, linear_format, 1, nullptr, 0);
				bs->last_cx = cx;
				bs->last_cy = cy;
				bs->last_format = linear_format;
			}
		} else if (bs->extra_texture) {
			gs_texture_destroy(bs->extra_texture);
			bs->extra_texture = nullptr;
			bs->last_cx = 0;
			bs->last_cy = 0;
			bs->last_format = GS_UNKNOWN;
		}
	}
}

void BrowserClient::OnAcceleratedPaint(CefRefPtr<CefBrowser>,
				       PaintElementType type, const RectList &,
				       void *shared_handle)
{
	if (type != PET_VIEW) {
		// TODO Overlay texture on top of bs->texture
		return;
	}

	if (!valid()) {
		return;
	}

#ifndef _WIN32
	if (shared_handle == bs->last_handle)
		return;
#endif

	obs_enter_graphics();

	if (bs->texture) {
#ifdef _WIN32
		//gs_texture_release_sync(bs->texture, 0);
#endif
		gs_texture_destroy(bs->texture);
		bs->texture = nullptr;
	}

#if defined(__APPLE__) && CHROME_VERSION_BUILD > 4183
	bs->texture = gs_texture_create_from_iosurface(
		(IOSurfaceRef)(uintptr_t)shared_handle);
#elif defined(_WIN32) && CHROME_VERSION_BUILD > 4183
	bs->texture =
		gs_texture_open_nt_shared((uint32_t)(uintptr_t)shared_handle);
	//if (bs->texture)
	//	gs_texture_acquire_sync(bs->texture, 1, INFINITE);

#else
	bs->texture =
		gs_texture_open_shared((uint32_t)(uintptr_t)shared_handle);
#endif
	UpdateExtraTexture();
	obs_leave_graphics();

	bs->last_handle = shared_handle;
}

#ifdef CEF_ON_ACCELERATED_PAINT2
void BrowserClient::OnAcceleratedPaint2(CefRefPtr<CefBrowser>,
					PaintElementType type, const RectList &,
					void *shared_handle, bool new_texture)
{
	if (type != PET_VIEW) {
		// TODO Overlay texture on top of bs->texture
		return;
	}

	if (!valid()) {
		return;
	}

	if (!new_texture) {
		return;
	}

	obs_enter_graphics();

	if (bs->texture) {
		gs_texture_destroy(bs->texture);
		bs->texture = nullptr;
	}

#if defined(__APPLE__) && CHROME_VERSION_BUILD > 4183
	bs->texture = gs_texture_create_from_iosurface(
		(IOSurfaceRef)(uintptr_t)shared_handle);
#elif defined(_WIN32) && CHROME_VERSION_BUILD > 4183
	bs->texture =
		gs_texture_open_nt_shared((uint32_t)(uintptr_t)shared_handle);

#else
	bs->texture =
		gs_texture_open_shared((uint32_t)(uintptr_t)shared_handle);
#endif
	UpdateExtraTexture();
	obs_leave_graphics();
}
#endif
#endif

static speaker_layout GetSpeakerLayout(CefAudioHandler::ChannelLayout cefLayout)
{
	switch (cefLayout) {
	case CEF_CHANNEL_LAYOUT_MONO:
		return SPEAKERS_MONO; /**< Channels: MONO */
	case CEF_CHANNEL_LAYOUT_STEREO:
		return SPEAKERS_STEREO; /**< Channels: FL, FR */
	case CEF_CHANNEL_LAYOUT_2POINT1:
		return SPEAKERS_2POINT1; /**< Channels: FL, FR, LFE */
	case CEF_CHANNEL_LAYOUT_2_2:
	case CEF_CHANNEL_LAYOUT_QUAD:
	case CEF_CHANNEL_LAYOUT_4_0:
		return SPEAKERS_4POINT0; /**< Channels: FL, FR, FC, RC */
	case CEF_CHANNEL_LAYOUT_4_1:
		return SPEAKERS_4POINT1; /**< Channels: FL, FR, FC, LFE, RC */
	case CEF_CHANNEL_LAYOUT_5_1:
	case CEF_CHANNEL_LAYOUT_5_1_BACK:
		return SPEAKERS_5POINT1; /**< Channels: FL, FR, FC, LFE, RL, RR */
	case CEF_CHANNEL_LAYOUT_7_1:
	case CEF_CHANNEL_LAYOUT_7_1_WIDE_BACK:
	case CEF_CHANNEL_LAYOUT_7_1_WIDE:
		return SPEAKERS_7POINT1; /**< Channels: FL, FR, FC, LFE, RL, RR, SL, SR */
	default:
		return SPEAKERS_UNKNOWN;
	}
}

#if CHROME_VERSION_BUILD >= 4103
void BrowserClient::OnAudioStreamStarted(CefRefPtr<CefBrowser> browser,
					 const CefAudioParameters &params_,
					 int channels_)
{
	UNUSED_PARAMETER(browser);
	channels = channels_;
	channel_layout = (ChannelLayout)params_.channel_layout;
	sample_rate = params_.sample_rate;
	frames_per_buffer = params_.frames_per_buffer;
}

void BrowserClient::OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
					const float **data, int frames,
					int64_t pts)
{
	UNUSED_PARAMETER(browser);
	if (!valid()) {
		return;
	}
	struct obs_source_audio audio = {};
	const uint8_t **pcm = (const uint8_t **)data;
	speaker_layout speakers = GetSpeakerLayout(channel_layout);
	int speaker_count = get_audio_channels(speakers);
	for (int i = 0; i < speaker_count; i++)
		audio.data[i] = pcm[i];
	audio.samples_per_sec = sample_rate;
	audio.frames = frames;
	audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
	audio.speakers = speakers;
	audio.timestamp = (uint64_t)pts * 1000000LLU;
	obs_source_output_audio(bs->source, &audio);
}

void BrowserClient::OnAudioStreamStopped(CefRefPtr<CefBrowser> browser)
{
	UNUSED_PARAMETER(browser);
}

void BrowserClient::OnAudioStreamError(CefRefPtr<CefBrowser> browser,
				       const CefString &message)
{
	UNUSED_PARAMETER(browser);
	UNUSED_PARAMETER(message);
}

static CefAudioHandler::ChannelLayout Convert2CEFSpeakerLayout(int channels)
{
	switch (channels) {
	case 1:
		return CEF_CHANNEL_LAYOUT_MONO;
	case 2:
		return CEF_CHANNEL_LAYOUT_STEREO;
	case 3:
		return CEF_CHANNEL_LAYOUT_2_1;
	case 4:
		return CEF_CHANNEL_LAYOUT_4_0;
	case 5:
		return CEF_CHANNEL_LAYOUT_4_1;
	case 6:
		return CEF_CHANNEL_LAYOUT_5_1;
	case 8:
		return CEF_CHANNEL_LAYOUT_7_1;
	default:
		return CEF_CHANNEL_LAYOUT_UNSUPPORTED;
	}
}

bool BrowserClient::GetAudioParameters(CefRefPtr<CefBrowser> browser,
				       CefAudioParameters &params)
{
	UNUSED_PARAMETER(browser);
	int channels = (int)audio_output_get_channels(obs_get_audio());
	params.channel_layout = Convert2CEFSpeakerLayout(channels);
	params.sample_rate = (int)audio_output_get_sample_rate(obs_get_audio());
	params.frames_per_buffer = kFramesPerBuffer;
	return true;
}
#elif CHROME_VERSION_BUILD < 4103
void BrowserClient::OnAudioStreamStarted(CefRefPtr<CefBrowser> browser, int id,
					 int, ChannelLayout channel_layout,
					 int sample_rate, int)
{
	UNUSED_PARAMETER(browser);
	if (!valid()) {
		return;
	}

	AudioStream &stream = bs->audio_streams[id];
	if (!stream.source) {
		stream.source = obs_source_create_private("audio_line", nullptr,
							  nullptr);

		obs_source_add_active_child(bs->source, stream.source);

		std::lock_guard<std::mutex> lock(bs->audio_sources_mutex);
		bs->audio_sources.push_back(stream.source);
	}

	stream.speakers = GetSpeakerLayout(channel_layout);
	stream.channels = get_audio_channels(stream.speakers);
	stream.sample_rate = sample_rate;
}

void BrowserClient::OnAudioStreamPacket(CefRefPtr<CefBrowser> browser, int id,
					const float **data, int frames,
					int64_t pts)
{
	UNUSED_PARAMETER(browser);
	if (!valid()) {
		return;
	}

	AudioStream &stream = bs->audio_streams[id];
	struct obs_source_audio audio = {};

	const uint8_t **pcm = (const uint8_t **)data;
	for (int i = 0; i < stream.channels; i++)
		audio.data[i] = pcm[i];

	audio.samples_per_sec = stream.sample_rate;
	audio.frames = frames;
	audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
	audio.speakers = stream.speakers;
	audio.timestamp = (uint64_t)pts * 1000000LLU;

	obs_source_output_audio(stream.source, &audio);
}

void BrowserClient::OnAudioStreamStopped(CefRefPtr<CefBrowser> browser, int id)
{
	UNUSED_PARAMETER(browser);
	if (!valid()) {
		return;
	}

	auto pair = bs->audio_streams.find(id);
	if (pair == bs->audio_streams.end()) {
		return;
	}

	AudioStream &stream = pair->second;
	{
		std::lock_guard<std::mutex> lock(bs->audio_sources_mutex);
		for (size_t i = 0; i < bs->audio_sources.size(); i++) {
			obs_source_t *source = bs->audio_sources[i];
			if (source == stream.source) {
				bs->audio_sources.erase(
					bs->audio_sources.begin() + i);
				break;
			}
		}
	}
	bs->audio_streams.erase(pair);
}
#endif

void BrowserClient::OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
			      int)
{
	if (!valid()) {
		return;
	}

	if (frame->IsMain() && bs->css.length()) {
		std::string uriEncodedCSS =
			CefURIEncode(bs->css, false).ToString();

		std::string script;
		script += "const obsCSS = document.createElement('style');";
		script += "obsCSS.innerHTML = decodeURIComponent(\"" +
			  uriEncodedCSS + "\");";
		script += "document.querySelector('head').appendChild(obsCSS);";

		frame->ExecuteJavaScript(script, "", 0);
	}
}

bool BrowserClient::OnConsoleMessage(CefRefPtr<CefBrowser>,
				     cef_log_severity_t level,
				     const CefString &message,
				     const CefString &source, int line)
{
	int errorLevel = LOG_INFO;
	switch (level) {
	case LOGSEVERITY_ERROR:
		errorLevel = LOG_WARNING;
		break;
	case LOGSEVERITY_FATAL:
		errorLevel = LOG_ERROR;
		break;
	default:
		return false;
	}

	blog(errorLevel, "obs-browser: %s (source: %s:%d)",
	     message.ToString().c_str(), source.ToString().c_str(), line);
	return false;
}

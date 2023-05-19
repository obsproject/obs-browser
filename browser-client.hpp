/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

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

#pragma once

#include <graphics/graphics.h>
#include <util/threading.h>
#include "cef-headers.hpp"
#include "obs-browser-source.hpp"

struct BrowserSource;

class BrowserClient : public CefClient,
		      public CefDisplayHandler,
		      public CefLifeSpanHandler,
		      public CefRequestHandler,
#if CHROME_VERSION_BUILD >= 4638
		      public CefResourceRequestHandler,
#endif
		      public CefContextMenuHandler,
		      public CefRenderHandler,
		      public CefAudioHandler,
		      public CefLoadHandler {

	bool sharing_available = false;
	bool reroute_audio = true;
	ControlLevel webpage_control_level = DEFAULT_CONTROL_LEVEL;

	inline bool valid() const;

	void UpdateExtraTexture();

public:
	BrowserSource *bs;
	CefRect popupRect;
	CefRect originalPopupRect;

#if CHROME_VERSION_BUILD >= 4103
	int sample_rate;
	int channels;
	ChannelLayout channel_layout;
	int frames_per_buffer;
#endif
	inline BrowserClient(BrowserSource *bs_, bool sharing_avail,
			     bool reroute_audio_,
			     ControlLevel webpage_control_level_)
		: sharing_available(sharing_avail),
		  reroute_audio(reroute_audio_),
		  webpage_control_level(webpage_control_level_),
		  bs(bs_)
	{
	}

	/* CefClient */
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
#if CHROME_VERSION_BUILD >= 4638
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
#endif
	virtual CefRefPtr<CefContextMenuHandler>
	GetContextMenuHandler() override;
	virtual CefRefPtr<CefAudioHandler> GetAudioHandler() override;

	virtual bool
	OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefProcessId source_process,
				 CefRefPtr<CefProcessMessage> message) override;

	/* CefDisplayHandler */
	virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
				      cef_log_severity_t level,
				      const CefString &message,
				      const CefString &source,
				      int line) override;
	virtual bool OnTooltip(CefRefPtr<CefBrowser> browser,
			       CefString &text) override;

	/* CefLifeSpanHandler */
	virtual bool
	OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		      const CefString &target_url,
		      const CefString &target_frame_name,
		      cef_window_open_disposition_t target_disposition,
		      bool user_gesture, const CefPopupFeatures &popupFeatures,
		      CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client,
		      CefBrowserSettings &settings,
		      CefRefPtr<CefDictionaryValue> &extra_info,
		      bool *no_javascript_access) override;
#if CHROME_VERSION_BUILD >= 4638
	/* CefRequestHandler */
	virtual CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request, bool is_navigation,
		bool is_download, const CefString &request_initiator,
		bool &disable_default_handling) override;

	/* CefResourceRequestHandler */
	virtual CefResourceRequestHandler::ReturnValue
	OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
			     CefRefPtr<CefFrame> frame,
			     CefRefPtr<CefRequest> request,
			     CefRefPtr<CefCallback> callback) override;
#endif

	/* CefContextMenuHandler */
	virtual void
	OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
			    CefRefPtr<CefFrame> frame,
			    CefRefPtr<CefContextMenuParams> params,
			    CefRefPtr<CefMenuModel> model) override;

	/* CefRenderHandler */
	virtual void GetViewRect(CefRefPtr<CefBrowser> browser,
				 CefRect &rect) override;
	virtual void OnPaint(CefRefPtr<CefBrowser> browser,
			     PaintElementType type, const RectList &dirtyRects,
			     const void *buffer, int width,
			     int height) override;
#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
					PaintElementType type,
					const RectList &dirtyRects,
					void *shared_handle) override;
#ifdef CEF_ON_ACCELERATED_PAINT2
	virtual void OnAcceleratedPaint2(CefRefPtr<CefBrowser> browser,
					 PaintElementType type,
					 const RectList &dirtyRects,
					 void *shared_handle,
					 bool new_texture) override;
#endif
#endif
#if CHROME_VERSION_BUILD >= 4103
	virtual void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
					 const float **data, int frames,
					 int64_t pts) override;

	virtual void
	OnAudioStreamStopped(CefRefPtr<CefBrowser> browser) override;

	virtual void OnAudioStreamStarted(CefRefPtr<CefBrowser> browser,
					  const CefAudioParameters &params,
					  int channels) override;
	virtual void OnAudioStreamError(CefRefPtr<CefBrowser> browser,
					const CefString &message) override;
	const int kFramesPerBuffer = 1024;
	virtual bool GetAudioParameters(CefRefPtr<CefBrowser> browser,
					CefAudioParameters &params) override;
#else
	virtual void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
					 int audio_stream_id,
					 const float **data, int frames,
					 int64_t pts) override;

	virtual void OnAudioStreamStopped(CefRefPtr<CefBrowser> browser,
					  int audio_stream_id);

	virtual void OnAudioStreamStarted(CefRefPtr<CefBrowser> browser,
					  int audio_stream_id, int channels,
					  ChannelLayout channel_layout,
					  int sample_rate,
					  int frames_per_buffer) override;
#endif
	/* CefLoadHandler */
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
			       CefRefPtr<CefFrame> frame,
			       int httpStatusCode) override;

	IMPLEMENT_REFCOUNTING(BrowserClient);
};

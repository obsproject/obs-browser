#pragma once

#include <obs.h>
#include "../cef-headers.hpp"

class StreamElementsOutputSettingsManager
{
public:
	StreamElementsOutputSettingsManager();
	virtual ~StreamElementsOutputSettingsManager();

public:
	void GetAvailableEncoders(CefRefPtr<CefValue>& result, obs_encoder_type* encoder_type = nullptr);

	bool SetStreamingSettings(CefRefPtr<CefValue> input);

	bool SetEncodingSettings(CefRefPtr<CefValue> input);
	bool GetEncodingSettings(CefRefPtr<CefValue>& output);

private:
	void StopAllOutputs();
};

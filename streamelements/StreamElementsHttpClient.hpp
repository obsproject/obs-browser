#pragma once

#include "cef-headers.hpp"

class StreamElementsHttpClient
{
public:
	StreamElementsHttpClient() {}
	~StreamElementsHttpClient() {}

public:
	void DeserializeHttpRequestText(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> output);
};

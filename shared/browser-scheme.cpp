#include <cctype>
#include <algorithm>
#include <include/cef_url.h>

#include "browser-scheme.hpp"
#include "browser-mime-types.h"


BrowserSchemeHandlerFactory::BrowserSchemeHandlerFactory()
{}

CefRefPtr<CefResourceHandler> BrowserSchemeHandlerFactory::Create(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString& scheme_name, CefRefPtr<CefRequest> request)
{
	(void)frame;
	(void)scheme_name;

	if (!browser || !request)
		return nullptr;

	return CefRefPtr<BrowserSchemeHandler>(new BrowserSchemeHandler());
}

BrowserSchemeHandler::BrowserSchemeHandler()
: isComplete(false), length(0), remaining(0)
{
}

// First method called
bool BrowserSchemeHandler::ProcessRequest(CefRefPtr<CefRequest> request,
		CefRefPtr<CefCallback> callback)
{
	CefURLParts parts;
	CefParseURL(request->GetURL(), parts);

	std::string path = CefString(&parts.path);
	fileName = path.substr(path.find_last_of("/") + 1);

	inputStream.open(path, std::ifstream::binary);
	if (!inputStream.is_open()) {
		callback->Cancel();
		return false;
	}

	inputStream.seekg(0, std::ifstream::end);
	length = remaining = inputStream.tellg();
	inputStream.seekg(0, std::ifstream::beg);
	callback->Continue();
	return true;
}

void BrowserSchemeHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
		int64& response_length, CefString& redirectUrl)
{
	if (!response) {
		response_length = -1;
		redirectUrl = "";
		return;
	}

	std::string fileExtension = fileName.substr(
		fileName.find_last_of(".") + 1);
	std::transform(fileExtension.begin(), fileExtension.end(),
			fileExtension.begin(), ::tolower);

	response->SetStatus(200);
	response->SetMimeType(CefGetMimeType(fileExtension));
	response->SetStatusText("OK");
	response_length = length;
	redirectUrl = "";
}

bool BrowserSchemeHandler::ReadResponse(void* data_out, int bytes_to_read,
		int& bytes_read, CefRefPtr<CefCallback> callback)
{
	(void)callback;

	if (!data_out || !inputStream.is_open()) {
		bytes_read = 0;
		inputStream.close();
		return false;
	}

	if (isComplete) {
		bytes_read = 0;
		return false;
	}

	inputStream.read((char *)data_out, bytes_to_read);
	remaining -= bytes_read = inputStream.gcount();
	if (remaining == 0) {
		isComplete = true;
		inputStream.close();
	}

	return true;
}

void BrowserSchemeHandler::Cancel()
{
	if (inputStream.is_open())
		inputStream.close();
}
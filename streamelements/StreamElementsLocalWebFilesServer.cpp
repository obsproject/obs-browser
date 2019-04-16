#include "StreamElementsLocalWebFilesServer.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"
#include <filesystem>
#include <codecvt>
#include <algorithm>
#include <obs.h>
#include "wide-string.hpp"

class StreamElementsHttpErrorCefResourceHandlerImpl : public CefResourceHandler {
public:
	StreamElementsHttpErrorCefResourceHandlerImpl(int statusCode, std::string statusText) :
		m_statusCode(statusCode),
		m_statusText(statusText)
	{
	}

	virtual bool ProcessRequest(
		CefRefPtr<CefRequest> request,
		CefRefPtr<CefCallback> callback) override
	{
		callback->Continue();

		return true;
	}

	virtual void GetResponseHeaders(
		CefRefPtr<CefResponse> response,
		int64 &response_length,
		CefString &redirectUrl) override
	{
		if (!response) {
			return;
		}

		CefResponse::HeaderMap headers;

		headers.emplace(std::make_pair<CefString, CefString>("Pragma", "no-cache"));
		headers.emplace(std::make_pair<CefString, CefString>("Cache-Control", "no-cache"));
		headers.emplace(std::make_pair<CefString, CefString>("Access-Control-Allow-Origin", "*"));
		headers.emplace(std::make_pair<CefString, CefString>("Access-Control-Allow-Methods", "GET, HEAD"));

		response->SetStatus(m_statusCode);
		response->SetStatusText(m_statusText);
		response->SetHeaderMap(headers);
	}

	virtual bool ReadResponse(
		void *data_out,
		int bytes_to_read,
		int &bytes_read,
		CefRefPtr<CefCallback> callback) override
	{
		return false;
	}

	virtual void Cancel() override
	{
	}

	IMPLEMENT_REFCOUNTING(StreamElementsHttpErrorCefResourceHandlerImpl);

private:
	int m_statusCode;
	std::string m_statusText;
};

///
// CefResourceHandler implementation to serve local files
//
class StreamElementsLocalFileCefResourceHandlerImpl : public CefResourceHandler {
public:
	StreamElementsLocalFileCefResourceHandlerImpl(std::string filePath)
	{
		m_filePath = filePath;
	}

	virtual bool ProcessRequest(
		CefRefPtr<CefRequest> request,
		CefRefPtr<CefCallback> callback) override
	{
#ifdef WIN32
		m_inputStream.open(to_wide(m_filePath), std::ifstream::binary);
#else
		inputStream.open(fileName, std::ifstream::binary);
#endif

		if (!m_inputStream.is_open()) {
			return false;
		}

		m_inputStream.seekg(0, std::ifstream::end);
		m_length = m_remaining = (int64_t)m_inputStream.tellg();
		m_inputStream.seekg(0, std::ifstream::beg);
		callback->Continue();
		return true;
	}

	virtual void GetResponseHeaders(
		CefRefPtr<CefResponse> response,
		int64 &response_length,
		CefString &redirectUrl) override
	{
		if (!response) {
			response_length = -1;
			redirectUrl = "";
			return;
		}

		std::string fileExtension =
			m_filePath.substr(m_filePath.find_last_of(".") + 1);

		for (char &ch : fileExtension)
			ch = (char)tolower(ch);
		if (fileExtension.compare("woff2") == 0)
			fileExtension = "woff";

		std::string mime = CefGetMimeType(fileExtension);

		if (fileExtension == "js") {
			mime = "text/javascript";
		}
		else if (fileExtension == "css") {
			mime = "text/css";
		}

		CefResponse::HeaderMap headers;

		headers.emplace(std::make_pair<CefString, CefString>("Pragma", "no-cache"));
		headers.emplace(std::make_pair<CefString, CefString>("Cache-Control", "no-cache"));
		headers.emplace(std::make_pair<CefString, CefString>("Access-Control-Allow-Origin", "*"));
		headers.emplace(std::make_pair<CefString, CefString>("Access-Control-Allow-Methods", "GET, HEAD"));

		response->SetStatus(200);
		response->SetStatusText("OK");
		response->SetHeaderMap(headers);
		response->SetMimeType(mime);
		response_length = m_length;
		redirectUrl = "";
	}

	virtual bool ReadResponse(
		void *data_out,
		int bytes_to_read,
		int &bytes_read,
		CefRefPtr<CefCallback> callback) override
	{
		if (!data_out || !m_inputStream.is_open()) {
			bytes_read = 0;
			m_inputStream.close();
			return false;
		}

		if (m_isComplete) {
			bytes_read = 0;
			return false;
		}

		m_inputStream.read((char *)data_out, bytes_to_read);
		bytes_read = (int64_t)m_inputStream.gcount();
		m_remaining -= bytes_read;

		if (m_remaining == 0) {
			m_isComplete = true;
			m_inputStream.close();
		}

		return true;
	}

	virtual void Cancel() override
	{
		if (m_inputStream.is_open()) {
			m_inputStream.close();
		}
	}

	IMPLEMENT_REFCOUNTING(StreamElementsLocalFileCefResourceHandlerImpl);

private:
	std::string m_filePath;
	std::ifstream m_inputStream;
	bool m_isComplete = false;
	int64_t m_length = 0;
	int64_t m_remaining = 0;
};

StreamElementsLocalWebFilesServer::StreamElementsLocalWebFilesServer(std::string rootFolder) :
	m_rootFolder(rootFolder)
{
	if (!std::experimental::filesystem::is_directory(m_rootFolder)) {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsLocalWebFilesServer: folder does not exist: %s",
			m_rootFolder.c_str());

		return;
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	for (auto& p : std::experimental::filesystem::directory_iterator(m_rootFolder)) {
		std::string path = myconv.to_bytes(p.path().c_str());

		if (std::experimental::filesystem::is_directory(path)) {
			std::string host = path;

			// extract hostname part of the path
			size_t pos = host.find_last_of('/');
			if (pos != std::string::npos) {
				host = host.substr(pos + 1);
			}
			pos = host.find_last_of('\\');
			if (pos != std::string::npos) {
				host = host.substr(pos + 1);
			}

			// transform host to lower-case
			std::transform(host.begin(), host.end(), host.begin(), ::tolower);

			// add to map
			m_hostsMap[host] =
				std::make_shared<StreamElementsFileSystemMapper>(path);

			blog(LOG_INFO,
				"obs-browser: StreamElementsLocalWebFilesServer: added mapping between '%s' and '%s'.",
				host.c_str(),
				path.c_str());
		}
	}
}

StreamElementsLocalWebFilesServer::~StreamElementsLocalWebFilesServer()
{
}

bool StreamElementsLocalWebFilesServer::HasHost(std::string host)
{
	// transform host to lower-case
	std::transform(host.begin(), host.end(), host.begin(), ::tolower);

	return m_hostsMap.count(host);
}

bool StreamElementsLocalWebFilesServer::MapRequestPath(std::string host, std::string relative, std::string& absolute_path)
{
	//////////////////////////////////////////////////////////////////////
	// Security consideration:
	//
	// The value of <host> may arrive as '..', however,
	// since '..' key does not exist in <m_hostsMap>, this does not pose
	// a security risk.
	//
	// Nevertheless, we will check the value of <host> for bad input.
	//////////////////////////////////////////////////////////////////////

	if (host == "..") {
		return false;
	}

	// transform host to lower-case
	std::transform(host.begin(), host.end(), host.begin(), ::tolower);

	if (!m_hostsMap.count(host)) {
		return false;
	}

	return m_hostsMap[host]->MapAbsolutePath(relative, absolute_path);
}

CefRefPtr<CefResourceHandler> StreamElementsLocalWebFilesServer::GetCefResourceHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request)
{
	//////////////////////////////////////////////////////////////////////
	// Examine request
	//////////////////////////////////////////////////////////////////////

	std::string method = request->GetMethod().ToString();

	if (method != "GET" && method != "HEAD") {
		return nullptr;
	}

	CefURLParts parts;
	CefParseURL(request->GetURL(), parts);

	std::string scheme = CefString(&parts.scheme);
	if (scheme != "http" && scheme != "https") {
		return nullptr;
	}

	std::string host = CefString(&parts.host);
	if (host == "absolute") {
		// Handle absolute path
		std::string path;
		if (VerifySessionSignedAbsolutePathURL(
			request->GetURL().ToString(), path)) {
			return new StreamElementsLocalFileCefResourceHandlerImpl(path);
		}
		else {
			return new StreamElementsHttpErrorCefResourceHandlerImpl(403, "Forbidden");
		}
	}
	else {
		// Local overrides for remote HTTP/HTTPS host requests

		std::string path = CefString(&parts.path);
		path = CefURIDecode(path, true, cef_uri_unescape_rule_t::UU_SPACES);
		path = CefURIDecode(path, true, cef_uri_unescape_rule_t::UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

		std::string absolute_path;

		bool isMapped = MapRequestPath(host, path, absolute_path);

		if (isMapped) {
			// Serve mapped local file
			return new StreamElementsLocalFileCefResourceHandlerImpl(absolute_path);
		}
		else if (HasHost(host)) {
			blog(LOG_INFO,
				"obs-browser: StreamElementsLocalWebFilesServer: host '%s' is mapped to local folder, but no local file exists for '%s'",
				host.c_str(),
				path.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////
	// Use default request handler
	//////////////////////////////////////////////////////////////////////

	return nullptr;
}

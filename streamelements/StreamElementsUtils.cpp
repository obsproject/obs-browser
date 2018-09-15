#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"
#include "Version.hpp"

#include <cstdint>
#include <codecvt>

#include <curl/curl.h>

#include <obs-frontend-api.h>

#include <QUrl>

#include <QFile>
#include <QDir>
#include <QUrl>
#include <regex>

/* ========================================================= */

// convert wstring to UTF-8 string
static std::string wstring_to_utf8(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

static std::vector<std::string> tokenizeString(const std::string& str, const std::string& delimiters)
{
	std::vector<std::string> tokens;
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos)
	{  // Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
	return tokens;
}

/* ========================================================= */

void QtPostTask(void(*func)(void*), void* const data)
{
	/*
	if (QThread::currentThread() == qApp->thread()) {
	func(data);
	}
	else {*/
	QTimer *t = new QTimer();
	t->moveToThread(qApp->thread());
	t->setSingleShot(true);
	QObject::connect(t, &QTimer::timeout, [=]() {
		t->deleteLater();

		func(data);
	});
	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));
	/*}*/
}

void QtExecSync(void(*func)(void*), void* const data)
{
	if (QThread::currentThread() == qApp->thread()) {
		func(data);
	}
	else {
		os_event_t* completeEvent;

		os_event_init(&completeEvent, OS_EVENT_TYPE_AUTO);

		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			t->deleteLater();

			func(data);

			os_event_signal(completeEvent);
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection, Q_ARG(int, 0));

		QApplication::sendPostedEvents();

		os_event_wait(completeEvent);
		os_event_destroy(completeEvent);
	}
}

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area)
{
	switch (area)
	{
	case Qt::LeftDockWidgetArea:
		return "left";
	case Qt::RightDockWidgetArea:
		return "right";
	case Qt::TopDockWidgetArea:
		return "top";
	case Qt::BottomDockWidgetArea:
		return "bottom";
	case Qt::NoDockWidgetArea:
	default:
		return "floating";
	}
}

std::string GetCommandLineOptionValue(const std::string key)
{
	QStringList args = QCoreApplication::instance()->arguments();

	std::string search = "--" + key + "=";

	for (int i = 0; i < args.size(); ++i) {
		std::string arg = args.at(i).toStdString();

		if (arg.substr(0, search.size()) == search) {
			return arg.substr(search.size());
		}
	}

	return std::string();
}

std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
}

/* ========================================================= */

static uint64_t FromFileTime(const FILETIME& ft) {
	ULARGE_INTEGER uli = { 0 };
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}

void SerializeSystemTimes(CefRefPtr<CefValue>& output)
{
	SYNC_ACCESS();

	output->SetNull();

	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;

#ifdef _WIN32
	if (::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		static bool hasSavedStartingValues = false;
		static uint64_t savedIdleTime;
		static uint64_t savedKernelTime;
		static uint64_t savedUserTime;

		if (!hasSavedStartingValues) {
			savedIdleTime = FromFileTime(idleTime);
			savedKernelTime = FromFileTime(kernelTime);
			savedUserTime = FromFileTime(userTime);

			hasSavedStartingValues = true;
		}

		const uint64_t SECOND_PART = 10000000L;
		const uint64_t MS_PART = SECOND_PART / 1000L;

		uint64_t idleInt = FromFileTime(idleTime) - savedIdleTime;
		uint64_t kernelInt = FromFileTime(kernelTime) - savedKernelTime;
		uint64_t userInt = FromFileTime(userTime) - savedUserTime;

		uint64_t idleMs = idleInt / MS_PART;
		uint64_t kernelMs = kernelInt / MS_PART;
		uint64_t userMs = userInt / MS_PART;

		uint64_t idleSec = idleMs / (uint64_t)1000;
		uint64_t kernelSec = kernelMs / (uint64_t)1000;
		uint64_t userSec = userMs / (uint64_t)1000;

		uint64_t idleMod = idleMs % (uint64_t)1000;
		uint64_t kernelMod = kernelMs % (uint64_t)1000;
		uint64_t userMod = userMs % (uint64_t)1000;

		double idleRat = idleSec + ((double)idleMod / 1000.0);
		double kernelRat = kernelSec + ((double)kernelMod / 1000.0);
		double userRat = userSec + ((double)userMod / 1000.0);

		// https://msdn.microsoft.com/en-us/84f674e7-536b-4ae0-b523-6a17cb0a1c17
		// lpKernelTime [out, optional]
		// A pointer to a FILETIME structure that receives the amount of time that
		// the system has spent executing in Kernel mode (including all threads in
		// all processes, on all processors)
		//
		// >>> This time value also includes the amount of time the system has been idle.
		//

		d->SetDouble("idleSeconds", idleRat);
		d->SetDouble("kernelSeconds", kernelRat - idleRat);
		d->SetDouble("userSeconds", userRat);
		d->SetDouble("totalSeconds", kernelRat + userRat);
		d->SetDouble("busySeconds", kernelRat + userRat - idleRat);
	}
#endif
}

void SerializeSystemMemoryUsage(CefRefPtr<CefValue>& output)
{
	output->SetNull();

#ifdef _WIN32
	MEMORYSTATUSEX mem;

	mem.dwLength = sizeof(mem);

	if (GlobalMemoryStatusEx(&mem)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		const DWORDLONG DIV = 1048576;

		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage", mem.dwMemoryLoad);
		d->SetInt("totalPhysicalMemory", mem.ullTotalPhys / DIV);
		d->SetInt("freePhysicalMemory", mem.ullAvailPhys / DIV);
		d->SetInt("totalVirtualMemory", mem.ullTotalVirtual / DIV);
		d->SetInt("freeVirtualMemory", mem.ullAvailVirtual / DIV);
		d->SetInt("freeExtendedVirtualMemory", mem.ullAvailExtendedVirtual / DIV);
		d->SetInt("totalPageFileSize", mem.ullTotalPageFile/ DIV);
		d->SetInt("freePageFileSize", mem.ullAvailPageFile/ DIV);
	}
#endif
}

static CefString getRegStr(HKEY parent, const WCHAR* subkey, const WCHAR* key)
{
	CefString result;

	DWORD dataSize = 0;

	if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key, RRF_RT_ANY, NULL, NULL, &dataSize)) {
		WCHAR* buffer = new WCHAR[dataSize];

		if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key, RRF_RT_ANY, NULL, buffer, &dataSize)) {
			result = buffer;
		}

		delete[] buffer;
	}

	return result;
};

static DWORD getRegDWORD(HKEY parent, const WCHAR* subkey, const WCHAR* key)
{
	DWORD result = 0;
	DWORD dataSize = sizeof(DWORD);

	::RegGetValueW(parent, subkey, key, RRF_RT_DWORD, NULL, &result, &dataSize);

	return result;
}

void SerializeSystemHardwareProperties(CefRefPtr<CefValue>& output)
{
	output->SetNull();

#ifdef _WIN32
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
	output->SetDictionary(d);

	SYSTEM_INFO info;

	::GetNativeSystemInfo(&info);

	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL: d->SetString("cpuArch", "x86"); break;
	case PROCESSOR_ARCHITECTURE_IA64: d->SetString("cpuArch", "IA64"); break;
	case PROCESSOR_ARCHITECTURE_AMD64: d->SetString("cpuArch", "x64"); break;
	case PROCESSOR_ARCHITECTURE_ARM: d->SetString("cpuArch", "ARM"); break;
	case PROCESSOR_ARCHITECTURE_ARM64: d->SetString("cpuArch", "ARM64"); break;

	default:
	case PROCESSOR_ARCHITECTURE_UNKNOWN: d->SetString("cpuArch", "Unknown"); break;
	}

	d->SetInt("cpuCount", info.dwNumberOfProcessors);
	d->SetInt("cpuLevel", info.wProcessorLevel);

	/*
	CefRefPtr<CefDictionaryValue> f = CefDictionaryValue::Create();

	f->SetBool("PF_3DNOW_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_3DNOW_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_CHANNELS_ENABLED", ::IsProcessorFeaturePresent(PF_CHANNELS_ENABLED));
	f->SetBool("PF_COMPARE_EXCHANGE_DOUBLE", ::IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE_DOUBLE));
	f->SetBool("PF_COMPARE_EXCHANGE128", ::IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128));
	f->SetBool("PF_COMPARE64_EXCHANGE128", ::IsProcessorFeaturePresent(PF_COMPARE64_EXCHANGE128));
	f->SetBool("PF_FASTFAIL_AVAILABLE", ::IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE));
	f->SetBool("PF_FLOATING_POINT_EMULATED", ::IsProcessorFeaturePresent(PF_FLOATING_POINT_EMULATED));
	f->SetBool("PF_FLOATING_POINT_PRECISION_ERRATA", ::IsProcessorFeaturePresent(PF_FLOATING_POINT_PRECISION_ERRATA));
	f->SetBool("PF_MMX_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_NX_ENABLED", ::IsProcessorFeaturePresent(PF_NX_ENABLED));
	f->SetBool("PF_PAE_ENABLED", ::IsProcessorFeaturePresent(PF_PAE_ENABLED));
	f->SetBool("PF_RDTSC_INSTRUCTION_AVAILABLE", ::IsProcessorFeaturePresent(PF_RDTSC_INSTRUCTION_AVAILABLE));
	f->SetBool("PF_RDWRFSGSBASE_AVAILABLE", ::IsProcessorFeaturePresent(PF_RDWRFSGSBASE_AVAILABLE));
	f->SetBool("PF_SECOND_LEVEL_ADDRESS_TRANSLATION", ::IsProcessorFeaturePresent(PF_SECOND_LEVEL_ADDRESS_TRANSLATION));
	f->SetBool("PF_SSE3_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_VIRT_FIRMWARE_ENABLED", ::IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED));
	f->SetBool("PF_XMMI_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_XMMI64_INSTRUCTIONS_AVAILABLE", ::IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE));
	f->SetBool("PF_XSAVE_ENABLED", ::IsProcessorFeaturePresent(PF_XSAVE_ENABLED));

	CefRefPtr<CefValue> featuresValue = CefValue::Create();
	featuresValue->SetDictionary(f);
	d->SetValue("cpuFeatures", featuresValue);*/


	{
		CefRefPtr<CefListValue> cpuList = CefListValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS == ::RegOpenKeyA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor", &hRoot)) {
			WCHAR cpuKeyBuffer[2048];

			for (DWORD index = 0; ERROR_SUCCESS == ::RegEnumKeyW(hRoot, index, cpuKeyBuffer, sizeof(cpuKeyBuffer)); ++index) {
				CefRefPtr<CefDictionaryValue> p = CefDictionaryValue::Create();

				p->SetString("name", getRegStr(hRoot, cpuKeyBuffer, L"ProcessorNameString"));
				p->SetString("vendor", getRegStr(hRoot, cpuKeyBuffer, L"VendorIdentifier"));
				p->SetInt("speedMHz", getRegDWORD(hRoot, cpuKeyBuffer, L"~MHz"));
				p->SetString("identifier", getRegStr(hRoot, cpuKeyBuffer, L"Identifier"));

				cpuList->SetDictionary(cpuList->GetSize(), p);
			}

			::RegCloseKey(hRoot);
		}

		d->SetList("cpuHardware", cpuList);
	}

	{
		CefRefPtr<CefDictionaryValue> bios = CefDictionaryValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS == ::RegOpenKeyW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System", &hRoot)) {
			HKEY hBios;
			if (ERROR_SUCCESS == ::RegOpenKeyW(hRoot, L"BIOS", &hBios)) {
				WCHAR subKeyBuffer[2048];
				DWORD bufSize = sizeof(subKeyBuffer) / sizeof(subKeyBuffer[0]);

				DWORD valueIndex = 0;
				DWORD valueType = 0;

				LSTATUS callStatus = ::RegEnumValueW(hBios, valueIndex, subKeyBuffer, &bufSize, NULL, &valueType, NULL, NULL);
				while (ERROR_NO_MORE_ITEMS != callStatus) {
					switch (valueType) {
					case REG_DWORD_BIG_ENDIAN:
					case REG_DWORD_LITTLE_ENDIAN:
						bios->SetInt(subKeyBuffer, getRegDWORD(hRoot, L"BIOS", subKeyBuffer));
						break;

					case REG_QWORD:
						bios->SetInt(subKeyBuffer, getRegDWORD(hRoot, L"BIOS", subKeyBuffer));
						break;

					case REG_SZ:
					case REG_EXPAND_SZ:
					case REG_MULTI_SZ:
						bios->SetString(subKeyBuffer, getRegStr(hRoot, L"BIOS", subKeyBuffer));
						break;
					}

					++valueIndex;

					bufSize = sizeof(subKeyBuffer) / sizeof(subKeyBuffer[0]);
					callStatus = ::RegEnumValueW(hBios, valueIndex, subKeyBuffer, &bufSize, NULL, &valueType, NULL, NULL);
				}

				::RegCloseKey(hBios);
			}

			::RegCloseKey(hRoot);
		}

		d->SetDictionary("bios", bios);
	}
#endif
}

/* ========================================================= */

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue>& output)
{
	// Response codec collection (array)
	CefRefPtr<CefListValue> list = CefListValue::Create();

	// Response codec collection is our root object
	output->SetList(list);

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
}

std::string SerializeAppStyleSheet()
{
	std::string result = qApp->styleSheet().toStdString();

	if (result.compare(0, 8, "file:///") == 0) {
		QUrl url(result.c_str());

		if (url.isLocalFile()) {
			result = result.substr(8);

			QFile file(result.c_str());

			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QTextStream in(&file);

				result = in.readAll().toStdString();
			}
		}
	}

	return result;
}

std::string GetAppStyleSheetSelectorContent(std::string selector)
{
	std::string result;

	std::string css = SerializeAppStyleSheet();

	std::replace(css.begin(), css.end(), '\n', ' ');

	css = std::regex_replace(css, std::regex("/\\*.*?\\*/"), "");

	std::regex selector_regex("[^\\s]*" + selector + "[\\s]*\\{(.*?)\\}");
	std::smatch selector_match;

	if (std::regex_search(css, selector_match, selector_regex)) {
		result = std::string(selector_match[1].first, selector_match[1].second);
	}

	return result;
}

std::string GetCurrentThemeName()
{
	std::string result;

	config_t* globalConfig = obs_frontend_get_global_config(); // does not increase refcount

	const char *themeName = config_get_string(globalConfig, "General", "CurrentTheme");
	if (!themeName) {
		/* Use deprecated "Theme" value if available */
		themeName = config_get_string(globalConfig, "General", "Theme");
		if (!themeName) {
			themeName = "Default";
		}

		result = themeName;
	}

	std::string appStyle = qApp->styleSheet().toStdString();

	if (appStyle.substr(0, 7) == "file://") {
		QUrl url(appStyle.c_str());

		if (url.isLocalFile()) {
			result = url.fileName().split('.')[0].toStdString();
		}
	}

	return result;
}

std::string GetCefVersionString()
{
	char buf[64];

	sprintf(buf,
		"cef.%d.%d.chrome.%d.%d.%d.%d",
		cef_version_info(0),
		cef_version_info(1),
		cef_version_info(2),
		cef_version_info(3),
		cef_version_info(4),
		cef_version_info(5));

	return std::string(buf);
}

std::string GetCefPlatformApiHash()
{
	return cef_api_hash(0);
}

std::string GetCefUniversalApiHash()
{
	return cef_api_hash(1);
}

std::string GetStreamElementsPluginVersionString()
{
	char version_buf[64];
	sprintf(version_buf, "%d.%d.%d.%d",
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 1000000000000L) / 10000000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 10000000000L) / 100000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 100000000L) / 1000000L),
		(int)(STREAMELEMENTS_PLUGIN_VERSION % 1000000L));

	return version_buf;
}

std::string GetStreamElementsApiVersionString()
{
	char version_buf[64];

	sprintf(version_buf, "%d.%d", HOST_API_VERSION_MAJOR, HOST_API_VERSION_MINOR);

	return version_buf;
}

/* ========================================================= */

#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")
void SetGlobalCURLOptions(CURL* curl, const char* url)
{
	std::string proxy = GetCommandLineOptionValue("streamelements-http-proxy");

	if (!proxy.size()) {
#ifdef _WIN32
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config;

		if (WinHttpGetIEProxyConfigForCurrentUser(&config)) {
			// http=127.0.0.1:8888;https=127.0.0.1:8888
			if (config.lpszProxy) {
				proxy = wstring_to_utf8(config.lpszProxy);

				std::map<std::string, std::string> schemes;
				for (auto kvstr : tokenizeString(proxy, ";")) {
					std::vector<std::string> kv = tokenizeString(kvstr, "=");

					if (kv.size() == 2) {
						std::transform(kv[0].begin(), kv[0].end(), kv[0].begin(), tolower);
						schemes[kv[0]] = kv[1];
					}
				}

				std::string scheme = tokenizeString(url, ":")[0];
				std::transform(scheme.begin(), scheme.end(), scheme.begin(), tolower);

				if (schemes.count(scheme)) {
					proxy = schemes[scheme];
				}
				else if (schemes.count("http")) {
					proxy = schemes["http"];
				}
				else {
					proxy = "";
				}
			}

			if (config.lpszProxy) {
				GlobalFree((HGLOBAL)config.lpszProxy);
			}

			if (config.lpszProxyBypass) {
				GlobalFree((HGLOBAL)config.lpszProxyBypass);
			}

			if (config.lpszAutoConfigUrl) {
				GlobalFree((HGLOBAL)config.lpszAutoConfigUrl);
			}
		}
#endif
	}

	if (proxy.size()) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
	}
}

struct http_callback_context
{
	http_client_callback_t callback;
	void* userdata;
};
static size_t http_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	http_callback_context* context = (http_callback_context*)userdata;

	bool result = true;
	if (context->callback) {
		result = context->callback(ptr, size * nmemb, context->userdata);
	}

	if (result) {
		return size * nmemb;
	}
	else {
		return 0;
	}
};

bool HttpGet(const char* url, http_client_callback_t callback, void* userdata)
{
	bool result = false;

	CURL* curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		http_callback_context context;
		context.callback = callback;
		context.userdata = userdata;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

		CURLcode res = curl_easy_perform(curl);

		if (CURLE_OK == res) {
			result = true;
		}

		curl_easy_cleanup(curl);
	}

	return result;
}

bool HttpPost(const char* url, const char* contentType, void* buffer, size_t buffer_len, http_client_callback_t callback, void* userdata)
{
	bool result = false;

	CURL* curl = curl_easy_init();

	if (curl) {
		SetGlobalCURLOptions(curl, url);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		http_callback_context context;
		context.callback = callback;
		context.userdata = userdata;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)buffer_len);
		curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, buffer);

		curl_slist *headers = NULL;
		headers = curl_slist_append(headers, (std::string("Content-Type: ") + contentType).c_str());

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		CURLcode res = curl_easy_perform(curl);

		if (CURLE_OK == res) {
			result = true;
		}

		curl_easy_cleanup(curl);
	}

	return result;
}

/* ========================================================= */

std::string CreateGloballyUniqueIdString()
{
	std::string result;

	const int GUID_STRING_LENGTH = 39;

	GUID guid;
	CoCreateGuid(&guid);

	OLECHAR guidStr[GUID_STRING_LENGTH];
	StringFromGUID2(guid, guidStr, GUID_STRING_LENGTH);

	guidStr[GUID_STRING_LENGTH - 2] = 0;
	result = wstring_to_utf8(guidStr + 1);

	return result;
}

#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
std::string GetComputerSystemUniqueId()
{
	std::string result = "";

	const char* REG_KEY_PATH = "SOFTWARE\\StreamElements";
	const char* REG_VALUE_NAME = "MachineUniqueIdentifier";

	HKEY hkeyRoot;
	if (ERROR_SUCCESS == RegCreateKeyA(HKEY_LOCAL_MACHINE, REG_KEY_PATH, &hkeyRoot)) {
		RegCloseKey(hkeyRoot);
	}

	char buf[128];
	DWORD bufLen = 128;

	if (ERROR_SUCCESS == RegGetValueA(
		HKEY_LOCAL_MACHINE,
		REG_KEY_PATH,
		REG_VALUE_NAME,
		RRF_RT_REG_SZ,
		NULL,
		buf,
		&bufLen)) {
		result = buf;
	}
	else {
		// Get unique ID from WMI

		HRESULT hr = CoInitialize(NULL);

		// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/initializing-com-for-a-wmi-application
		if (SUCCEEDED(hr)) {
			bool uinitializeCom = hr == S_OK;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/setting-the-default-process-security-level-using-c-
			CoInitializeSecurity(
				NULL,                       // security descriptor
				-1,                          // use this simple setting
				NULL,                        // use this simple setting
				NULL,                        // reserved
				RPC_C_AUTHN_LEVEL_DEFAULT,   // authentication level  
				RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
				NULL,                        // use this simple setting
				EOAC_NONE,                   // no special capabilities
				NULL);                          // reserved

			IWbemLocator *pLocator;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
			hr = CoCreateInstance(
				CLSID_WbemLocator, 0,
				CLSCTX_INPROC_SERVER,
				IID_IWbemLocator,
				(LPVOID*)&pLocator);

			if (SUCCEEDED(hr)) {
				IWbemServices *pSvc = 0;

				// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
				hr = pLocator->ConnectServer(
					BSTR(L"root\\cimv2"),  //namespace
					NULL,       // User name 
					NULL,       // User password
					0,         // Locale 
					NULL,     // Security flags
					0,         // Authority 
					0,        // Context object 
					&pSvc);   // IWbemServices proxy

				if (SUCCEEDED(hr)) {
					hr = CoSetProxyBlanket(pSvc,
						RPC_C_AUTHN_WINNT,
						RPC_C_AUTHZ_NONE,
						NULL,
						RPC_C_AUTHN_LEVEL_CALL,
						RPC_C_IMP_LEVEL_IMPERSONATE,
						NULL,
						EOAC_NONE
					);

					if (SUCCEEDED(hr)) {
						IEnumWbemClassObject *pEnumerator = NULL;

						hr = pSvc->ExecQuery(
							(BSTR)L"WQL",
							(BSTR)L"select * from Win32_ComputerSystemProduct",
							WBEM_FLAG_FORWARD_ONLY,
							NULL,
							&pEnumerator);

						if (SUCCEEDED(hr)) {
							IWbemClassObject *pObj = NULL;

							ULONG resultCount;
							hr = pEnumerator->Next(
								WBEM_INFINITE,
								1,
								&pObj,
								&resultCount);

							if (SUCCEEDED(hr)) {
								VARIANT value;

								hr = pObj->Get(L"UUID", 0, &value, NULL, NULL);

								if (SUCCEEDED(hr)) {
									if (value.vt != VT_NULL) {
										result = std::string("WUID/") + wstring_to_utf8(std::wstring(value.bstrVal));
									}
									VariantClear(&value);
								}
							}

							pEnumerator->Release();
						}
					}

					pSvc->Release();
				}

				pLocator->Release();
			}

			if (uinitializeCom) {
				CoUninitialize();
			}
		}
	}

	if (!result.size()) {
		// Failed retrieving UUID, generate our own
		result = std::string("SEID/") + CreateGloballyUniqueIdString();
	}

	// Save for future use
	RegSetKeyValueA(
		HKEY_LOCAL_MACHINE,
		REG_KEY_PATH,
		REG_VALUE_NAME,
		REG_SZ,
		result.c_str(),
		result.size());

	return result;
}


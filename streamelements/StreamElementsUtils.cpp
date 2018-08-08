#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"

#include <cstdint>

#include <obs-frontend-api.h>

#include <QUrl>

#include <QFile>
#include <QDir>
#include <QUrl>
#include <regex>

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

#include "StreamElementsUtils.hpp"

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
		static ULARGE_INTEGER savedIdleTime;
		static ULARGE_INTEGER savedKernelTime;
		static ULARGE_INTEGER savedUserTime;

		if (!hasSavedStartingValues) {
			savedIdleTime.HighPart = idleTime.dwHighDateTime;
			savedIdleTime.LowPart = idleTime.dwLowDateTime;

			savedKernelTime.HighPart = kernelTime.dwHighDateTime;
			savedKernelTime.LowPart = kernelTime.dwLowDateTime;

			savedUserTime.HighPart = userTime.dwHighDateTime;
			savedUserTime.LowPart = userTime.dwLowDateTime;

			hasSavedStartingValues = true;
		}

		const unsigned long SECOND_PART = 10000000L;
		const unsigned long MS_PART = SECOND_PART / 1000L;

		ULARGE_INTEGER idleInt;
		idleInt.HighPart = idleTime.dwHighDateTime;
		idleInt.LowPart = idleTime.dwLowDateTime;
		idleInt.QuadPart -= savedIdleTime.QuadPart;

		ULARGE_INTEGER kernelInt;
		kernelInt.HighPart = kernelTime.dwHighDateTime;
		kernelInt.LowPart = kernelTime.dwLowDateTime;
		kernelInt.QuadPart -= savedKernelTime.QuadPart;

		ULARGE_INTEGER userInt;
		userInt.HighPart = userTime.dwHighDateTime;
		userInt.LowPart = userTime.dwLowDateTime;
		userInt.QuadPart -= savedUserTime.QuadPart;

		long idleMs = idleInt.QuadPart / MS_PART;
		long kernelMs = kernelInt.QuadPart / MS_PART;
		long userMs = userInt.QuadPart / MS_PART;

		/*
		int idleSec = (int)(idleMs / 1000L);
		int kernelSec = (int)(kernelMs / 1000L);
		int userSec = (int)(userMs / 1000L);
		*/

		double idleSec = (double)idleMs / (double)1000.0;
		double kernelSec = (double)kernelMs / (double)1000.0;
		double userSec = (double)userMs / (double)1000.0;

		/*
		d->SetInt("idleSeconds", idleSec);
		d->SetInt("kernelSeconds", kernelSec);
		d->SetInt("userSeconds", userSec);
		d->SetInt("totalSeconds", idleSec + kernelSec + userSec);
		*/

		d->SetDouble("idleSeconds", idleSec);
		d->SetDouble("kernelSeconds", kernelSec);
		d->SetDouble("userSeconds", userSec);
		d->SetDouble("totalSeconds", idleSec + kernelSec + userSec);
		d->SetDouble("busySeconds", kernelSec + userSec);
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

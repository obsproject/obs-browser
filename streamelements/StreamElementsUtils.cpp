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

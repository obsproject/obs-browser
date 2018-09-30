#include "StreamElementsCrashHandler.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include <util/base.h>
#include <util/platform.h>
#include <util/config-file.h>
#include <obs-frontend-api.h>
#include <time.h>
#include <cstdio>
#include <string>
#include <ios>
#include <fstream>
#include <codecvt>
#include <windows.h>
#include <winuser.h>

static void null_crash_handler(const char *format, va_list args,
	void *param)
{
	exit(0);

	UNUSED_PARAMETER(param);
}

static std::string GenerateTimeDateFilename(const char *extension, bool noSpace = false)
{
	time_t    now = time(0);
	char      file[256] = {};
	struct tm *cur_time;

	cur_time = localtime(&now);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
		cur_time->tm_year + 1900,
		cur_time->tm_mon + 1,
		cur_time->tm_mday,
		noSpace ? '_' : ' ',
		cur_time->tm_hour,
		cur_time->tm_min,
		cur_time->tm_sec,
		extension);

	return std::string(file);
}

static void delete_oldest_file(bool has_prefix, const char *location)
{
	std::string      logDir(os_get_config_path_ptr(location));
	std::string      oldestLog;
	time_t	         oldest_ts = (time_t)-1;
	struct os_dirent *entry;

	unsigned int maxLogs = (unsigned int)config_get_uint(
		obs_frontend_get_global_config(), "General", "MaxLogs");

	os_dir_t *dir = os_opendir(logDir.c_str());
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			std::string filePath = logDir + "/" + std::string(entry->d_name);
			struct stat st;
			if (0 == os_stat(filePath.c_str(), &st)) {
				time_t ts = st.st_ctime;

				if (ts) {
					if (ts < oldest_ts) {
						oldestLog = filePath;
						oldest_ts = ts;
					}

					count++;
				}
			}
		}

		os_closedir(dir);

		if (count > maxLogs) {
			os_unlink(oldestLog.c_str());
		}
	}
}

#define MAX_CRASH_REPORT_SIZE (300 * 1024)

#define CRASH_MESSAGE \
	"Woops, OBS has crashed!\n\nWould you like to copy the crash log " \
	"to the clipboard?  (Crash logs will still be saved to the " \
	"%appdata%\\obs-studio\\crashes directory)"

static void write_file_content(std::string& path, const char* content)
{
	std::fstream file;

#ifdef _WIN32
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	std::wstring wpath = myconv.from_bytes(path);

	file.open(wpath, std::ios_base::in | std::ios_base::out |
		std::ios_base::trunc | std::ios_base::binary);
#else
	file.open(path, std::ios_base::in | std::ios_base::out |
		std::ios_base::trunc | std::ios_base::binary);
#endif
	file << content;

	file.close();
}

///
// StreamElements Crash Handler
//
// Don't use any asynchronous calls here
// Don't use stdio FILE* here
//
// Repeats crash handler functionality found in obs-app.cpp
//
// This is because there is no way to chain two crash handlers
// together at the moment of this writing.
//
static void main_crash_handler(const char *format, va_list args, void *param)
{
	// Allocate space for crash report content
	char* text = new char[MAX_CRASH_REPORT_SIZE];

	// Build crash report
	vsnprintf(text, MAX_CRASH_REPORT_SIZE, format, args);
	text[MAX_CRASH_REPORT_SIZE - 1] = 0;

	// Delete oldest crash report
	delete_oldest_file(true, "obs-studio/crashes");

	// Build output file path
	std::string name = "obs-studio/crashes/Crash ";
	name += GenerateTimeDateFilename("txt");

	std::string path(os_get_config_path_ptr(name.c_str()));

	// Write crash report content to crash dump file
	write_file_content(path, text);

	// Send event report to analytics service.
	StreamElementsGlobalStateManager::GetInstance()->GetAnalyticsEventsManager()->trackSynchronousEvent(
		"OBS Studio Crashed",
		json11::Json::object{
			{ "crashReportText", text }
		}
	);

	int ret = MessageBoxA(NULL, CRASH_MESSAGE, "OBS has crashed!",
		MB_YESNO | MB_ICONERROR | MB_TASKMODAL);

	if (ret == IDYES) {
		size_t len = strlen(text);

		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len);
		memcpy(GlobalLock(mem), text, len);
		GlobalUnlock(mem);

		OpenClipboard(0);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
	}

	exit(-1);

	UNUSED_PARAMETER(param);
}

StreamElementsCrashHandler::StreamElementsCrashHandler()
{
	base_set_crash_handler(main_crash_handler, this);
}

StreamElementsCrashHandler::~StreamElementsCrashHandler()
{
	base_set_crash_handler(null_crash_handler, nullptr);
}

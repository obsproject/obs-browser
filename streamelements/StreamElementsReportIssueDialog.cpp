#include "StreamElementsReportIssueDialog.hpp"
#include "StreamElementsProgressDialog.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsNetworkDialog.hpp"
#include "StreamElementsConfig.hpp"
#include "Version.hpp"
#include "ui_StreamElementsReportIssueDialog.h"

#include "deps/zip/zip.h"

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include "cef-headers.hpp"

#include <windows.h>

#include <thread>
#include <iostream>
#include <filesystem>
#include <stdio.h>

#include <QMessageBox>

#pragma optimize("", off)
static double GetCpuCoreBenchmark(const uint64_t CPU_BENCH_TOTAL, uint64_t& cpu_bench_delta)
{
	uint64_t cpu_bench_begin = os_gettime_ns();
	//const uint64_t CPU_BENCH_TOTAL = 10000000;
	uint64_t cpu_bench_accumulator = 2L;

	for (uint64_t bench = 0; bench < CPU_BENCH_TOTAL; ++bench) {
		cpu_bench_accumulator *= cpu_bench_accumulator;
		//if (dialog.cancelled()) {
		//	goto cancelled;
		//}
	}

	uint64_t cpu_bench_end = os_gettime_ns();
	cpu_bench_delta = cpu_bench_end - cpu_bench_begin;
	double cpu_benchmark = (double)CPU_BENCH_TOTAL / (double)cpu_bench_delta;

	return cpu_benchmark * (double)100.0;
}
#pragma optimize("", on)

StreamElementsReportIssueDialog::StreamElementsReportIssueDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StreamElementsReportIssueDialog)
{
    ui->setupUi(this);

    ui->txtIssue->setTabChangesFocus(true);
    ui->txtIssue->setWordWrapMode(QTextOption::WordWrap);

    // Disable context help button
    this->setWindowFlags((
	    (windowFlags() | Qt::CustomizeWindowHint)
	    & ~Qt::WindowContextHelpButtonHint
	    ));
}

StreamElementsReportIssueDialog::~StreamElementsReportIssueDialog()
{
    delete ui;
}

void StreamElementsReportIssueDialog::update()
{
	QDialog::update();

	ui->cmdOK->setEnabled(ui->txtIssue->toPlainText().trimmed().size());
}

void StreamElementsReportIssueDialog::accept()
{
	auto wstring_to_utf8 = [](const std::wstring& str) -> std::string
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
		return myconv.to_bytes(str);
	};

	std::string tempBufPath;

	ui->txtIssue->setEnabled(false);
	ui->cmdOK->setEnabled(false);

	obs_frontend_push_ui_translation(obs_module_get_string);

	StreamElementsProgressDialog dialog(this);

	obs_frontend_pop_ui_translation();

	dialog.setEnableCancel(false);

	std::thread thread([&]() {
		bool collect_all = ui->checkCollectLogsAndSettings->isChecked();
		std::string descriptionText = wstring_to_utf8(
			ui->txtIssue->toPlainText().trimmed().toStdWString());

		const size_t BUF_LEN = 2048;
		wchar_t pathBuffer[BUF_LEN];

		if (!::GetTempPathW(BUF_LEN, pathBuffer)) {
			QMessageBox::warning(
				this,
				obs_module_text("StreamElements.ReportIssue.Error.Generic.Title"),
				obs_module_text("StreamElements.ReportIssue.Error.GetTempPath.Text"),
				QMessageBox::Ok);

			return;
		}

		std::wstring wtempBufPath(pathBuffer);

		if (0 == ::GetTempFileNameW(wtempBufPath.c_str(), L"obs-live-error-report-data", 0, pathBuffer)) {
			QMessageBox::warning(
				this,
				obs_module_text("StreamElements.ReportIssue.Error.Generic.Title"),
				obs_module_text("StreamElements.ReportIssue.Error.GetTempFileName.Text"),
				QMessageBox::Ok);

			return;
		}

		wtempBufPath = pathBuffer;
		wtempBufPath += L".zip";

		tempBufPath = wstring_to_utf8(wtempBufPath);

		char programDataPathBuf[BUF_LEN];
		int ret = os_get_config_path(programDataPathBuf, BUF_LEN, "obs-studio");

		if (ret <= 0) {
			QMessageBox::warning(
				this,
				obs_module_text("StreamElements.ReportIssue.Error.Generic.Title"),
				obs_module_text("StreamElements.ReportIssue.Error.GetObsDataPath.Text"),
				QMessageBox::Ok);

			return;
		}

		std::wstring obsDataPath = QString(programDataPathBuf).toStdWString();

		zip_t* zip = zip_open(tempBufPath.c_str(), 9, 'w');

		auto addBufferToZip = [&](BYTE* buf, size_t bufLen, std::wstring zipPath)
		{
			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			zip_entry_write(zip, buf, bufLen);

			zip_entry_close(zip);
		};

		auto addLinesBufferToZip = [&](std::vector<std::string>& lines, std::wstring zipPath)
		{
			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			for (auto line : lines) {
				zip_entry_write(zip, line.c_str(), line.size());
				zip_entry_write(zip, "\r\n", 2);
			}

			zip_entry_close(zip);
		};

		auto addCefValueToZip = [&](CefRefPtr<CefValue>& input, std::wstring zipPath)
		{
			std::string buf =
				wstring_to_utf8(
					CefWriteJSON(
						input,
						JSON_WRITER_PRETTY_PRINT)
					.ToWString());

			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			zip_entry_write(zip, buf.c_str(), buf.size());

			zip_entry_close(zip);
		};

		auto addFileToZip = [&](std::wstring localPath, std::wstring zipPath)
		{
			FILE* stream;
			if (0 == _wfopen_s(&stream, localPath.c_str(), L"rb")) {
				size_t BUF_LEN = 32768;

				BYTE* buf = new BYTE[BUF_LEN];

				zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

				size_t read = fread(buf, 1, BUF_LEN, stream);
				while (read > 0 && 0 == zip_entry_write(zip, buf, read)) {
					read = fread(buf, 1, BUF_LEN, stream);
				}

				zip_entry_close(zip);

				delete[] buf;

				fclose(stream);
			}
		};

		auto addWindowCaptureToZip = [&](const HWND& hWnd, int nBitCount, std::wstring zipPath)
		{
			//calculate the number of color indexes in the color table
			int nColorTableEntries = -1;
			switch (nBitCount)
			{
			case 1:
				nColorTableEntries = 2;
				break;
			case 4:
				nColorTableEntries = 16;
				break;
			case 8:
				nColorTableEntries = 256;
				break;
			case 16:
			case 24:
			case 32:
				nColorTableEntries = 0;
				break;
			default:
				nColorTableEntries = -1;
				break;
			}

			if (nColorTableEntries == -1)
			{
				// printf("bad bits-per-pixel argument\n");
				return false;
			}

			HDC hDC = GetDC(hWnd);
			HDC hMemDC = CreateCompatibleDC(hDC);

			int nWidth = 0;
			int nHeight = 0;

			if (hWnd != HWND_DESKTOP)
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				nWidth = rect.right - rect.left;
				nHeight = rect.bottom - rect.top;
			}
			else
			{
				nWidth = ::GetSystemMetrics(SM_CXSCREEN);
				nHeight = ::GetSystemMetrics(SM_CYSCREEN);
			}

			HBITMAP hBMP = CreateCompatibleBitmap(hDC, nWidth, nHeight);
			SelectObject(hMemDC, hBMP);
			BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, 0, 0, SRCCOPY);

			int nStructLength = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * nColorTableEntries;
			LPBITMAPINFOHEADER lpBitmapInfoHeader = (LPBITMAPINFOHEADER)new char[nStructLength];
			::ZeroMemory(lpBitmapInfoHeader, nStructLength);

			lpBitmapInfoHeader->biSize = sizeof(BITMAPINFOHEADER);
			lpBitmapInfoHeader->biWidth = nWidth;
			lpBitmapInfoHeader->biHeight = nHeight;
			lpBitmapInfoHeader->biPlanes = 1;
			lpBitmapInfoHeader->biBitCount = nBitCount;
			lpBitmapInfoHeader->biCompression = BI_RGB;
			lpBitmapInfoHeader->biXPelsPerMeter = 0;
			lpBitmapInfoHeader->biYPelsPerMeter = 0;
			lpBitmapInfoHeader->biClrUsed = nColorTableEntries;
			lpBitmapInfoHeader->biClrImportant = nColorTableEntries;

			DWORD dwBytes = ((DWORD)nWidth * nBitCount) / 32;
			if (((DWORD)nWidth * nBitCount) % 32) {
				dwBytes++;
			}
			dwBytes *= 4;

			DWORD dwSizeImage = dwBytes * nHeight;
			lpBitmapInfoHeader->biSizeImage = dwSizeImage;

			LPBYTE lpDibBits = 0;
			HBITMAP hBitmap = ::CreateDIBSection(hMemDC, (LPBITMAPINFO)lpBitmapInfoHeader, DIB_RGB_COLORS, (void**)&lpDibBits, NULL, 0);
			SelectObject(hMemDC, hBitmap);
			BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, 0, 0, SRCCOPY);
			ReleaseDC(hWnd, hDC);

			BITMAPFILEHEADER bmfh;
			bmfh.bfType = 0x4d42;  // 'BM'
			int nHeaderSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * nColorTableEntries;
			bmfh.bfSize = 0;
			bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
			bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * nColorTableEntries;

			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			DWORD nColorTableSize = 0;
			if (nBitCount != 24) {
				nColorTableSize = (1ULL << nBitCount) * sizeof(RGBQUAD);
			}
			else {
				nColorTableSize = 0;
			}

			zip_entry_write(zip, &bmfh, sizeof(BITMAPFILEHEADER));
			zip_entry_write(zip, lpBitmapInfoHeader, nHeaderSize);

			if (nBitCount < 16)
			{
				//int nBytesWritten = 0;
				RGBQUAD *rgbTable = new RGBQUAD[nColorTableEntries * sizeof(RGBQUAD)];
				//fill RGBQUAD table and write it in file
				for (int i = 0; i < nColorTableEntries; ++i)
				{
					rgbTable[i].rgbRed = rgbTable[i].rgbGreen = rgbTable[i].rgbBlue = i;
					rgbTable[i].rgbReserved = 0;

					zip_entry_write(zip, &rgbTable[i], sizeof(RGBQUAD));
				}

				delete[] rgbTable;
			}

			zip_entry_write(zip, lpDibBits, dwSizeImage);

			zip_entry_close(zip);

			::DeleteObject(hBMP);
			::DeleteObject(hBitmap);
			delete[]lpBitmapInfoHeader;

			return true;
		};

		std::string package_manifest = "generator=report_issue\nversion=3\n";
		addBufferToZip((BYTE*)package_manifest.c_str(), package_manifest.size(), L"manifest.ini");

		// Add user-provided description
		addBufferToZip((BYTE*)descriptionText.c_str(), descriptionText.size(), L"description.txt");

		// Add window capture
		addWindowCaptureToZip(
			(HWND)StreamElementsGlobalStateManager::GetInstance()->mainWindow()->winId(),
			24,
			L"obs-main-window.bmp");

		std::map<std::wstring, std::wstring> local_to_zip_files_map;

		if (collect_all) {
			std::vector<std::wstring> blacklist = {
				L"plugin_config/obs-streamelements/obs-streamelements-update.exe",
				L"plugin_config/obs-browser/cache/"
			};

			// Collect all files
			for (auto& i : std::experimental::filesystem::recursive_directory_iterator(programDataPathBuf)) {
				if (!std::experimental::filesystem::is_directory(i.path())) {
					std::wstring local_path = i.path().c_str();
					std::wstring zip_path = local_path.substr(obsDataPath.size() + 1);

					std::wstring zip_path_lcase = zip_path;
					std::transform(zip_path_lcase.begin(), zip_path_lcase.end(), zip_path_lcase.begin(), ::towlower);
					std::transform(zip_path_lcase.begin(), zip_path_lcase.end(), zip_path_lcase.begin(), [](wchar_t ch) {
						return ch == L'\\' ? L'/' : ch;
					});

					bool accept = true;
					for (auto item : blacklist) {
						if (zip_path_lcase.size() >= item.size()) {
							if (zip_path_lcase.substr(0, item.size()) == item) {
								accept = false;

								break;
							}
						}
					}

					if (accept) {
						local_to_zip_files_map[local_path] = L"obs-studio\\" + zip_path;
					}
				}
			}
		}
		else {
			// Collect only own files
			local_to_zip_files_map[obsDataPath + L"\\plugin_config\\obs-streamelements\\obs-streamelements.ini"] = L"obs-studio\\plugin_config\\obs-streamelements\\obs-streamelements.ini";
			local_to_zip_files_map[obsDataPath + L"\\plugin_config\\obs-browser\\obs-browser-streamelements.ini"] = L"obs-studio\\plugin_config\\obs-browser\\obs-browser-streamelements.ini";
			local_to_zip_files_map[obsDataPath + L"\\global.ini"] = L"obs-studio\\global.ini";
		}

		dialog.setMessage(obs_module_text("StreamElements.ReportIssue.Progress.Message.CollectingFiles"));

		size_t count = 0;
		size_t total = local_to_zip_files_map.size();

		for (auto item : local_to_zip_files_map) {
			if (dialog.cancelled()) {
				goto cancelled;
			}

			++count;

			addFileToZip(item.first, item.second);

			dialog.setProgress(0, (int)total, (int)count);
		}

		if (dialog.cancelled()) {
			goto cancelled;
		}

		dialog.setMessage(obs_module_text("StreamElements.ReportIssue.Progress.Message.CollectingCpuBenchmark"));
		qApp->sendPostedEvents();

		/*
		uint64_t cpu_bench_begin = os_gettime_ns();
		const uint64_t CPU_BENCH_TOTAL = 10000000;
		uint64_t cpu_bench_accumulator = 2L;

		for (uint64_t bench = 0; bench < CPU_BENCH_TOTAL; ++bench) {
			cpu_bench_accumulator *= cpu_bench_accumulator;
			//if (dialog.cancelled()) {
			//	goto cancelled;
			//}
		}

		uint64_t cpu_bench_end = os_gettime_ns();
		uint64_t cpu_bench_delta = cpu_bench_end - cpu_bench_begin;
		double cpu_benchmark = (double)CPU_BENCH_TOTAL / (double)cpu_bench_delta;
		*/

		const uint64_t CPU_BENCH_TOTAL = 10000000;
		uint64_t cpu_bench_delta = 0;
		double cpu_benchmark = GetCpuCoreBenchmark(CPU_BENCH_TOTAL, cpu_bench_delta);

		if (dialog.cancelled()) {
			goto cancelled;
		}

		dialog.setMessage(obs_module_text("StreamElements.ReportIssue.Progress.Message.CollectingSysInfo"));
		qApp->sendPostedEvents();

		{
			CefRefPtr<CefValue> basicInfo = CefValue::Create();
			CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
			basicInfo->SetDictionary(d);

			std::string bench; bench += (cpu_benchmark);
			d->SetString("obsVersion", obs_get_version_string());
			d->SetString("cefVersion", GetCefVersionString());
			d->SetString("cefApiHash", GetCefPlatformApiHash());
#ifdef _WIN32
			d->SetString("platform", "windows");
#elif APPLE
			d->SetString("platform", "macos");
#elif LINUX
			d->SetString("platform", "linux");
#else
			d->SetString("platform", "other");
#endif
			d->SetString("streamelementsPluginVersion", GetStreamElementsPluginVersionString());
			d->SetDouble("cpuCoreBenchmarkScore", (double)cpu_benchmark);
			d->SetDouble("cpuCoreBenchmarkOpsCount", (double)CPU_BENCH_TOTAL);
			d->SetDouble("cpuCoreBenchmarkNanoseconds", (double)cpu_bench_delta);
#ifdef _WIN64
			d->SetString("platformArch", "64bit");
#else
			d->SetString("platformArch", "32bit");
#endif
			d->SetString("machineUniqueId", GetComputerSystemUniqueId());

			addCefValueToZip(basicInfo, L"system\\basic.json");
		}

		{
			CefRefPtr<CefValue> sysHardwareInfo = CefValue::Create();

			SerializeSystemHardwareProperties(sysHardwareInfo);

			addCefValueToZip(sysHardwareInfo, L"system\\hardware.json");
		}

		{
			CefRefPtr<CefValue> sysMemoryInfo = CefValue::Create();

			SerializeSystemMemoryUsage(sysMemoryInfo);

			addCefValueToZip(sysMemoryInfo, L"system\\memory.json");
		}

		{
			// Histogram CPU & memory usage (past hour, 1 minute intervals)

			auto cpuUsageHistory =
				StreamElementsGlobalStateManager::GetInstance()->GetPerformanceHistoryTracker()->getCpuUsageSnapshot();

			auto memoryUsageHistory =
				StreamElementsGlobalStateManager::GetInstance()->GetPerformanceHistoryTracker()->getMemoryUsageSnapshot();

			char lineBuf[512];

			{
				std::vector<std::string> lines;

				lines.push_back("totalSeconds,busySeconds,idleSeconds");
				for (auto item : cpuUsageHistory) {
					sprintf(lineBuf, "%1.2Lf,%1.2Lf,%1.2Lf", item.totalSeconds, item.busySeconds, item.idleSeconds);

					lines.push_back(lineBuf);
				}

				addLinesBufferToZip(lines, L"system\\usage_history_cpu.csv");
			}

			{
				std::vector<std::string> lines;

				lines.push_back("totalSeconds,memoryUsedPercentage");

				size_t index = 0;
				for (auto item : memoryUsageHistory) {
					if (index < cpuUsageHistory.size()) {
						auto totalSec = cpuUsageHistory[index].totalSeconds;

						sprintf(lineBuf, "%1.2Lf,%d",
							totalSec,
							item.dwMemoryLoad // % Used
						);
					}
					else {
						sprintf(lineBuf, "%1.2Lf,%d",
							0.0,
							item.dwMemoryLoad // % Used
						);
					}

					lines.push_back(lineBuf);

					++index;
				}

				addLinesBufferToZip(lines, L"system\\usage_history_memory.csv");
			}
		}

cancelled:
		zip_close(zip);

		if (!dialog.cancelled()) {
			StreamElementsGlobalStateManager::GetInstance()->GetAnalyticsEventsManager()->trackEvent(
				"Issue Report",
				json11::Json::object{ { "issueDescription", descriptionText.c_str() } }
			);

			QMetaObject::invokeMethod(&dialog, "accept", Qt::QueuedConnection);
		}
	});

	if (dialog.exec() == QDialog::Accepted)
	{
		// HTTP upload

		bool retry = true;

		while (retry) {
			obs_frontend_push_ui_translation(obs_module_get_string);

			StreamElementsNetworkDialog netDialog(
				StreamElementsGlobalStateManager::GetInstance()->mainWindow());

			obs_frontend_pop_ui_translation();

			bool success = netDialog.UploadFile(
				tempBufPath.c_str(),
				StreamElementsConfig::GetInstance()->GetUrlReportIssue().c_str(),
				"package",
				obs_module_text("StreamElements.ReportIssue.Upload.Message"));

			if (success) {
				retry = false;

				QMessageBox::information(
					&dialog,
					obs_module_text("StreamElements.ReportIssue.ThankYou.Title"),
					obs_module_text("StreamElements.ReportIssue.ThankYou.Text"),
					QMessageBox::Ok);
			}
			else {
				retry = QMessageBox::Yes == QMessageBox::question(
					&dialog,
					obs_module_text("StreamElements.ReportIssue.Upload.Retry.Title"),
					obs_module_text("StreamElements.ReportIssue.Upload.Retry.Text"),
					QMessageBox::Yes | QMessageBox::No);
			}
		}

		os_unlink(tempBufPath.c_str());
	}


	if (thread.joinable()) {
		thread.join();
	}

	QDialog::accept();
}

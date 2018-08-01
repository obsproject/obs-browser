#pragma once

#include <cef-headers.hpp>
#include <obs.h>
#include <obs-module.h>

#include <memory>
#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdarg>

#include <functional>

#include <util/threading.h>

#define SYNC_ACCESS() static std::mutex __sync_access_mutex; std::lock_guard<std::mutex> __sync_access_mutex_guard(__sync_access_mutex);

#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>

template<typename ... Args>
std::string FormatString(const char* format, ...)
{
	int size = 512;
	char* buffer = 0;
	buffer = new char[size];
	va_list vl;
	va_start(vl, format);
	int nsize = vsnprintf(buffer, size, format, vl);
	if (size <= nsize) { //fail delete buffer and try again
		delete[] buffer;
		buffer = 0;
		buffer = new char[nsize + 1]; //+1 for /0
		nsize = vsnprintf(buffer, size, format, vl);
	}
	std::string ret(buffer);
	va_end(vl);
	delete[] buffer;
	return ret;
}

void QtPostTask(void(*func)(void*), void* const data);
void QtExecSync(void(*func)(void*), void* const data);
std::string DockWidgetAreaToString(const Qt::DockWidgetArea area);
std::string GetCommandLineOptionValue(const std::string key);
std::string LoadResourceString(std::string path);

/* ========================================================= */

void SerializeSystemTimes(CefRefPtr<CefValue>& output);
void SerializeSystemMemoryUsage(CefRefPtr<CefValue>& output);

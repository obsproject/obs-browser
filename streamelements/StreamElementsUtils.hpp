#pragma once

#include <memory>
#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdarg>

#include <functional>
#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>

#include <util/threading.h>

#define SYNC_ACCESS() static std::mutex __sync_access_mutex; std::lock_guard<std::mutex> __sync_access_mutex_guard(__sync_access_mutex);

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

inline static void QtPostTask(void(*func)(void*), void* const data)
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

inline static void QtExecSync(void(*func)(void*), void* const data)
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

inline std::string DockWidgetAreaToString(const Qt::DockWidgetArea area)
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

inline static std::string GetCommandLineOptionValue(const std::string key)
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

inline static std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
}

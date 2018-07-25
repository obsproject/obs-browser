#pragma once

#include <QDialog>

#include "cef-headers.hpp"

#include <string>

#include "StreamElementsUtils.hpp"
#include "StreamElementsBrowserWidget.hpp"

class StreamElementsDialogApiMessageHandler;

class StreamElementsBrowserDialog: public QDialog
{
	friend StreamElementsDialogApiMessageHandler;

public:
	StreamElementsBrowserDialog(QWidget* parent, std::string url, std::string executeJavaScriptOnLoad);
	~StreamElementsBrowserDialog();

	std::string result() { return m_result; }

public Q_SLOTS:
	virtual int exec() override;

private:
	StreamElementsBrowserWidget* m_browser;
	std::string m_result;

	std::string m_url;
	std::string m_executeJavaScriptOnLoad;
};

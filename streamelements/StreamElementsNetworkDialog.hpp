#pragma once

#include <QDialog>

#include <curl/curl.h>

#include <util/config-file.h>

#include "StreamElementsAsyncTaskQueue.hpp"

namespace Ui {
	class StreamElementsNetworkDialog;
}

///
// Async file download with UI
//
class StreamElementsNetworkDialog : public QDialog
{
	Q_OBJECT

public:
	explicit StreamElementsNetworkDialog(QWidget *parent = 0);
	~StreamElementsNetworkDialog();

private:
	Ui::StreamElementsNetworkDialog *ui;

private:
	StreamElementsAsyncTaskQueue	m_taskQueue;		// asynchronous tasks queue
	bool		                m_cancel_pending;	// set to true to cancel current download
	bool				m_running;		// set to true when download or upload are running

	///
	// curl transfer progress callback
	//
	static int DownloadFileAsyncXferProgressCallback(
		void *clientp,
		curl_off_t dltotal,
		curl_off_t dlnow,
		curl_off_t ultotal,
		curl_off_t ulnow);

	///
	// curl transfer progress callback
	//
	static int UploadFileAsyncXferProgressCallback(
		void *clientp,
		curl_off_t dltotal,
		curl_off_t dlnow,
		curl_off_t ultotal,
		curl_off_t ulnow);

public:
	///
	// Asynchronously download a file
	//
	// @param localFilePath	download destination file path
	// @param url		source file URL
	// @param callback	function to call when done: callback(download_success, param)
	// @param param		second parameter to @callback
	// @param message	message to display while downloading
	//
	void DownloadFileAsync(
		const char* localFilePath,
		const char* url,
		bool large_file,
		void(*callback)(bool, void*),
		void* param,
		const char* message);

	///
	// Synchronously upload a file
	//
	// @param localFilePath	download destination file path
	// @param url		source file URL
	// @param fieldName	HTTP POST file field name
	// @param message	message to display while downloading
	//
	bool UploadFile(
		const char* localFilePath,
		const char* url,
		const char* fieldName,
		const char* message);

public slots:
	virtual void reject() override;

private slots:
	///
	// Called by DownloadFileAsyncXferProgressCallback on Qt UI thread to
	// update the user interface
	//
	void DownloadFileAsyncUpdateUserInterface(long dltotal, long dlnow);

	///
	// Called by UploadFileAsyncXferProgressCallback on Qt UI thread to
	// update the user interface
	//
	void UploadFileAsyncUpdateUserInterface(long dltotal, long dlnow);

	///
	// Handle user's click on "cancel" button
	//
	// Sets m_cancel_pending = true
	//
	void on_ctl_cancelButton_clicked();
};

#include "StreamElementsBrowserDialog.hpp"
#include "StreamElementsApiMessageHandler.hpp"

#include <QVBoxLayout>

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

static std::mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name) RegisterIncomingApiCallHandler(name, [](StreamElementsApiMessageHandler* self, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result, CefRefPtr<CefBrowser> browser, void (*complete_callback)(void*), void* complete_context) { std::lock_guard<std::mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END() complete_callback(complete_context); });

class StreamElementsDialogApiMessageHandler : public StreamElementsApiMessageHandler
{
public:
	StreamElementsDialogApiMessageHandler(StreamElementsBrowserDialog* dialog) :
		m_dialog(dialog)
	{
	}

private:
	StreamElementsBrowserDialog* m_dialog;

protected:
	StreamElementsBrowserDialog* dialog() { return m_dialog; }

	virtual void RegisterIncomingApiCallHandlers() override
	{
		StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlers();

		API_HANDLER_BEGIN("endModalDialog");
		{
			result->SetBool(false);

			if (args->GetSize()) {
				StreamElementsDialogApiMessageHandler* msgHandler =
					static_cast<StreamElementsDialogApiMessageHandler*>(self);

				CefRefPtr<CefValue> arg = args->GetValue(0);

				msgHandler->dialog()->m_result =
					CefWriteJSON(arg, JSON_WRITER_DEFAULT).ToString();

				QtPostTask([](void* data) {
					StreamElementsDialogApiMessageHandler* msgHandler =
						(StreamElementsDialogApiMessageHandler*)data;

					msgHandler->dialog()->accept();
				}, msgHandler);
			}
		}
		API_HANDLER_END();
	}
};

StreamElementsBrowserDialog::StreamElementsBrowserDialog(QWidget* parent, std::string url, std::string executeJavaScriptOnLoad)
	: QDialog(parent), m_url(url), m_executeJavaScriptOnLoad(executeJavaScriptOnLoad)
{
	setLayout(new QVBoxLayout());

	this->setWindowFlags((
		(windowFlags() | Qt::CustomizeWindowHint)
		& ~Qt::WindowContextHelpButtonHint
		));
}

StreamElementsBrowserDialog::~StreamElementsBrowserDialog()
{
}

int StreamElementsBrowserDialog::exec()
{
	m_browser = new StreamElementsBrowserWidget(
		nullptr,
		m_url.c_str(),
		m_executeJavaScriptOnLoad.c_str(),
		"dialog",
		"",
		new StreamElementsDialogApiMessageHandler(this));

	layout()->addWidget(m_browser);

	int result = QDialog::exec();

	m_browser->deleteLater();

	return result;
}

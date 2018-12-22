#include "StreamElementsControllerServer.hpp"
#include "StreamElementsMessageBus.hpp"

static const char* PIPE_NAME = "\\\\.\\pipe\\StreamElementsObsLiveControllerServer";
static const size_t MAX_CLIENTS = 15;
static const char* SOURCE_ADDR = "";

StreamElementsControllerServer::StreamElementsControllerServer(StreamElementsMessageBus* bus) :
	m_bus(bus)
{
	auto msgReceiver = [this](const char* const buffer, const size_t) {
		std::string str(buffer);

		OnMsgReceivedInternal(str);
	};

	m_server = new NamedPipesServer(
		PIPE_NAME,
		msgReceiver,
		MAX_CLIENTS);

	m_server->Start();
}

StreamElementsControllerServer::~StreamElementsControllerServer()
{
	m_server->Stop();

	delete m_server;
}

void StreamElementsControllerServer::OnMsgReceivedInternal(std::string& msg)
{
	CefRefPtr<CefValue> root =
		CefParseJSON(msg, JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!!root.get() && root->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = root->GetDictionary();

		if (d->HasKey("version") && d->HasKey("source") && d->HasKey("target") && d->HasKey("payload")) {
			m_bus->NotifyAllMessageListeners(
				StreamElementsMessageBus::DEST_ALL_LOCAL & ~StreamElementsMessageBus::DEST_BROWSER_SOURCE,
				StreamElementsMessageBus::SOURCE_EXTERNAL,
				SOURCE_ADDR,
				root);
		}
	}
}

void StreamElementsControllerServer::NotifyAllClients(
	std::string source,
	std::string sourceAddress,
	CefRefPtr<CefValue> payload)
{
	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();
	root->SetDictionary(rootDict);

	CefRefPtr<CefDictionaryValue> sourceDict = CefDictionaryValue::Create();
	CefRefPtr<CefDictionaryValue> targetDict = CefDictionaryValue::Create();

	sourceDict->SetString("class", source);
	sourceDict->SetString("address", sourceAddress);

	targetDict->SetString("scope", "broadcast");

	rootDict->SetInt("version", 1);
	rootDict->SetDictionary("source", sourceDict);
	rootDict->SetDictionary("target", targetDict);

	if (payload->GetType() == VTYPE_DICTIONARY) {
		rootDict->SetValue("payload", payload->Copy());
	}

	std::string buf = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	m_server->WriteMessage(buf.c_str(), buf.size());
}

void StreamElementsControllerServer::SendEventAllClients(
	std::string source,
	std::string sourceAddress,
	std::string eventName,
	CefRefPtr<CefValue> eventData)
{
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetString("class", "event");

	CefRefPtr<CefDictionaryValue> ed = CefDictionaryValue::Create();

	ed->SetString("name", eventName);
	ed->SetValue("data", eventData);

	d->SetDictionary("event", ed);

	root->SetDictionary(d);

	NotifyAllClients(source, sourceAddress, root);
}

void StreamElementsControllerServer::SendMessageAllClients(
	std::string source,
	std::string sourceAddress,
	CefRefPtr<CefValue> message)
{
	CefRefPtr<CefValue> root = CefValue::Create();

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetString("class", "message");
	d->SetValue("data", message);

	root->SetDictionary(d);

	NotifyAllClients(source, sourceAddress, root);
}

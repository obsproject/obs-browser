#pragma once

#include "deps/server/NamedPipesServer.hpp"
#include "cef-headers.hpp"

class StreamElementsMessageBus;

///////////////////////////////////////////////////////////////////////
//
// Listens for JSON messages over a network connection (currently
// implemented as Named Pipes server).
//
// In case the messages are properly structured, dispatches them to
// all browsers except Browser Sources as an onHostMessageReceived
// event.
//
// For the message to be properly structured, it must contain the
// following root-level properties:
//
//	version		- number. protocol version (1)
//	source		- object
//	target		- object ({ scope: "broadcast" })
//	payload		- object. the message payload.
//
// In case of a command message, the "payload" property value is
// structured as follows:
//
//	class		- string. "command"
//	command		- object. ({ name: "command-name", "data": object })
//
// The primary purpose of this class is to support integration with
// external controllers, such as Elgato Streamdeck.
//
class StreamElementsControllerServer
{
public:
	StreamElementsControllerServer(StreamElementsMessageBus* bus);
	virtual ~StreamElementsControllerServer();

public:
	///
	// Send a message with specified payload to all connected clients
	//
	//	source		- message source type
	//	sourceAddress	- message source address
	//	payload		- message payload
	//
	void NotifyAllClients(
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload);

	///
	// Send an event message to all connected clients
	//
	//	source		- message source type
	//	sourceAddress	- message source address
	//	eventName	- event name
	//	eventData	- event data
	//
	void SendEventAllClients(
		std::string source,
		std::string sourceAddress,
		std::string eventName,
		CefRefPtr<CefValue> eventData);

	///
	// Send a generic message to all connected clients
	//
	//	source		- message source type
	//	sourceAddress	- message source address
	//	message		- message data
	//
	void SendMessageAllClients(
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> message);

private:
	void OnMsgReceivedInternal(std::string& msg);

private:
	StreamElementsMessageBus* m_bus = nullptr;
	NamedPipesServer* m_server = nullptr;
};

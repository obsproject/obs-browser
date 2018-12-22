#pragma once

#include "StreamElementsApiMessageHandler.hpp"

// Exposes a limited subset of API calls to Browser Sources
//
class StreamElementsBrowserSourceApiMessageHandler :
	public StreamElementsApiMessageHandler
{
public:
	StreamElementsBrowserSourceApiMessageHandler();
	virtual ~StreamElementsBrowserSourceApiMessageHandler();

protected:
	virtual void RegisterIncomingApiCallHandlers() override;
};

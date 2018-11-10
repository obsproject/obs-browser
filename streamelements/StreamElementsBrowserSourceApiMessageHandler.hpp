#pragma once

#include "StreamElementsApiMessageHandler.hpp"

class StreamElementsBrowserSourceApiMessageHandler :
	public StreamElementsApiMessageHandler
{
public:
	StreamElementsBrowserSourceApiMessageHandler();
	virtual ~StreamElementsBrowserSourceApiMessageHandler();

protected:
	virtual void RegisterIncomingApiCallHandlers() override;
};

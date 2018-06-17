#pragma once

#include "StreamElementsBandwidthTestClient.hpp"
#include "StreamElementsObsAppMonitor.hpp"

class StreamElementsObsBandwidthTestClient :
	public StreamElementsBandwidthTestClient,
	public StreamElementsObsAppMonitor
{
public:
	StreamElementsObsBandwidthTestClient();
	virtual ~StreamElementsObsBandwidthTestClient();

protected:
	virtual void OnObsExit() override;
};

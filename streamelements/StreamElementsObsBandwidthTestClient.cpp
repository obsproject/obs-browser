#include "StreamElementsObsBandwidthTestClient.hpp"

StreamElementsObsBandwidthTestClient::StreamElementsObsBandwidthTestClient()
{
}

StreamElementsObsBandwidthTestClient::~StreamElementsObsBandwidthTestClient()
{
}

void StreamElementsObsBandwidthTestClient::OnObsExit()
{
	CancelAll();
}

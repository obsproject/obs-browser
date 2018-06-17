#pragma once

#include <obs-frontend-api.h>

class StreamElementsObsAppMonitor
{
public:
	StreamElementsObsAppMonitor()
	{
		Connect();
	}

	~StreamElementsObsAppMonitor()
	{
		Disconnect();
	}

protected:
	virtual void OnObsExit() = 0;

private:
	void Connect()
	{
		obs_frontend_add_event_callback(obs_frontend_event_handler, this);
	}

	void Disconnect()
	{
		obs_frontend_remove_event_callback(obs_frontend_event_handler, this);
	}


	static void obs_frontend_event_handler(enum obs_frontend_event event, void *data)
	{
		StreamElementsObsAppMonitor* self = (StreamElementsObsAppMonitor*)data;

		switch (event) {
		case OBS_FRONTEND_EVENT_EXIT:
			self->OnObsExit();
			break;
		}
	}
};

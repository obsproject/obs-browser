#pragma once

#include "StreamElementsExternalSceneDataProvider.hpp"
#include <util/platform.h>
#include <string>
#include <codecvt>
#include <filesystem>

class StreamElementsExternalSceneDataProviderSlobsClient :
	public StreamElementsExternalSceneDataProvider
{
public:
	StreamElementsExternalSceneDataProviderSlobsClient(std::string providerId) :
		StreamElementsExternalSceneDataProvider(providerId, "Streamlabs OBS")
	{
		char* path = new char[MAX_PATH];

		if (os_get_config_path(path, MAX_PATH, "slobs-client/SceneCollections") >= 0) {
			m_basePath = path;

			std::replace(m_basePath.begin(), m_basePath.end(), '\\', '/');

			if (!std::experimental::filesystem::is_directory(m_basePath)) {
				blog(LOG_INFO,
					"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: path does not exist: %s",
					m_basePath.c_str());

				m_basePath = "";
			}
		}
		else {
			m_basePath = "";
		}

		delete[] path;
	}

protected:
	virtual bool GetSceneCollections(
		std::vector<scene_collection_t>& result) override;

	virtual bool GetSceneCollection(
		std::string collectionId,
		scene_collection_content_t& result) override;

private:
	std::string m_basePath;
};

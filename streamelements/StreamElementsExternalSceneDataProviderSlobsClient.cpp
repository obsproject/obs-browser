#include "StreamElementsExternalSceneDataProviderSlobsClient.hpp"

bool StreamElementsExternalSceneDataProviderSlobsClient::GetSceneCollections(
	std::vector<scene_collection_t>& result)
{
	if (!m_basePath.size()) {
		return false;
	}

	bool success = false;

	char* manifestContent =
		os_quick_read_utf8_file((m_basePath + "/manifest.json").c_str());

	CefRefPtr<CefValue> manifestValue =
		CefParseJSON(
			manifestContent ? manifestContent : "{}",
			JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (manifestContent) {
		bfree(manifestContent);
	}
	else {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: failed loading file: %s",
			(m_basePath + "/manifest.json").c_str());
	}

	if (manifestValue->GetType() != VTYPE_DICTIONARY) {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: invalid file format (expected JSON object): %s",
			(m_basePath + "/manifest.json").c_str());

		return false;
	}

	CefRefPtr<CefDictionaryValue> root = manifestValue->GetDictionary();

	if (root.get() && root->HasKey("collections") && root->GetType("collections") == VTYPE_LIST) {
		success = true;

		CefRefPtr<CefListValue> collections = root->GetList("collections");

		for (size_t collectionIndex = 0; collectionIndex < collections->GetSize(); ++collectionIndex) {
			if (collections->GetType(collectionIndex) != VTYPE_DICTIONARY) {
				continue;
			}

			CefRefPtr<CefDictionaryValue> collection =
				collections->GetDictionary(collectionIndex);

			if (collection->HasKey("deleted") && collection->GetType("deleted") == VTYPE_BOOL) {
				if (collection->GetBool("deleted")) {
					continue;
				}
			}

			if (!collection->HasKey("id") || collection->GetType("id") != VTYPE_STRING) {
				blog(LOG_WARNING,
					"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: invalid collection object format (expected 'id' string): %s",
					(m_basePath + "/manifest.json").c_str());

				continue;
			}

			scene_collection_t item;

			item.collectionId = collection->GetString("id");

			if (!collection->HasKey("name") || collection->GetType("name") != VTYPE_STRING) {
				item.name = item.collectionId;
			}
			else {
				item.name = collection->GetString("name");
			}

			result.push_back(item);
		}
	}
	else {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: invalid file format (expected 'collections' array): %s",
			(m_basePath + "/manifest.json").c_str());
	}

	return success;
}

static void dumpPaths(CefRefPtr<CefValue> parent, std::vector<std::string>& paths)
{
	if (parent->GetType() == VTYPE_STRING) {
		std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

		std::string candidate =
			myconv.to_bytes(
				parent->GetString().ToWString());

		if (std::experimental::filesystem::is_regular_file(candidate)) {
			paths.push_back(candidate);
		}
		else {
			// Not a file or file does not exist
		}
	}

	if (parent->GetType() == VTYPE_LIST) {
		CefRefPtr<CefListValue> list = parent->GetList();

		for (size_t i = 0; i < list->GetSize(); ++i) {
			dumpPaths(list->GetValue(i), paths);
		}
	}

	if (parent->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = parent->GetDictionary();

		CefDictionaryValue::KeyList keys;
		if (d->GetKeys(keys)) {
			for (auto key : keys) {
				dumpPaths(d->GetValue(key), paths);
			}
		}
	}
}

bool StreamElementsExternalSceneDataProviderSlobsClient::GetSceneCollection(
	std::string collectionId,
	scene_collection_content_t& result)
{
	if (!m_basePath.size()) {
		return false;
	}

	std::vector<scene_collection_t> collections;

	if (!GetSceneCollections(collections)) {
		return false;
	}

	scene_collection_t* collection;

	for (size_t collectionIndex = 0; collectionIndex < collections.size(); ++collectionIndex) {
		if (collections[collectionIndex].collectionId == collectionId) {
			collection = &collections[collectionIndex];
			break;
		}
	}

	if (!collection) {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: collection id '%s' does not exist in '%s'",
			collectionId.c_str(),
			(m_basePath + "/manifest.json").c_str());

		return false;
	}

	bool success = false;

	result.collectionId = collection->collectionId;
	result.name = collection->name;

	char* manifest_content =
		os_quick_read_utf8_file((m_basePath + "/manifest.json").c_str());

	if (manifest_content) {
		scene_collection_file_content_t meta_manifest;

		meta_manifest.path = m_basePath + "/manifest.json";
		meta_manifest.content = manifest_content;
		result.metadataFiles.push_back(meta_manifest);

		bfree(manifest_content);
	}
	else {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: failed loading file: %s",
			(m_basePath + "/manifest.json").c_str());

		return false;
	}

	char* scene_collection_content =
		os_quick_read_utf8_file((m_basePath + "/" + result.collectionId + ".json").c_str());

	if (scene_collection_content) {
		scene_collection_file_content_t meta_scene_collection;

		meta_scene_collection.path = m_basePath + "/" + result.collectionId + ".json";
		meta_scene_collection.content = scene_collection_content;
		result.metadataFiles.push_back(meta_scene_collection);

		bfree(scene_collection_content);

		CefRefPtr<CefValue> root =
			CefParseJSON(
				meta_scene_collection.content.c_str(),
				JSON_PARSER_ALLOW_TRAILING_COMMAS);

		if (root->GetType() != VTYPE_NULL && root->GetType() != VTYPE_INVALID) {
			success = true;

			std::vector<std::string> paths;

			dumpPaths(root, paths);

			for (auto path : paths) {
				result.referencedFiles.push_back(path);
			}
		}
		else {
			blog(LOG_WARNING,
				"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: invalid scene collection definition file (expected JSON): %s",
				(m_basePath + "/" + collectionId + ".json").c_str());
		}
	}
	else {
		blog(LOG_WARNING,
			"obs-browser: StreamElementsExternalSceneDataProviderSlobsClient: failed loading scene collection definition file: %s",
			(m_basePath + "/" + collectionId + ".json").c_str());
	}

	return success;
}

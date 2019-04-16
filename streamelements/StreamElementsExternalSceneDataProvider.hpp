#pragma once

#include <string>
#include <vector>
#include "StreamElementsUtils.hpp"

class StreamElementsExternalSceneDataProvider
{
protected:
	StreamElementsExternalSceneDataProvider(std::string providerId, std::string name) :
		m_providerId(providerId),
		m_name(name)
	{
	}

	virtual ~StreamElementsExternalSceneDataProvider()
	{
	}

public:
	std::string name() { return m_name; }
	std::string providerId() { return m_providerId; }

protected:
	struct scene_collection_t {
		std::string collectionId;
		std::string name;
	};

	struct scene_collection_file_content_t {
		std::string path;
		std::string content;
	};

	struct scene_collection_content_t : public scene_collection_t {
		std::vector<scene_collection_file_content_t> metadataFiles;
		std::vector<std::string> referencedFiles;
	};

	virtual bool GetSceneCollections(
		std::vector<scene_collection_t>& result) = 0;

	virtual bool GetSceneCollection(
		std::string collectionId, scene_collection_content_t& result) = 0;

public:
	bool SerializeSceneCollections(CefRefPtr<CefValue> result)
	{
		std::vector<scene_collection_t> collections;

		if (!GetSceneCollections(collections)) {
			return false;
		}

		CefRefPtr<CefListValue> list = CefListValue::Create();

		for (auto col : collections) {
			CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

			d->SetString("providerId", providerId().c_str());
			d->SetString("collectionId", col.collectionId.c_str());
			d->SetString("name", col.name.c_str());

			list->SetDictionary(list->GetSize(), d);
		}

		result->SetList(list);

		return true;
	}

	bool SerializeSceneCollection(
		std::string collectionId,
		CefRefPtr<CefValue> result)
	{
		scene_collection_content_t collection;

		if (!GetSceneCollection(collectionId, collection)) {
			return false;
		}

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("providerId", providerId().c_str());
		d->SetString("collectionId", collection.collectionId.c_str());
		d->SetString("name", collection.name.c_str());

		{
			CefRefPtr<CefListValue> list = CefListValue::Create();

			for (auto file : collection.metadataFiles) {
				CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();

				item->SetString("path", file.path.c_str());
				item->SetString("content", file.content.c_str());

				list->SetDictionary(list->GetSize(), item);
			}

			d->SetList("metadataFiles", list);
		}

		{
			CefRefPtr<CefListValue> list = CefListValue::Create();

			for (auto file : collection.referencedFiles) {
				CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();

				item->SetString("path", file.c_str());
				item->SetString("url", CreateSessionSignedAbsolutePathURL(file));

				list->SetDictionary(list->GetSize(), item);
			}

			d->SetList("referencedFiles", list);
		}

		result->SetDictionary(d);

		return true;
	}

private:
	std::string m_name;
	std::string m_providerId;
};

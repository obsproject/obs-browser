#pragma once

#include "StreamElementsExternalSceneDataProvider.hpp"
#include "StreamElementsExternalSceneDataProviderSlobsClient.hpp"

#include <string>
#include <map>
#include "cef-headers.hpp"

class StreamElementsExternalSceneDataProviderManager
{
public:
	StreamElementsExternalSceneDataProviderManager()
	{
		m_providers["slobs-client"] =
			std::make_shared<StreamElementsExternalSceneDataProviderSlobsClient>(
				"slobs-client");
	}

	void SerializeProviders(CefRefPtr<CefValue> result)
	{
		CefRefPtr<CefListValue> list = CefListValue::Create();

		for (auto provider : m_providers) {
			CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

			d->SetString("providerId", provider.first.c_str());
			d->SetString("name", provider.second->name().c_str());

			list->SetDictionary(list->GetSize(), d);
		}

		result->SetList(list);
	}

	bool SerializeProviderSceneCollections(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> result)
	{
		if (input->GetType() != VTYPE_DICTIONARY) {
			return false;
		}

		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (!d->HasKey("providerId") || d->GetType("providerId") != VTYPE_STRING) {
			return false;
		}

		std::string providerId = d->GetString("providerId");

		if (m_providers.count(providerId)) {
			auto provider = m_providers[providerId];

			return provider->SerializeSceneCollections(result);
		}
		else {
			return false;
		}
	}

	bool SerializeProviderSceneColletion(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> result)
	{
		if (input->GetType() != VTYPE_DICTIONARY) {
			return false;
		}

		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (!d->HasKey("providerId") || d->GetType("providerId") != VTYPE_STRING) {
			return false;
		}

		if (!d->HasKey("collectionId") || d->GetType("collectionId") != VTYPE_STRING) {
			return false;
		}

		std::string providerId = d->GetString("providerId");

		if (m_providers.count(providerId)) {
			auto provider = m_providers[providerId];

			std::string collectionId = d->GetString("collectionId");

			return provider->SerializeSceneCollection(collectionId, result);
		}
		else {
			return false;
		}
	}
private:
	std::map<std::string, std::shared_ptr<StreamElementsExternalSceneDataProvider>> m_providers;
};

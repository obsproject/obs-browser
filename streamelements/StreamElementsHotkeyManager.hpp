#pragma once

#include "obs.h"
#include "obs-hotkey.h"
#include "cef-headers.hpp"

#include <map>
#include <mutex>

class StreamElementsHotkeyManager
{
public:
	StreamElementsHotkeyManager();
	~StreamElementsHotkeyManager();

	bool SerializeHotkeyBindings(CefRefPtr<CefValue>& result, bool onlyManagedBindings = false);
	bool DeserializeHotkeyBindings(CefRefPtr<CefValue> input);

	obs_hotkey_id DeserializeSingleHotkeyBinding(CefRefPtr<CefValue> input);

	bool RemoveHotkeyBindingById(obs_hotkey_id id);
	void RemoveAllManagedHotkeyBindings();

public:
	virtual void keyCombinationTriggered(CefRefPtr<CefBrowser> browser, obs_key_combination_t combination, bool pressed);

protected:
	virtual void hotkeyTriggered(obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

private:
	std::recursive_mutex m_mutex;

	std::map<std::string, obs_hotkey_id> m_registeredHotkeyNamesToHotkeyIds;
	std::map<obs_hotkey_id, std::string> m_registeredHotkeyIdsToNames;
	std::map<obs_hotkey_id, CefRefPtr<CefValue>> m_registeredHotkeySerializedValues;
	std::map<obs_hotkey_id, std::string> m_registeredHotkeyDataString;

private:
	static void hotkey_change_handler(void*, calldata_t*);
};

#include "StreamElementsHotkeyManager.hpp"
#include "StreamElementsCefClient.hpp"

#include <util/dstr.h>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <vector>

static obs_key_combination_t DeserializeKeyCombination(CefRefPtr<CefValue> input)
{
	obs_key_combination_t combination = { 0 };

	if (input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (d->HasKey("keyName") && d->GetType("keyName") == VTYPE_STRING) {
			combination.key = obs_key_from_name(d->GetString("keyName").ToString().c_str());
		}
		else if (d->HasKey("virtualKeyCode") && d->GetType("virtualKeyCode") == VTYPE_INT) {
			combination.key = obs_key_from_virtual_key(d->GetInt("virtualKeyCode"));
		}

		if (!obs_key_combination_is_empty(combination)) {
			if (d->HasKey("left") && d->GetBool("left")) {
				combination.modifiers |= INTERACT_IS_LEFT;
			}

			if (d->HasKey("right") && d->GetBool("right")) {
				combination.modifiers |= INTERACT_IS_RIGHT;
			}


			if (d->HasKey("altKey") && d->GetBool("altKey")) {
				combination.modifiers |= INTERACT_ALT_KEY;
			}

			if (d->HasKey("ctrlKey") && d->GetBool("ctrlKey")) {
				combination.modifiers |= INTERACT_CONTROL_KEY;
			}

			if (d->HasKey("commandKey") && d->GetBool("commandKey")) {
				combination.modifiers |= INTERACT_COMMAND_KEY;
			}

			if (d->HasKey("shiftKey") && d->GetBool("shiftKey")) {
				combination.modifiers |= INTERACT_SHIFT_KEY;
			}


			if (d->HasKey("capsLock") && d->GetBool("capsLock")) {
				combination.modifiers |= INTERACT_CAPS_KEY;
			}

			if (d->HasKey("numLock") && d->GetBool("numLock")) {
				combination.modifiers |= INTERACT_NUMLOCK_KEY;
			}

			if (d->HasKey("mouseLeftButton") && d->GetBool("mouseLeftButton")) {
				combination.modifiers |= INTERACT_MOUSE_LEFT;
			}

			if (d->HasKey("mouseMidButton") && d->GetBool("mouseMidButton")) {
				combination.modifiers |= INTERACT_MOUSE_MIDDLE;
			}

			if (d->HasKey("mouseRightButton") && d->GetBool("mouseRightButton")) {
				combination.modifiers |= INTERACT_MOUSE_RIGHT;
			}
		}
	}

	return combination;
}

static CefRefPtr<CefDictionaryValue> SerializeKeyCombination(obs_key_combination_t combination)
{
	dstr combo_str = { 0 };
	dstr key_str = { 0 };

	dstr_init(&combo_str);
	dstr_init(&key_str);

	obs_key_combination_to_str(combination, &combo_str);
	obs_key_to_str(combination.key, &key_str);

	CefRefPtr<CefDictionaryValue> comboDict = CefDictionaryValue::Create();

	comboDict->SetString("description", combo_str.array);
	comboDict->SetInt("keyCode", combination.key);
	comboDict->SetString("keyName", obs_key_to_name(combination.key));
	comboDict->SetString("keySymbol", key_str.array);
	comboDict->SetInt("virtualKeyCode", obs_key_to_virtual_key(combination.key));

	comboDict->SetBool("left", combination.modifiers & INTERACT_IS_LEFT);
	comboDict->SetBool("right", combination.modifiers & INTERACT_IS_RIGHT);

	comboDict->SetBool("altKey", combination.modifiers & INTERACT_ALT_KEY);
	comboDict->SetBool("ctrlKey", combination.modifiers & INTERACT_CONTROL_KEY);
	comboDict->SetBool("commandKey", combination.modifiers & INTERACT_COMMAND_KEY);
	comboDict->SetBool("shiftKey", combination.modifiers & INTERACT_SHIFT_KEY);

	comboDict->SetBool("capsLock", combination.modifiers & INTERACT_CAPS_KEY);
	comboDict->SetBool("numLock", combination.modifiers & INTERACT_NUMLOCK_KEY);
	comboDict->SetBool("mouseLeftButton", combination.modifiers & INTERACT_MOUSE_LEFT);
	comboDict->SetBool("mouseMidButton", combination.modifiers & INTERACT_MOUSE_MIDDLE);
	comboDict->SetBool("mouseRightButton", combination.modifiers & INTERACT_MOUSE_RIGHT);

	dstr_free(&key_str);
	dstr_free(&combo_str);

	return comboDict;
}

void StreamElementsHotkeyManager::hotkey_change_handler(void*, calldata_t*)
{
	StreamElementsCefClient::DispatchJSEvent("hostHotkeyBindingsChanged", "null");
}

static const char* HOTKEY_BINDINGS_CHANGED_SIGNAL_NAMES[] = {
	"hotkey_layout_change",
	"hotkey_register",
	"hotkey_unregister",
	"hotkey_bindings_changed",
	nullptr
};

StreamElementsHotkeyManager::StreamElementsHotkeyManager()
{
	for (size_t i = 0; HOTKEY_BINDINGS_CHANGED_SIGNAL_NAMES[i]; ++i) {
		signal_handler_connect(
			obs_get_signal_handler(),
			HOTKEY_BINDINGS_CHANGED_SIGNAL_NAMES[i],
			hotkey_change_handler,
			this
		);
	}
}

StreamElementsHotkeyManager::~StreamElementsHotkeyManager()
{
	for (size_t i = 0; HOTKEY_BINDINGS_CHANGED_SIGNAL_NAMES[i]; ++i) {
		signal_handler_disconnect(
			obs_get_signal_handler(),
			HOTKEY_BINDINGS_CHANGED_SIGNAL_NAMES[i],
			hotkey_change_handler,
			this
		);
	}
}

bool StreamElementsHotkeyManager::SerializeHotkeyBindings(CefRefPtr<CefValue>& output, bool onlyManagedBindings)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefListValue> rootList = CefListValue::Create();
	output->SetList(rootList);

	using keys_t = std::map<obs_hotkey_id, std::vector<obs_key_combination_t>>;
	keys_t keys;
	obs_enum_hotkey_bindings([](void *data,
		size_t, obs_hotkey_binding_t *binding)
	{
		auto &keys = *static_cast<keys_t*>(data);

		keys[obs_hotkey_binding_get_hotkey_id(binding)].emplace_back(
			obs_hotkey_binding_get_key_combination(binding));

		return true;
	}, &keys);

	// Enum hotkeys
	struct local_hotkey
	{
		obs_hotkey_id hotkey_id;
		obs_hotkey_registerer_t registerer_type_id;
		void *registerer_ptr;
		obs_hotkey_id partner_hotkey_id;
		std::string name;
		std::string description;
		std::string registerer_type;
		std::string registerer;
	};

	std::vector<local_hotkey> data;
	using data_t = decltype(data);
	obs_enum_hotkeys([](void *data, obs_hotkey_id id, obs_hotkey_t *key)
	{
		data_t &d = *static_cast<data_t*>(data);

		local_hotkey item;

		item.hotkey_id = id;
		item.registerer_type_id = obs_hotkey_get_registerer_type(key);
		item.registerer_ptr = obs_hotkey_get_registerer(key);
		item.partner_hotkey_id = obs_hotkey_get_pair_partner_id(key);
		item.name = obs_hotkey_get_name(key);
		item.description = obs_hotkey_get_description(key);

		switch (item.registerer_type_id) {
		case OBS_HOTKEY_REGISTERER_FRONTEND:
			item.registerer_type = "frontend";
			break;

		case OBS_HOTKEY_REGISTERER_ENCODER:
		{
			item.registerer_type = "encoder";
			auto weak = static_cast<obs_weak_encoder_t*>(item.registerer_ptr);
			auto strong = obs_weak_encoder_get_encoder(weak);
			if (strong) {
				item.registerer = obs_encoder_get_name(strong);
			}
			obs_encoder_release(strong);
		}
		break;

		case OBS_HOTKEY_REGISTERER_OUTPUT:
		{
			item.registerer_type = "output";
			auto weak = static_cast<obs_weak_output_t*>(item.registerer_ptr);
			auto strong = obs_weak_output_get_output(weak);
			if (strong) {
				item.registerer = obs_output_get_name(strong);
			}
			obs_output_release(strong);
		}
		break;

		case OBS_HOTKEY_REGISTERER_SERVICE:
		{
			item.registerer_type = "service";
			auto weak = static_cast<obs_weak_service_t*>(item.registerer_ptr);
			auto strong = obs_weak_service_get_service(weak);
			if (strong) {
				item.registerer = obs_service_get_name(strong);
			}
			obs_service_release(strong);
		}
		break;

		case OBS_HOTKEY_REGISTERER_SOURCE:
		{
			item.registerer_type = "source";
			auto weak = static_cast<obs_weak_source_t*>(item.registerer_ptr);
			auto strong = obs_weak_source_get_source(weak);
			if (strong) {
				item.registerer = obs_source_get_name(strong);
			}
			obs_source_release(strong);
		}
		break;
		}

		d.emplace_back(item);

		return true;
	}, &data);

	for (auto item : data) {
		if (onlyManagedBindings && !m_registeredHotkeyIdsToNames.count(item.hotkey_id)) {
			// If only managed bindings requested and this hotkey id is
			// not managed: skip to the next hotkey id.
			continue;
		}

		CefRefPtr<CefDictionaryValue> itemDict = CefDictionaryValue::Create();

		itemDict->SetInt("id", item.hotkey_id);
		itemDict->SetString("name", item.name);
		itemDict->SetString("description", item.description);

		if (m_registeredHotkeyDataString.count(item.hotkey_id)) {
			itemDict->SetValue("eventDetail", CefParseJSON(m_registeredHotkeyDataString[item.hotkey_id], JSON_PARSER_ALLOW_TRAILING_COMMAS));
		}

		itemDict->SetInt("registererTypeId", item.registerer_type_id);
		itemDict->SetString("registererTypeName", item.registerer_type);

		if (item.registerer_type_id != OBS_HOTKEY_REGISTERER_FRONTEND) {
			itemDict->SetString("registererName", item.registerer);
		}

		if (item.partner_hotkey_id != OBS_INVALID_HOTKEY_ID) {
			itemDict->SetInt("partnerHotkeyBindingId", item.partner_hotkey_id);
		}

		CefRefPtr<CefListValue> bindingComboList = CefListValue::Create();

		for (auto combination : keys[item.hotkey_id]) {
			bindingComboList->SetDictionary(
				bindingComboList->GetSize(),
				SerializeKeyCombination(combination));
		}

		itemDict->SetList("triggers", bindingComboList);

		rootList->SetDictionary(rootList->GetSize(), itemDict);
	}

	return true;
}

bool StreamElementsHotkeyManager::DeserializeHotkeyBindings(CefRefPtr<CefValue> input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!input.get()) {
		return false;
	}

	if (input->GetType() != VTYPE_LIST) {
		return false;
	}

	CefRefPtr<CefListValue> list = input->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		DeserializeSingleHotkeyBinding(list->GetValue(index));
	}

	return true;
}

void StreamElementsHotkeyManager::hotkeyTriggered(obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_registeredHotkeyDataString.count(id)) {
		std::string dataString = m_registeredHotkeyDataString[id];

		if (pressed) {
			StreamElementsCefClient::DispatchJSEvent("hostHotkeyPressed", dataString);
		}
		else {
			StreamElementsCefClient::DispatchJSEvent("hostHotkeyReleased", dataString);
		}
	}
}

obs_hotkey_id StreamElementsHotkeyManager::DeserializeSingleHotkeyBinding(CefRefPtr<CefValue> input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (input->GetType() != VTYPE_DICTIONARY) {
		return OBS_INVALID_HOTKEY_ID;
	}

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	if (!root->HasKey("name") || !root->HasKey("description")) {
		return OBS_INVALID_HOTKEY_ID;
	}

	std::string name = root->GetString("name");
	std::string description = root->GetString("description");

	CefRefPtr<CefValue> data = CefValue::Create();

	if (root->HasKey("eventDetail")) {
		data = root->GetValue("eventDetail");
	}
	else {
		data->SetNull();
	}

	if (!name.size() || !description.size()) {
		return false;
	}

	auto hotkeyFunc = [](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
		StreamElementsHotkeyManager* self = (StreamElementsHotkeyManager*)data;

		self->hotkeyTriggered(id, hotkey, pressed);
	};

	obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;

	if (m_registeredHotkeyNamesToHotkeyIds.count(name)) {
		// Existing hotkey binding
		return OBS_INVALID_HOTKEY_ID;
	}
	else {
		// New hotkey

		// Make sure we don't already have a hotkey with the same name registered
		// by another module.
		struct local_context {
			std::string name;
			obs_hotkey_id id;
		};

		local_context context;

		context.name = name;
		context.id = OBS_INVALID_HOTKEY_ID;

		obs_enum_hotkeys([](void *data, obs_hotkey_id id, obs_hotkey_t *key)
		{
			local_context* context = static_cast<local_context*>(data);

			if (obs_hotkey_get_name(key) == context->name) {
				context->id = id;
				return false;
			}

			return true;
		}, &context);

		if (context.id != OBS_INVALID_HOTKEY_ID) {
			// We already have the same hotkey name registered
			return OBS_INVALID_HOTKEY_ID;
		}

		// Register new hotkey
		id = obs_hotkey_register_frontend(
			name.c_str(),
			description.c_str(),
			hotkeyFunc,
			this);
	}

	// Save hotkey name -> id, and id -> configuration
	m_registeredHotkeyNamesToHotkeyIds[name] = id;
	m_registeredHotkeyIdsToNames[id] = name;
	m_registeredHotkeySerializedValues[id] = input;

	// Save hotkey id -> data
	if (data->GetType() == VTYPE_NULL) {
		m_registeredHotkeyDataString[id] = "null";
	}
	else {
		std::string dataString = CefWriteJSON(data, JSON_WRITER_DEFAULT);

		m_registeredHotkeyDataString[id] = dataString;
	}

	std::vector<obs_key_combination_t> combinations;

	if (root->HasKey("triggers") && root->GetValue("triggers")->GetType() == VTYPE_LIST) {
		CefRefPtr<CefListValue> triggersList = root->GetList("triggers");

		if (triggersList.get()) {
			for (size_t index = 0; index < triggersList->GetSize(); ++index) {
				CefRefPtr<CefDictionaryValue> d = triggersList->GetDictionary(index);

				if (!d.get()) {
					continue;
				}

				obs_key_combination_t combination = DeserializeKeyCombination(triggersList->GetValue(index));

				if (!obs_key_combination_is_empty(combination)) {
					combinations.emplace_back(combination);
				}
			}
		}
	}

	// Update hotkey key combinations
	auto AtomicUpdate = [&]()
	{
		obs_hotkey_load_bindings(id,
			combinations.data(), combinations.size());
	};
	using AtomicUpdate_t = decltype(&AtomicUpdate);

	obs_hotkey_update_atomic([](void *d)
	{
		(*static_cast<AtomicUpdate_t>(d))();
	}, static_cast<void*>(&AtomicUpdate));

	return id;
}

bool StreamElementsHotkeyManager::RemoveHotkeyBindingById(obs_hotkey_id id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_registeredHotkeyIdsToNames.count(id)) {
		std::string name = m_registeredHotkeyIdsToNames[id];

		m_registeredHotkeyIdsToNames.erase(id);
		m_registeredHotkeyNamesToHotkeyIds.erase(name);
		m_registeredHotkeySerializedValues.erase(id);
		m_registeredHotkeyDataString.erase(id);

		obs_hotkey_unregister(id);

		return true;
	}

	return false;
}

void StreamElementsHotkeyManager::RemoveAllManagedHotkeyBindings()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto kv : m_registeredHotkeyIdsToNames) {
		obs_hotkey_id id = kv.first;

		obs_hotkey_unregister(id);
	}

	m_registeredHotkeyIdsToNames.clear();
	m_registeredHotkeyNamesToHotkeyIds.clear();
	m_registeredHotkeySerializedValues.clear();
	m_registeredHotkeyDataString.clear();
}

void StreamElementsHotkeyManager::keyCombinationTriggered(CefRefPtr<CefBrowser> browser, obs_key_combination_t combination, bool pressed)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefValue> serialized = CefValue::Create();
	serialized->SetDictionary(SerializeKeyCombination(combination));
	std::string json = CefWriteJSON(serialized, JSON_WRITER_DEFAULT);

	if (pressed) {
		StreamElementsCefClient::DispatchJSEvent(browser, "hostContainerKeyCombinationPressed", json);
	}
	else {
		StreamElementsCefClient::DispatchJSEvent(browser, "hostContainerKeyCombinationReleased", json);
	}
}

#include "StreamElementsConfig.hpp"
#include "Version.hpp"

StreamElementsConfig* StreamElementsConfig::s_instance = nullptr;

StreamElementsConfig::StreamElementsConfig():
	m_config(nullptr)
{

}

StreamElementsConfig::~StreamElementsConfig()
{
	SaveConfig();
}


config_t* StreamElementsConfig::GetConfig()
{
	if (!m_config) {
		config_open(
			&m_config,
			obs_module_config_path(CONFIG_FILE_NAME),
			CONFIG_OPEN_ALWAYS);

		config_set_default_uint(m_config, "Header", "Version", STREAMELEMENTS_PLUGIN_VERSION);
		config_set_default_uint(m_config, "Startup", "Flags", STARTUP_FLAGS_ONBOARDING_MODE);
		config_set_default_string(m_config, "Startup", "State", "");
	}

	return m_config;
}

void StreamElementsConfig::SaveConfig()
{
	if (!m_config) return;

	config_set_uint(m_config, "Header", "Version", STREAMELEMENTS_PLUGIN_VERSION);

	config_save_safe(m_config, "tmp", "bak");
}

std::string StreamElementsConfig::GetHeapAnalyticsAppId()
{
	std::string result = "413792583";

	const char* REG_KEY_PATH = "SOFTWARE\\StreamElements";
	const char* REG_VALUE_NAME = "HeapAnalyticsAppId";

	DWORD bufLen = 16384;
	char* buffer = new char[bufLen];

	if (ERROR_SUCCESS != RegGetValueA(
		HKEY_LOCAL_MACHINE,
		REG_KEY_PATH,
		REG_VALUE_NAME,
		RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
		NULL,
		buffer,
		&bufLen)) {
		if (ERROR_SUCCESS == RegGetValueA(
			HKEY_LOCAL_MACHINE,
			REG_KEY_PATH,
			REG_VALUE_NAME,
			RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY,
			NULL,
			buffer,
			&bufLen)) {
			if (buffer[0]) {
				result = buffer;
			}
		}
	}
	else {
		if (buffer[0]) {
			result = buffer;
		}
	}

	delete[] buffer;

	return result;
}

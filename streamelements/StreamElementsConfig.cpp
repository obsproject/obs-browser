#include "StreamElementsConfig.hpp"

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

		config_set_default_uint(m_config, "Startup", "Flags", STARTUP_FLAGS_ONBOARDING_MODE);
		config_set_default_string(m_config, "Startup", "State", "");
	}

	return m_config;
}

void StreamElementsConfig::SaveConfig()
{
	if (!m_config) return;

	config_save(m_config);
}

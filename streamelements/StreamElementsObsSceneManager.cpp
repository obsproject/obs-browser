#include "StreamElementsObsSceneManager.hpp"

#include <util/platform.h>

static bool IsSceneItemInfoValid(CefRefPtr<CefValue>& input, bool requireClass)
{
	if (input->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	if (!root->HasKey("name") || root->GetType("name") != VTYPE_STRING || root->GetString("name").empty()) {
		return false;
	}

	if (requireClass) {
		if (!root->HasKey("class") || root->GetType("class") != VTYPE_STRING || root->GetString("class").empty()) {
			return false;
		}
	}

	if (!root->HasKey("settings") || root->GetType("settings") != VTYPE_DICTIONARY) {
		return false;
	}

	return true;
}

static bool IsBrowserSourceSceneItemInfoValid(CefRefPtr<CefValue>& input)
{
	if (!IsSceneItemInfoValid(input, false)) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary()->GetDictionary("settings");

	if (!d->HasKey("url") || d->GetType("url") != VTYPE_STRING || d->GetString("url").empty()) {
		return false;
	}

	if (!d->HasKey("width") || d->GetType("width") != VTYPE_INT || d->GetInt("width") <= 0) {
		return false;
	}

	if (!d->HasKey("height") || d->GetType("height") != VTYPE_INT || d->GetInt("height") <= 0) {
		return false;
	}

	if (!d->HasKey("fps") || d->GetType("fps") != VTYPE_INT || d->GetInt("fps") <= 0) {
		return false;
	}

	if (!d->HasKey("shutdown") || d->GetType("shutdown") != VTYPE_BOOL) {
		return false;
	}

	if (!d->HasKey("restart_when_active") || d->GetType("restart_when_active") != VTYPE_BOOL) {
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////

static CefRefPtr<CefValue> SerializePropsAndData(obs_properties_t* props, obs_data_t* data)
{
	return 	CefParseJSON(
		obs_data_get_json(data),
		JSON_PARSER_ALLOW_TRAILING_COMMAS);
}

static CefRefPtr<CefValue> SerializeObsSourceSettings(obs_source_t* source)
{
	obs_properties_t* props = obs_source_properties(source);
	obs_data_t* data = obs_source_get_settings(source);

	CefRefPtr<CefValue> result = SerializePropsAndData(props, data);

	obs_data_release(data);
	obs_properties_destroy(props);

	return result;
}

static CefRefPtr<CefDictionaryValue> SerializeVec2(vec2& vec)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetDouble("x", vec.x);
	d->SetDouble("y", vec.y);

	return d;
}

static CefRefPtr<CefDictionaryValue> SerializeObsSceneItemCompositionSettings(
	obs_source_t* source,
	obs_sceneitem_t* sceneitem)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetInt("srcWidth", obs_source_get_width(source));
	d->SetInt("srcHeight", obs_source_get_height(source));

	vec2 pos;
	obs_sceneitem_get_pos(sceneitem, &pos);

	d->SetDictionary("position", SerializeVec2(pos));

	vec2 scale;
	obs_sceneitem_get_scale(sceneitem, &scale);

	d->SetDictionary("scale", SerializeVec2(scale));

	d->SetDouble("rotationDegrees", (double)obs_sceneitem_get_rot(sceneitem));

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(sceneitem, &crop);

	CefRefPtr<CefDictionaryValue> cropInfo = CefDictionaryValue::Create();

	cropInfo->SetInt("left", crop.left);
	cropInfo->SetInt("top", crop.top);
	cropInfo->SetInt("right", crop.right);
	cropInfo->SetInt("bottom", crop.bottom);

	d->SetDictionary("crop", cropInfo);

	return d;
}

static void SerializeSourceAndSceneItem(
	CefRefPtr<CefValue>& result,
	obs_source_t* source,
	obs_sceneitem_t* sceneitem)
{
	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();

	root->SetString("name", obs_source_get_name(source));
	root->SetString("class", obs_source_get_id(source));

	root->SetValue("settings", SerializeObsSourceSettings(source));
	root->SetDictionary("composition", SerializeObsSceneItemCompositionSettings(source, sceneitem));

	result->SetDictionary(root);
}

///////////////////////////////////////////////////////////////////////

StreamElementsObsSceneManager::StreamElementsObsSceneManager(QMainWindow* parent) :
	m_parent(parent)
{
}

StreamElementsObsSceneManager::~StreamElementsObsSceneManager()
{
}

void StreamElementsObsSceneManager::ObsAddSourceInternal(
	obs_source_t* parentScene,
	const char* sourceId,
	const char* sourceName,
	obs_data_t* sourceSettings,
	obs_data_t* sourceHotkeyData,
	bool preferExistingSource,
	obs_source_t** output_source,
	obs_sceneitem_t** output_sceneitem)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	bool releaseParentScene = false;

	if (parentScene == NULL) {
		parentScene = obs_frontend_get_current_scene();

		releaseParentScene = true;
	}

	obs_source_t* source = NULL;

	if (preferExistingSource)
	{
		// Try locating existing source of the same type for reuse
		//
		// This is especially relevant for video capture sources
		//

		struct enum_sources_args {
			const char* id;
			obs_source_t* result;
		};

		enum_sources_args enum_args = {};
		enum_args.id = sourceId;
		enum_args.result = NULL;

		obs_enum_sources([](void* arg, obs_source_t* iterator) {
			enum_sources_args* args = (enum_sources_args*)arg;

			const char* id = obs_source_get_id(iterator);

			if (strcmp(id, args->id) == 0) {
				args->result = obs_source_get_ref(iterator);

				return false;
			}

			return true;
		},
		&enum_args);

		source = enum_args.result;
	}

	if (source == NULL)
	{
		// Not reusing an existing source, create a new one
		source = obs_source_create(sourceId, sourceName, NULL, sourceHotkeyData);

		QApplication::processEvents();

		obs_source_update(source, sourceSettings);
	}

	// Wait for dimensions: some sources like video capture source do not
	// get their dimensions immediately: they are initializing asynchronously
	// and are not aware of the source dimensions until they do.
	//
	// We'll do this for maximum 15 seconds and give up.
	for (int i = 0; i < 150 && obs_source_get_width(source) == 0; ++i) {
		os_sleep_ms(100);
	}

	// Does not increment refcount. No obs_scene_release() call is necessary.
	obs_scene_t* scene = obs_scene_from_source(parentScene);

	struct atomic_update_args {
		obs_source_t* source;
		obs_sceneitem_t* sceneitem;
	};

	atomic_update_args args = {};

	args.source = source;
	args.sceneitem = NULL;

	obs_enter_graphics();
	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *scene) {
			atomic_update_args* args = (atomic_update_args*)data;

			args->sceneitem = obs_scene_add(scene, args->source);
			obs_sceneitem_set_visible(args->sceneitem, true);

			// obs_sceneitem_release??
		},
		&args);
	obs_leave_graphics();

	if (output_sceneitem != NULL) {
		obs_sceneitem_addref(args.sceneitem);

		*output_sceneitem = args.sceneitem;
	}

	if (output_source != NULL) {
		*output_source = source;
	}
	else {
		obs_source_release(source);
	}

	if (releaseParentScene)
	{
		obs_source_release(parentScene);
	}
}

std::string StreamElementsObsSceneManager::ObsGetUniqueSourceName(std::string name)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string result(name);

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique) {
		isUnique = true;

		obs_source_t* source = obs_get_source_by_name(result.c_str());
		if (source != NULL) {
			obs_source_release(source);

			isUnique = false;
		}

		if (!isUnique) {
			++sequence;

			char buf[32];
			result = name + " ";
			result += itoa(sequence, buf, 10);
		}
	}

	return result;
}

void StreamElementsObsSceneManager::DeserializeObsBrowserSource(CefRefPtr<CefValue>& input, CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsBrowserSourceSceneItemInfoValid(input)) {
		return;
	}

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	CefRefPtr<CefDictionaryValue> d = root->GetDictionary("settings");

	// Get parameter values

	std::string unique_name = ObsGetUniqueSourceName(root->GetString("name").ToString());
	std::string source_class = "browser_source";

	std::string url = d->GetString("url").ToString();
	std::string css =
		(d->HasKey("css") && d->GetType("css") == VTYPE_STRING)
		? d->GetString("css").ToString()
		: "";
	int width = d->GetInt("width");
	int height = d->GetInt("height");
	int fps = d->GetInt("fps");
	bool shutdown = d->GetBool("shutdown");
	bool restart_when_active = d->GetBool("restart_when_active");

	// Add browser source

	obs_source_t* parent_scene = nullptr; // user default: current scene

	obs_data_t* settings = obs_data_create();

	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "url", url.c_str());
	obs_data_set_string(settings, "css", css.c_str());
	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_int(settings, "fps", fps);
	obs_data_set_bool(settings, "shutdown", shutdown);
	obs_data_set_bool(settings, "restart_when_active", restart_when_active);

	obs_source_t* source;
	obs_sceneitem_t* sceneitem;

	ObsAddSourceInternal(parent_scene, source_class.c_str(), unique_name.c_str(), settings, nullptr, false, &source, &sceneitem);

	// Result
	SerializeSourceAndSceneItem(output, source, sceneitem);

	obs_sceneitem_release(sceneitem);
	obs_source_release(source);

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::SerializeObsCurrentSceneItems(CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	// Get list of scenes
	obs_source_t* sceneSource =
		obs_frontend_get_current_scene();

	if (!sceneSource) {
		return;
	}

	// Get scene handle
	obs_scene_t* scene = obs_scene_from_source(sceneSource); // does not increment refcount

	if (scene) {
		struct local_context {
			CefRefPtr<CefListValue> list;
		};

		local_context context;

		context.list = CefListValue::Create();

		// For each scene item
		obs_scene_enum_items(
			scene,
			[](obs_scene_t* scene, obs_sceneitem_t* sceneitem, void* param) {
				local_context* context = (local_context*)param;

				obs_source_t* source =
					obs_sceneitem_get_source(sceneitem); // does not increase refcount

				CefRefPtr<CefValue> item = CefValue::Create();

				SerializeSourceAndSceneItem(item, source, sceneitem);

				context->list->SetValue(context->list->GetSize(), item);

				// Continue iteration
				return true;
			},
			&context);

		output->SetList(context.list);
	}

	obs_source_release(sceneSource);
}

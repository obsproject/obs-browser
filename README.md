# obs-browser

CEF Based obs-studio browser plugin

## JS Bindings

obs-browser provides a global object that allows access to some obs specific functionality from javascript.

### Get OBS Studio Browser Plugin Version
```
window.obsstudio.pluginVersion
// => 1.24.0
```

### Register for visibility callbacks
```
/**
 * onVisibilityChange gets callbacks when the visibility of the browser source changes in OBS
 *
 * @param {bool} visiblity - True -> visible, False -> hidden
 */
window.obsstudio.onVisibilityChange = function(visiblity) {
	
};
```

### Register for scene change callbacks
```
/**
 * onSceneChange gets callbacks when the scene is changed
 *
 * @param {string} sceneName - The name of the scene that was changed to.
 */
window.obsstudio.onSceneChange = function(sceneName) {
	
};
```
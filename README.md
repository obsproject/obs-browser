# obs-browser

CEF Based obs-studio browser plugin

## JS Bindings

obs-browser provides a global object that allows access to some obs specific functionality from javascript.

### Get OBS Studio Version
```
window.obsstudio.version
// => 0.14.2
```

### Register for visibility callbacks
```
/**
 * onVisibilityChange gets callbacks when the visibility of the browser source changes in OBS
 *
 * @param {bool} visiblity - True -> visible, False -> hidden
 */
window.obsstudio.onVisibilityChange(visiblity) {
	
};
```
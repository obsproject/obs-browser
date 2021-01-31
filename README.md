# obs-browser

obs-browser introduces a cross-platform Browser Source, powered by CEF ([Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef/src/master/README.md)), to OBS Studio. A Browser Source allows the user to integrate web-based overlays into their scenes, with complete access to modern web APIs.

On Windows, this also adds support for Service Integration (linking third party services) and Browser Docks (webpages loaded into the interface itself). macOS support for service integration & browser docks is in the works, and Linux support is planned.

**This plugin is included by default** on official packages on Windows and macOS. While Linux is supported, the official ppa does not currently include the browser source [due to a conflict with GTK](https://github.com/obsproject/obs-browser/issues/219).

## JS Bindings

obs-browser provides a global object that allows access to some OBS-specific functionality from JavaScript. This can be used to create an overlay that adapts dynamically to changes in OBS.

### Get Browser Plugin Version

```js
/**
 * @returns {number} OBS Browser plugin version
 */
window.obsstudio.pluginVersion
// => 1.24.0
```

### Register for event callbacks

```js
/**
 * @typedef {Object} OBSEvent
 * @property {object} detail - data from event
 */

window.addEventListener('obsSceneChanged', function(event) {
	var t = document.createTextNode(event.detail.name)
	document.body.appendChild(t)
})
```

#### Available events

Descriptions for these events can be [found here](https://obsproject.com/docs/reference-frontend-api.html?highlight=paused#c.obs_frontend_event).

* obsSceneChanged
* obsSourceVisibleChanged
* obsSourceActiveChanged
* obsStreamingStarting
* obsStreamingStarted
* obsStreamingStopping
* obsStreamingStopped
* obsRecordingStarting
* obsRecordingStarted
* obsRecordingPaused
* obsRecordingUnpaused
* obsRecordingStopping
* obsRecordingStopped
* obsReplaybufferStarting
* obsReplaybufferStarted
* obsReplaybufferSaved
* obsReplaybufferStopping
* obsReplaybufferStopped
* obsVirtualcamStarted
* obsVirtualcamStopped
* obsExit

### Get the current scene

```js
/**
 * @typedef {Object} Scene
 * @property {string} name - name of the scene
 * @property {number} width - width of the scene
 * @property {number} height - height of the scene
 */

/**
 * @param {function} callback
 * @returns {Scene}
 */
window.obsstudio.getCurrentScene(function(scene) {
	console.log(scene)
})
```

### Get OBS output status

```js
/**
 * @typedef {Object} Status
 * @property {boolean} recording - not affected by pause state
 * @property {boolean} recordingPaused
 * @property {boolean} streaming
 * @property {boolean} replaybuffer
 * @property {boolean} virtualcam
 */

/**
 * @param {function} callback
 * @returns {Status}
 */
window.obsstudio.getStatus(function (status) {
	console.log(status)
})
```

### Save OBS Replay Buffer

```js
/**
 * Does not accept any parameters and does not return anything
 */
window.obsstudio.saveReplayBuffer()
```

### Register for visibility callbacks

**This method is legacy. Register an event listener instead.**

```js
/**
 * onVisibilityChange gets callbacks when the visibility of the browser source changes in OBS
 *
 * @deprecated
 * @see obsSourceVisibleChanged
 * @param {boolean} visibility - True -> visible, False -> hidden
 */
window.obsstudio.onVisibilityChange = function(visibility) {
	
};
```

### Register for active/inactive callbacks

**This method is legacy. Register an event listener instead.**

```js
/**
 * onActiveChange gets callbacks when the active/inactive state of the browser source changes in OBS
 *
 * @deprecated
 * @see obsSourceActiveChanged
 * @param {bool} True -> active, False -> inactive
 */
window.obsstudio.onActiveChange = function(active) {
	
};
```

## Building

OBS Browser cannot be built standalone. It is built as part of OBS Studio.

By following the instructions, this will enable Browser Source & Custom Browser Docks on all three platforms. Both `BUILD_BROWSER` and `CEF_ROOT_DIR` are required.

### On Windows

Follow the [build instructions](https://obsproject.com/wiki/Install-Instructions#windows-build-directions) and be sure to download the **CEF Wrapper** and set `CEF_ROOT_DIR` in CMake to point to the extracted wrapper.

### On macOS

Use the [macOS Full Build Script](https://obsproject.com/wiki/Install-Instructions#macos-build-directions). This will automatically download & enable OBS Browser.

### On Linux

Follow the [build instructions](https://obsproject.com/wiki/Install-Instructions#linux-build-directions) and choose the "If building with browser source" option. This includes steps to download/extract the CEF Wrapper, and set the required CMake variables.

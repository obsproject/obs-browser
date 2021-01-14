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

## Building on OSX

### Building CEF

#### Getting

*  Download CEF Mac 64 from [https://cef-builds.spotifycdn.com/index.html](https://cef-builds.spotifycdn.com/index.html)
    *  Use CEF branch 3770
*  Extract and cd into the folder

#### Setting Up Project

```shell
mkdir build
cmake -D CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY=libc++ -G Xcode ..
open cef.xcodeproj/
```

#### Building

Build in Xcode (⌘+B)

TODO: tell user to move stuff, or update FindCEF.cmake

### Building OBS and obs-browser

#### Installing Dependencies

```shell
brew install ffmpeg x264 qt5 cmake
```

#### Setting Up Project

```shell
git clone --recursive https://github.com/jp9000/obs-studio.git
cd ./obs-studio
git clone git@github.com:kc5nra/obs-browser.git ./plugins/obs-browser
echo "add_subdirectory(obs-browser)" >> ./plugins/CMakeLists.txt
mkdir build
cd ./build
cmake -D CMAKE_PREFIX_PATH=/usr/local/opt/qt5/lib/cmake -D CEF_ROOT_DIR=/Users/username/Development/cef_binary_3.2883.1540.gedbfb20_macosx64 -D BUILD_BROWSER=yes -G Xcode ..
open obs-studio.xcodeproj/
```

#### Building

Build in Xcode (⌘+B)

## Building on Windows

### Building CEF

#### Getting

*  Download CEF Windows 64bit from [https://cef-builds.spotifycdn.com/index.html](https://cef-builds.spotifycdn.com/index.html)
  *  Use CEF branch 3770
*  Extract and cd into the folder

#### Setting Up the Project

* Run cmake-gui
  * In "where is the source code", enter in the repo directory (example: C:/Users/User/Desktop/cef_binary_3.2743.1445.gdad7c0a_windows64).
  * In "where to build the binaries", enter the repo directory path with the 'build' subdirectory (example: C:/Users/User/Desktop/cef_binary_3.2743.1445.gdad7c0a_windows64/build).
* Press 'Configure' and select the generator that fits to your installed VS Version:
Visual Studio 12 2013 Win64, Visual Studio 14 2015 Win64 or Visual Studio 15 2017 Win64
* Press 'Generate' to generate Visual Studio project files in the 'build' subdirectory.
* Open cef.sln from the 'build' subdirectory

#### Building

* Build at least libcef_dll_wrapper (as Release), the rest is optional and are just clients to test with

### Building OBS and obs-browser

#### Follow the OBS build instructions

[https://github.com/jp9000/obs-studio/wiki/Install-Instructions#windows](https://github.com/jp9000/obs-studio/wiki/Install-Instructions#windows)

#### Setting Up Project

* Enable BUILD_BROWSER and set the CEF_ROOT_DIR path in cmake-gui for obs-studio (example: C:/Users/User/Desktop/cef_binary_3.2743.1445.gdad7c0a_windows64)
* * Press 'Generate' to generate Visual Studio project files in the 'build' subdirectory.
* Open obs-studio.sln from the 'build' subdirectory

#### Building

Build in Visual Studio

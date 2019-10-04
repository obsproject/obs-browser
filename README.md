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
 * @param {bool} visibility - True -> visible, False -> hidden
 */
window.obsstudio.onVisibilityChange = function(visibility) {
	
};
```

### Register for active/inactive callbacks
```
/**
 * onActiveChange gets callbacks when the active/inactive state of the browser source changes in OBS
 *
 * @param {bool} True -> active, False -> inactive
 */
window.obsstudio.onActiveChange = function(active) {
	
};
```

### Register for scene change callbacks
```
window.addEventListener('obsSceneChanged', function(evt) {
	var t = document.createTextNode(evt.detail.name);
    document.body.appendChild(t);
});
```
#### Other events that are available
* obsStreamingStarting
* obsStreamingStarted
* obsStreamingStopping
* obsStreamingStopped
* obsRecordingStarting
* obsRecordingStarted
* obsRecordingStopping
* obsRecordingStopped

### Get the current scene
```
window.obsstudio.getCurrentScene(function(data) { console.log(data); });

data is a Javascript object containing the following properties:
* width
* height
* name
```

### Get OBS output status
```
window.obsstudio.getStatus(function data) { console.log(data); });

data is a Javascript object containing the following properties:
* recording (true/false)
* streaming (true/false)
* replaybuffer (true/false)
```

## Building on OSX

### Building CEF
#### Getting
*  Download CEF Mac 64 from [http://opensource.spotify.com/cefbuilds/index.html](http://opensource.spotify.com/cefbuilds/index.html)
  *  Use CEF branch 3112
*  Extract and cd into the folder

#### Setting Up Project
```
mkdir build
cmake -D CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY=libc++ -G Xcode ..
open cef.xcodeproj/
```

#### Building
Build in Xcode (⌘+B)

TODO: tell user to move stuff, or update FindCEF.cmake

### Building OBS and obs-browser
#### Installing Dependencies
```
brew install ffmpeg x264 qt5 cmake
```

#### Setting Up Project
```
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
*  Download CEF Windows 64bit from [http://opensource.spotify.com/cefbuilds/index.html](http://opensource.spotify.com/cefbuilds/index.html)
  *  Use CEF branch 3440 or newer (3579 if you want shared texture support)
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

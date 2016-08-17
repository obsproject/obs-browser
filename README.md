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
window.obsstudio.onVisibilityChange(visiblity) {
	
};
```

## Building on OSX

### Building CEF
#### Getting
*  Download CEF Mac 64 from [https://cefbuilds.com/](https://cefbuilds.com/)
  *  At the time of writing this I used build 2704
*  Extract and cd into the folder

#### Setting Up Project
```
mkdir build
cmake -D CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY=libc++ -G Xcode ..
open cef.xcodeproj/
```

TODO: tell user to move stuff, or FindCEF.cmake

#### Building
Build in Xcode (⌘+B)

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
cmake -D CMAKE_PREFIX_PATH=/usr/local/opt/qt5/lib/cmake -D CEF_ROOT_DIR=/Users/username/Development/cef_binary_3.2704.1434.gec3e9ed_macosx64 -G Xcode ..
open obs-studio.xcodeproj/
```

#### Building
Build in Xcode (⌘+B)

# Ultralight SDK

Ultralight is a lightweight, universal web renderer for C++.

Please visit our [website](https://ultralig.ht) for licensing information.

## Prerequisites

Before building the Ultralight SDK, ensure you have the following:

 * CMake (version 3.15 or later)
 * A C++20 compatible compiler

### Windows-specific Requirements

If building on Windows without VS 2022, you'll also need:

 * VS 2022 Redistributable: https://aka.ms/vs/17/release/vc_redist.x64.exe

## Building Samples and Tools

The SDK includes a number of samples and tools that can be built using CMake.

### Building via CMake

Run the following command in the root of the Ultralight SDK directory to build using the default generator:

```
cmake -B build && cmake --build build --config Release && cmake --install build --config Release
```

Build output can be found in the `build/out` directory.

### Building with Visual Studio Code

The SDK includes Visual Studio Code integration for convenient development:

1. Open the SDK folder in VS Code
2. Install the recommended extensions when prompted (C/C++ and CMake Tools)
3. Select your preferred build preset (Release or Debug) when prompted
4. Build the project using one of these methods:
   - Click the "Build" button in the CMake status bar
   - Run the "Build (Release)" or "Build (Debug)" task from the Terminal menu
   - Press F7 to build with the default configuration

#### Running Samples in VS Code

After building, you can run and debug any sample:
- Open the "Run and Debug" view (Ctrl+Shift+D)
- Select the sample you want to run from the dropdown
- Press F5 to start debugging or Ctrl+F5 to run without debugging

Each sample is pre-configured with the correct launch settings for Windows, macOS, and Linux.

## Using the SDK from CMake

The SDK ships a CMake package definition. Point CMake at the SDK root and link
the imported targets:

```
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/ultralight-sdk
```

Write the path with forward slashes on Windows too (`C:/ultralight-sdk`): inside a
`CMakeLists.txt` or a preset a backslash is an escape character, and some shells mangle
backslashed paths before CMake sees them.
`-DUltralight_DIR=/path/to/ultralight-sdk/lib/cmake/Ultralight` also works.

```cmake
find_package(Ultralight REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Ultralight::AppCore)
ultralight_copy_runtime_files(my_app)
```

`Ultralight::AppCore` brings in `Ultralight::Ultralight`, `Ultralight::UltralightCore`,
and `Ultralight::WebCore`. Link `Ultralight::Ultralight` instead to use the renderer
without the application framework. `ultralight_copy_runtime_files()` copies the
libraries next to the built executable and the `resources` directory to
`assets/resources` beside it.

## Versions

`VERSION.txt` holds the version of this SDK package. Pre-release packages carry a
suffix of the form `-alpha.N`, `-beta.N`, or `-rc.N` (e.g. `2.0.0-beta.1`).

`ULTRALIGHT_VERSION` in `include/Ultralight/Defines.h` holds the version of the library
itself, which tracks the API and omits any package suffix. The version
`find_package(Ultralight)` matches against is kept in step with it.

## Using the Library

For information using the library in your application, please visit our [online docs](https://ultralig.ht/docs).

## Useful Links

| Link                       | URL                                   |
| -------------------------- | ------------------------------------- |
| __Website__                | <https://ultralig.ht>                 |
| __Join our Discord!__      | <https://chat.ultralig.ht>            |
| __Documentation__          | <https://ultralig.ht/docs>            | 
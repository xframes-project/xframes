# React bindings for Dear ImGui (and ImPlot)

Please note that only Chrome, Edge and [Firefox Nightlies](https://www.mozilla.org/en-US/firefox/channel/desktop/) are currently supported.

## Building instructions

We provide instructions for manual setup on Windows as well as a devcontainer configuration. We recommend to use the Dev Container approach or JetBrain's CLion:

![React Dear Imgui Electron demo](/screenshots/dear-imgui/clion-toolchain.png?raw=true)

### Requirements for devcontainer-based setup (massive kudos to [@genautz](https://github.com/genautz))

We recommend you let VS Code clone the repo in a separate Docker volume

- [VS Code](https://code.visualstudio.com/)
    -  Install extension `Dev Containers`, Id: `ms-vscode-remote.remote-containers` [Marketplace Link](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
    -  Install extension `WSL` Id: `ms-vscode-remote.remote-wsl` [Marketplace Link](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl)
- [Docker Desktop](https://www.docker.com/products/docker-desktop/)

#### Building (Dev Container)

-   `cd packages/dear-imgui/cpp`
-   `cmake -S . -B build`
-   `cmake --build ./build --target reactDearImgui`

Note: no need to run `emsdk_env` as this has already been added to `.bash_profile`

----

### Requirements for manual setup

Tested with:

-   Windows 11
-   Visual Studio 2022
-   emscripten 3.1.60, follow these instructions https://emscripten.org/docs/getting_started/downloads.html
-   Node.js v20 (LTS) download it from https://nodejs.org/dist/v20.13.1/node-v20.13.1-x64.msi or install via [Node Version Manager](https://github.com/coreybutler/nvm-windows)
-   Yarn, follow these instructions https://yarnpkg.com/getting-started/install

#### Manual installation of Emscripten

-   Open a VS Studio 2022 developer prompt
-   Run `git submodule update --init --recursive` to retrieve all dependencies in the `deps` folder
-   Install emsdk:
    -   `git clone https://github.com/emscripten-core/emsdk.git`
    -   `cd emsdk`
    -   `./emsdk install 3.1.60` (`emsdk.bat` on Windows)
    -   `./emsdk activate 3.1.60` (`emsdk.bat` on Windows)
    -   cd into `emsdk/upstream/emscripten` and run `npm install` (this ensures `tsc` is available so that TypeScript typings can be generated)

#### Manual installation of React and other dependencies

-   `cd react-wasm/packages/dear-imgui/ts`
-   `npm i react@18.2.0 react-dom@18.2.0` (you may delete the package-lock.json file generated as part of running this command)
-   `yarn`

#### Building (on Windows)

-   Open a VS Studio 2022 developer prompt
-   **IMPORTANT**: Run `<emsdk-dir>/emsdk_env.bat` (if you haven't done so already)
-   `cd react-wasm/packages/dear-imgui/`
-   Build the WASM:
    -   `cd cpp`
    -   `cmake -S . -B build -GNinja`
    -   `cmake --build ./build --target reactDearImgui`

## Verifying

-   `cd react-wasm/packages/dear-imgui/ts`
-   `yarn start`

Your default browser will open, the URL should http://localhost:3000

Please note, not all browsers are currently supported.

## Notes

Terminating the WASM process throws an `ExitStatus` exception - this is expected, so long as the status is `0` then it's all good.

`Pthread` support was recently added. At the time of writing this causes Webpack to issue a warning on startup. The warning can be dismissed by clicking on the close (X) button. Enabling the `-s STRICT` linker flag should fix it, however this currently introduces our issues.

## Tests

### Unit tests (Google Test)

#### Windows

Open an x64 Developer Command Prompt then run the following commands:

- `cd tests`
- `cmake -S . -B build`
- `cmake --build ./build --target Google_tests_run`
- `build\Debug\Google_Tests_run.exe`

For convenience, you can run the `build_and_run_tests.bat` batch script to run all the above.
Also, `run_tests.bat` runs just the last two commands (this is useful when making changes to existing files only).

#### Linux

Run the following commands from a terminal:

- `cmake -S . -B build`
- `cd build`
- `make`
- `./Google_Tests_run`

For convenience, you can run the `build_and_run_tests.sh` shell script to run all the above.


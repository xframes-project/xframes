# XFrames - DOM-free GUI development

## Motivation

I've always found DOM-free GUI development very interesting conceptually but I never really needed to use it - until relatively recently.

I invested quite a bit of time and effort into figuring out how [WebAssembly](https://webassembly.org/) and [Emscripten](https://emscripten.org/index.html) work. I eventually managed to compile the [Dear ImGui](https://github.com/ocornut/imgui) emscripten example and modified it to suit my requirements. Given my lack of experience with C/C++, I can tell you that this has been far from being a straightforward process. I have eventually realized that someone else out there might benefit from this experience of mine.

As I write these paragraphs, I realise that, despite accomplishing a few small initial goals, there is still lot to do. I hope you find these libraries useful, if anything just to get yourself acquainted with WebAssembly, C/C++, GLFW, OpenGL.

## Supported platforms

| Architecture      | OS                      | Compiler           | Notes                                           |  
| ----------------- | ----------------------- | ------------------ | ----------------------------------------------- |
| wasm32-emscripten | N/A                     | emsdk 3.1.64       | Works in browsers that support WebGPU rendering |
| x64-windows       | Windows 11 Home         | Visual Studio 2022 | Works                                           |
| x64-linux         | WSL2 Ubuntu 24.02.1 LTS | gcc 13.2.0         | Works by setting `export GALLIUM_DRIVER=d3d12`  |
| x64-linux         | Fedora 41               | gcc 14             | WIP                                             |
| x64-linux         | Debian Trixie           | gcc 14             | WIP                                             |


## Caveats

### Overall quality

I work on this project during my spare time so the overall quality is ... suboptimal. Please bear with me while I make the necessary improvements.

Due of my limited expertise with C/C++, you might come across bugs - and the code may not be particularly performant.

On the plus side, although still lacking, I added some unit tests for the C++ layer.

### Performance

As the rendering is delegated to the GPU, the overall performance should be at the very least half-decent. That said, I am still getting familiar with the more advanced data structures available in C++. 

I still need to benchmark/compare the differences in terms performance between WASM and the native Node module.

### Accessibility

GUI libraries such as [egui](https://github.com/emilk/egui) are a very good example of accessible non-DOM based GUIs. The overall impression I get is that DOM-based GUIs are more accessible. Perhaps this project will foster interest in the topic and motivate people and/or companies to invest more in this area.

### Support for other frameworks

At the moment I am focusing on bindings for React only. The renderer is actually adapted from react-native's Fabric renderer.
Perhaps there are other options I could/should have considered. Feel free to let me know your thoughts.

## Basic online demo

([online demo](https://andreamancuso.github.io/react-wasm/dear-imgui)) Only browsers that natively support WebGPU: Chrome, Edge, Firefox nightly, possibly Safari (though I have not tested it).

## Screenshots

<video src='https://github.com/user-attachments/assets/61fbc418-a419-4bdc-8202-50ff16c5ee56' style="width:90%"></video>

![React Dear Imgui screenshot](https://github.com/user-attachments/assets/1512b95f-640d-4555-8a4b-57ad08119876)

![React Dear Imgui screenshot](https://github.com/user-attachments/assets/1a9b8ae9-d529-45af-ab7b-7e173799136f)

![React Dear Imgui screenshot 4](/screenshots/dear-imgui/screenshot-react-wasm-dear-imgui-sample-code.png?raw=true)

![React Dear Imgui Electron demo](/screenshots/dear-imgui/electron-demo.png?raw=true)

## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/genautz"><img src="https://avatars.githubusercontent.com/u/89743955?v=4?s=100" width="100px;" alt="genautz"/><br /><sub><b>genautz</b></sub></a><br /><a href="https://github.com/andreamancuso/react-wasm/commits?author=genautz" title="Code">💻</a> <a href="https://github.com/andreamancuso/react-wasm/commits?author=genautz" title="Documentation">📖</a> <a href="#platform-genautz" title="Packaging/porting to new platform">📦</a> <a href="#tool-genautz" title="Tools">🔧</a> <a href="#infra-genautz" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a></td>
    </tr>
  </tbody>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind welcome!

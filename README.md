# XFrames - cross-platform DOM-free GUI development for the desktop and the browser

**XFrames** is a high-performance library that empowers developers to build native desktop applications using whichever programming language they are familiar with, without the overhead of the DOM. XFrames was originally conceived as a streamlined alternative to Electron, designed for developers looking to maximize performance and efficiency. 

## Key Features

- **DOM-free rendering**: Bypasses the traditional DOM, providing a faster, more lightweight environment for GUI development.
- **Cross-platform support**: Easily create applications for multiple platforms, including the browser through WebAssembly, while maintaining a native feel.
- **Advanced layout capabilities**: Leverage the power and flexibility of [Yoga layouts](https://www.yogalayout.dev/).

## Getting started

### Node.js

At the moment the easiest way to get started on Node.js is to run

```
npx create-xframes-node-app
```

Enter a name for your project then wait until source files and dependencies are installed, then

```
cd <project-name>
npm start
```

You should see the following: 

![alt text](screenshots/dear-imgui/create-xframes-node-hello-world.png)

## FAQs

Please refer to our [project website's FAQ](https://xframes.dev/faq).

## Core Technologies

Please refer to the ['technologies used'](https://xframes.dev/technologies-used) section of our project website.

## Roadmap

XFrames is actively maintained with planned features and enhancements. The focus at the moment is to add support for more widgets and programming languages.


## Supported platforms

### Node-API v9

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| wasm32-emscripten | N/A                                          |  Works in browsers that support WebGPU rendering                   |
| x64-windows       | Windows 11 Home                              |  Works                                                             |
| x64-linux         | WSL2 (Ubuntu) 24.04.1 LTS                    |  Works by setting `export GALLIUM_DRIVER=d3d12`                    |
| x64-linux         | Debian Trixie                                |  Works                                                             |
| x64-linux         | Ubuntu 22.04 LTS                             |  Works                                                             |
| x64-linux         | Ubuntu 24.04 LTS                             |  Works                                                             |
| arm64-linux       | Raspberry Pi OS (Debian bookworm) Oct 2024   |  Works, though there are stricter limits with custom fonts loading |
| riscv64-linux     | Debian Trixie on Docker (emulated)           |  Compiles successfully, yet to be tested on real hardware          |

### Java

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| x64-windows       | Windows 11 Home                              |  Works                                                             |

### Kotlin

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| x64-windows       | Windows 11 Home                              |  Works                                                             |

### Scala

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| x64-windows       | Windows 11 Home                              |  Works                                                             |

### F#

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| x64-windows       | Windows 11 Home                              |  Works                                                             |
| x64-linux         | Debian Trixie                                |  Works                                                             |

### C#

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| x64-windows       | Windows 11 Home                              |  Works                                                             |

### Python

| Architecture      | OS                                           |  Notes                                                             |  
| ----------------- | -------------------------------------------- |  ----------------------------------------------------------------- |
| x64-windows       | Windows 11 Home                              |  Works                                                             |

## Accessibility

Accessibility is a key priority for the future of **XFrames**. While the current version lacks comprehensive accessibility support, we are committed to making XFrames an inclusive framework that provides equitable access for all users. Upcoming development will focus on implementing accessibility features and adhering to industry standards, ensuring XFrames applications can be used effectively by people with disabilities. Our goal is to create a robust, accessible platform that enables developers to build applications for diverse audiences with confidence.

## Basic online WASM demo

([online demo](https://andreamancuso.github.io/react-wasm/dear-imgui)) Only browsers that natively support WebGPU: Chrome, Edge, Firefox nightly, possibly Safari (though I have not tested it).

## Screenshots

<video src='https://github.com/user-attachments/assets/61fbc418-a419-4bdc-8202-50ff16c5ee56' style="width:90%"></video>

![React Dear Imgui screenshot 1](https://github.com/user-attachments/assets/1512b95f-640d-4555-8a4b-57ad08119876)

![React Dear Imgui screenshot 2](https://github.com/user-attachments/assets/1a9b8ae9-d529-45af-ab7b-7e173799136f)

![React Dear Imgui screenshot 3](/screenshots/dear-imgui/screenshot-react-wasm-dear-imgui-sample-code.png?raw=true)

![React Dear Imgui screenshot 4](https://github.com/user-attachments/assets/0859f067-304b-4078-a6d3-cf17a51386f7)

## Building

### Supported platforms

| Architecture      | OS                                           | Compiler           | Notes |  
| ----------------- | -------------------------------------------- | ------------------ | ----- |
| wasm32-emscripten | N/A                                          | emsdk 3.1.64       | Works |
| x64-windows       | Windows 11 Home                              | Visual Studio 2022 | Works |
| x64-linux         | WSL2 Ubuntu 24.04.1 LTS                      | gcc 13.2.0         | Works |
| x64-linux         | Debian Trixie                                | gcc 14             | Works |
| x64-linux         | Ubuntu 22.04 LTS                             | gcc 12.2           | Works |
| x64-linux         | Ubuntu 24.04 LTS                             | gcc 13.2           | Works |
| arm64-linux       | Raspberry Pi OS (Debian bookworm) Oct 2024   | gcc 12.2           | Works |

#### General considerations for Ubuntu

This may seem obvious, particularly if you are an experienced Linux user/developer, so this is for the avoidance of the doubt: building xframes on Ubuntu 24.04 means that the generated binary extension will run on Ubuntu 24.04 but not on Ubuntu 22.04 (or older). As part of doing our tests, the binary extension generated using Ubuntu 22.04 did work fine on Ubuntu 24.04. Moving forward, we'll try to build it on even earlier versions of Ubuntu, i.e. 20.04

## Contributing

We welcome contributions! If you’re interested in helping develop xframes, please get in touch and I'll help you get started.

[![Join the Xframes Discord Community](https://img.shields.io/badge/Join%20the%20Community-Discord-7289DA)](https://discord.gg/Cbgcajdq)

## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/genautz"><img src="https://avatars.githubusercontent.com/u/89743955?v=4?s=100" width="100px;" alt="genautz"/><br /><sub><b>genautz</b></sub></a><br /><a href="https://github.com/andreamancuso/react-wasm/commits?author=genautz" title="Code">💻</a> <a href="https://github.com/andreamancuso/react-wasm/commits?author=genautz" title="Documentation">📖</a> <a href="#platform-genautz" title="Packaging/porting to new platform">📦</a> <a href="#tool-genautz" title="Tools">🔧</a> <a href="#infra-genautz" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/radlinskii"><img src="https://avatars.githubusercontent.com/u/26116041?v=4?s=100" width="100px;" alt="Radliński Ignacy"/><br /><sub><b>Radliński Ignacy</b></sub></a><br /><a href="#userTesting-radlinskii" title="User Testing">📓</a></td>
    </tr>
  </tbody>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind welcome!

### Star history

[![Star History Chart](https://api.star-history.com/svg?repos=xframes-project/xframes&type=Date)](https://star-history.com/#xframes-project/xframes&Date)

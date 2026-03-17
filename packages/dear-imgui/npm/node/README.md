# @xframes/node

DOM-free, GPU-accelerated desktop GUI development for Node.js — powered by [Dear ImGui](https://github.com/ocornut/imgui) and [Yoga Layout](https://www.yogalayout.dev/).

Write React/TypeScript components that render as native desktop widgets with zero browser overhead.

## Quick Start

```bash
npx create-xframes-node-app
cd my-app
npm start
```

## Example

```tsx
import { resolve } from "path";
import * as React from "react";
import { render, XFrames } from "@xframes/node";

const fontDefs = {
  defs: [{ name: "roboto-regular", sizes: [16, 18, 20, 24] }]
    .map(({ name, sizes }) => sizes.map((size) => ({ name, size })))
    .flat(),
};

const theme = { /* ... */ };

const App = () => (
  <XFrames.Node root style={{ height: "100%" }}>
    <XFrames.UnformattedText text="Hello, world" />
    <XFrames.Button label="Click me" onClick={() => console.log("clicked")} />
  </XFrames.Node>
);

render(App, resolve("./assets"), fontDefs, theme);
```

## Available Widgets

**Input**: Button, Checkbox, Combo, InputText, Slider, MultiSlider, ColorPicker

**Text**: BulletText, DisabledText, SeparatorText, TextWrap, UnformattedText

**Layout**: Node, Child, Group, DIWindow, Separator, TabBar, TabItem, CollapsingHeader, TreeNode, TreeView

**Data**: Table (sorting, filtering, column reorder/hide, context menus), PlotLine, PlotBar, PlotScatter, PlotHeatmap, PlotHistogram, PlotPieChart, PlotCandlestick

**Canvas**: JsCanvas (QuickJS), LuaCanvas (Lua/Sol2), JanetCanvas (Janet) — scripted 2D rendering with Canvas 2D API shim

**Other**: Image, ProgressBar, ColorIndicator, ClippedMultiLineTextRenderer, ItemTooltip

## Key Features

- **React components** — familiar JSX syntax, hooks, refs, state management
- **Yoga flexbox layout** — the same layout engine used by React Native
- **Per-state styling** — base, hover, active, and disabled style variants
- **Imperative handles** — refs for Table, Plot widgets, InputText, and more
- **Font Awesome icons** — built-in icon support in tables and text
- **Scripted canvas rendering** — embed JavaScript, Lua, or Janet scripts for custom 2D drawing with an HTML5 Canvas 2D-style API
- **Interactive maps** — MapView widget with OpenStreetMap tiles, markers, polylines, and overlays
- **Theme system** — runtime theme switching via `patchStyle`
- **Prebuilt binaries** — `npm install` downloads platform-specific native addons (NAPI v9)

## Platform Support

| Architecture | OS | Notes |
|---|---|---|
| x64-windows | Windows 11 | Works |
| x64-linux | WSL2 Ubuntu 24.04 | Set `export GALLIUM_DRIVER=d3d12` |
| x64-linux | Debian Trixie | Works |
| x64-linux | Ubuntu 22.04 / 24.04 | Works |
| arm64-linux | Raspberry Pi OS (Bookworm) | Works |

## Building from Source

If a prebuilt binary isn't available for your platform, the native addon compiles from source during `npm install`. Requirements:

**Windows**: Visual Studio 2022

**Ubuntu 24.04**:
```bash
sudo apt install curl zip unzip tar build-essential cmake libglfw3 libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config
```

**Fedora 41**:
```bash
sudo dnf install @development-tools gcc-c++ cmake glfw-devel
```

**Raspberry Pi OS**:
```bash
sudo apt install curl zip unzip tar build-essential cmake libglfw3 libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config
export ARM64_LINUX=1
export VCPKG_FORCE_SYSTEM_BINARIES=1
```

## Links

- [Documentation](https://xframes.dev)
- [GitHub](https://github.com/xframes-project/xframes)
- [Discord](https://discord.gg/Cbgcajdq)

## License

MIT

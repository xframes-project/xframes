# @xframes/wasm

DOM-free, GPU-accelerated GUI development for the browser — powered by [Dear ImGui](https://github.com/ocornut/imgui), [Yoga Layout](https://www.yogalayout.dev/), and WebGPU.

Write React/TypeScript components that render as native widgets in the browser with zero DOM overhead.

Maintainers: see [React Native Fabric embedding](../FABRIC_EMBEDDING.md) for the React Native 0.87 integration, Docker build, and browser smoke procedure.

## Documentation and examples

See [xframes.dev](https://xframes.dev) (requires a browser with WebGPU support for live Wasm examples).

## Installation

```bash
npm install @xframes/wasm @xframes/common react
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
- **Scripted canvas rendering** — embed JavaScript, Lua, or Janet scripts for custom 2D drawing with an HTML5 Canvas 2D-style API
- **Interactive maps** — MapView widget with OpenStreetMap tiles, markers, polylines, and overlays
- **Font Awesome icons** — built-in icon support in tables and text
- **Theme system** — runtime theme switching via `patchStyle`

## Browser Support

Requires native WebGPU support:

| Browser         | Status    |
| --------------- | --------- |
| Chrome          | Supported |
| Edge            | Supported |
| Firefox Nightly | Supported |
| Safari          | Untested  |

The repository smoke test currently covers a Docker-built Wasm module in headless Chromium. Other browser rows describe intended runtime support and still require release-platform verification.

## Links

- [Documentation](https://xframes.dev)
- [GitHub](https://github.com/xframes-project/xframes)
- [Discord](https://discord.gg/Cbgcajdq)

## License

MIT

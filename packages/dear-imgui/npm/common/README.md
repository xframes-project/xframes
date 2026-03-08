# @xframes/common

Shared React components, types, and utilities for [XFrames](https://github.com/xframes-project/xframes) — a DOM-free GUI framework powered by Dear ImGui.

This package is a peer dependency of [`@xframes/node`](https://www.npmjs.com/package/@xframes/node) (desktop) and [`@xframes/wasm`](https://www.npmjs.com/package/@xframes/wasm) (browser).

## Quick Start

The easiest way to get started is with the scaffolding tool:

```bash
npx create-xframes-node-app
cd my-app
npm start
```

## Installation

```bash
npm install @xframes/common
```

## Components

**Input**
Button, Checkbox, Combo, InputText, Slider, MultiSlider, ColorPicker

**Text**
BulletText, DisabledText, SeparatorText, TextWrap, UnformattedText

**Layout**
Node, Child, Group, DIWindow, Separator, ItemTooltip, TabBar, TabItem, CollapsingHeader, TreeNode, TreeView

**Data Visualization**
Table, PlotLine, PlotBar, PlotScatter, PlotHeatmap, PlotHistogram, PlotPieChart, PlotCandlestick

**Other**
Image, MapView, ProgressBar, ColorIndicator, ClippedMultiLineTextRenderer

## Also Included

- **Style system** — `RWStyleSheet`, `XFramesStyle`, Yoga layout props, per-state styling (base/hover/active/disabled)
- **Imperative handles** — refs for Table, Plot widgets, InputText, Image, and more
- **WidgetRegistrationService** — manages widget lifecycle and data operations
- **Hooks** — `useWidgetRegistrationService`, `useXFramesFonts`, `useWidgetEventManagement`
- **Font Awesome icons** — `faIconMap` with icon name lookups

## Links

- [GitHub](https://github.com/xframes-project/xframes)
- [Documentation](https://xframes.dev)

## License

MIT

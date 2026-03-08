# Canvas Widget Design

## Purpose

A generic custom drawing surface that exposes ImGui's ImDrawList primitives to JavaScript. Unlocks visualizations that can't be built from existing widgets — satellite sky views, compass gauges, speedometers, altimeters, and any custom rendering.

## Architecture

### Data flow

```
JS (React component)
  → builds array of draw commands
  → JSON.stringify
  → elementInternalOp(id, { op: "setDrawCommands", commands: [...] })
  → NAPI / WASM bridge
  → C++ HandleInternalOp: parse JSON once, store as pre-parsed native structs
  → Render() each frame: loop through native structs, call ImDrawList methods
```

### Why not Lua?

We considered embedding a Lua interpreter so rendering logic could run directly in C++ without JSON serialization overhead. The problem: data originates in JS (e.g. satellite positions from ubx-parser callbacks). The flow would be JS → JSON → C++ → Lua tables → ImDrawList — three serialization steps to avoid one. The data has to cross the JS→C++ bridge regardless, so Lua adds complexity without shortcutting the real bottleneck.

Keeping the rendering logic in TypeScript (where the data already lives) is simpler. JS does the math (polar projection, color mapping), builds the draw commands, and sends one JSON array across the bridge.

### Pre-parsed structs, not raw JSON

The key performance decision: **parse JSON once in `HandleInternalOp`, store as native C++ structs**. The `Render()` method never touches JSON — it loops through pre-parsed structs with zero parsing overhead.

```cpp
struct DrawCmd {
    enum Type { Line, Circle, CircleFilled, Rect, Text, ... };
    Type type;
    ImVec2 p1, p2, p3, p4;
    float radius, thickness;
    ImU32 color;        // already converted from CSS string
    std::string text;   // only for text commands
    int segments;
};
std::vector<DrawCmd> m_drawCommands;
```

This means `Render()` at 60fps is just a fast struct loop with direct ImDrawList calls — no string comparisons, no color parsing, no JSON field lookups.

### Draw order

ImDrawList draws in order — later commands render on top. The JS side controls ordering by building the array in the right sequence (background first, foreground last). No layer system needed.

For mixed static/dynamic content (e.g. a sky view where the grid is static but satellites move at 20Hz), the JS side caches static commands and spreads them into the array:

```tsx
const staticGrid = useMemo(() => [...gridCommands], []);

// Called at 20Hz with new satellite data
canvasRef.current?.setDrawCommands([
  ...staticGrid,       // background (cached, not recomputed)
  ...satelliteMarkers, // dynamic (recalculated from new positions)
  ...staticOverlay,    // foreground (cached)
]);
```

We considered a two-layer system (static + dynamic) on the C++ side, but it breaks down when you need more than two layers or interleaved ordering. Keeping it as a single flat list with JS controlling the order is simpler, correct, and the serialization cost of repeating static commands is negligible.

### Performance at 20Hz update rate

For a receiver emitting at 20Hz: ~200 draw commands × ~100 bytes each = ~20KB of JSON per update. At 20Hz = 400KB/s crossing the bridge. This is well within what the existing pipeline handles (Table streams far more data). With static command caching on the JS side, the dynamic portion drops to ~40 commands (~4KB) at 20Hz = 80KB/s.

## Coordinate system

Canvas-local: (0,0) is top-left of the widget. Width and height come from Yoga layout. C++ translates to screen-space by adding `ImGui::GetCursorScreenPos()`.

## Color format

CSS color strings — same format used everywhere else in XFrames. Parsed by `extractColor()` from `color_helpers.h` during the pre-parse step (once, not per frame).

## Imperative handle

```ts
export type CanvasImperativeHandle = {
    setDrawCommands: (commands: DrawCommand[]) => void;
    clear: () => void;
};
```

## Supported draw commands

| Type | Fields | ImDrawList method |
|------|--------|-------------------|
| `line` | `x1, y1, x2, y2, color, thickness?` | `AddLine` |
| `rect` | `x, y, w, h, color, thickness?, rounding?` | `AddRect` |
| `rectFilled` | `x, y, w, h, color, rounding?` | `AddRectFilled` |
| `circle` | `cx, cy, radius, color, thickness?, segments?` | `AddCircle` |
| `circleFilled` | `cx, cy, radius, color, segments?` | `AddCircleFilled` |
| `triangle` | `x1, y1, x2, y2, x3, y3, color, thickness?` | `AddTriangle` |
| `triangleFilled` | `x1, y1, x2, y2, x3, y3, color` | `AddTriangleFilled` |
| `text` | `x, y, color, text` | `AddText` |
| `polyline` | `points: [{x,y},...], color, thickness?, closed?` | `AddPolyline` |
| `convexPolyFilled` | `points: [{x,y},...], color` | `AddConvexPolyFilled` |
| `bezierCubic` | `x1,y1, x2,y2, x3,y3, x4,y4, color, thickness?` | `AddBezierCubic` |
| `arc` | `cx, cy, radius, aMin, aMax, color, thickness?, segments?` | PathArcTo + PathStroke |
| `arcFilled` | `cx, cy, radius, aMin, aMax, color, segments?` | PathArcTo + PathFillConvex |
| `ellipse` | `cx, cy, rx, ry, color, thickness?, rotation?` | `AddEllipse` |
| `ellipseFilled` | `cx, cy, rx, ry, color, rotation?` | `AddEllipseFilled` |
| `ngon` | `cx, cy, radius, color, numSegments, thickness?` | `AddNgon` |
| `ngonFilled` | `cx, cy, radius, color, numSegments` | `AddNgonFilled` |

## Existing patterns to follow

- **ColorIndicator** (`color_indicator.cpp`) — simplest ImDrawList usage: `GetWindowDrawList()`, `AddCircleFilled`/`AddRectFilled`, `Dummy()` to advance cursor
- **PlotCandlestick** (`plot_candlestick.cpp`) — loops through data arrays calling ImDrawList methods, `HandleInternalOp` for batch data updates
- **Table** (`table.cpp`) — `HandleInternalOp` with `setData`/`appendData` pattern for batch JSON command processing

## What this unlocks

- Satellite sky view (polar grid + colored constellation markers)
- Compass / heading gauge
- Speedometer / altimeter dials
- Any custom visualization not covered by ImPlot

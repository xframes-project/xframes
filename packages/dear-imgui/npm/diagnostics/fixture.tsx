import React from "react";
import { components, type PlotBarImperativeHandle, type TableImperativeHandle } from "@xframes/common";

export const columns = [
    { fieldId: "sequence", heading: "Sequence", type: "number" as const },
    { fieldId: "signal", heading: "Signal", type: "number" as const },
    { fieldId: "used", heading: "Used", type: "boolean" as const },
];
export const makeRows = (count: number, sequence = 0) => Array.from({ length: count }, (_, index) => ({
    sequence: sequence + index, signal: 20 + index % 40, used: index % 2 === 0,
}));
export const makeHandles = () => ({ plot: React.createRef<PlotBarImperativeHandle>(), table: React.createRef<TableImperativeHandle>() });

export function Fixture({ handles, reversed = false, visible = true, points = 128 }: {
    handles: ReturnType<typeof makeHandles>; reversed?: boolean; visible?: boolean; points?: number;
}) {
    const stations = reversed ? ["B", "A"] : ["A", "B"];
    return React.createElement("node", { root: true, id: "fixture-root", style: { width: "100%", height: "100%", padding: { all: 12 } } },
        React.createElement("separator-text", { label: "XFrames Fabric lifecycle / streaming baseline" }),
        visible && React.createElement("node", { id: "content", style: { width: "100%", height: 630 } },
            React.createElement("node", { id: "stations", style: { flexDirection: "row", height: 40 } },
                stations.map(station => React.createElement("di-button", { key: station, id: `station-${station}`, label: `Station ${station}`, style: { width: 130, height: 30 } }))),
            React.createElement(components.PlotBar, { ref: handles.plot, dataPointsLimit: points, axisAutoFit: true,
                showLegend: true, series: [{ label: "Signal A" }, { label: "Signal B" }],
                style: { width: 820, height: 240 } }),
            React.createElement(components.Table, { ref: handles.table, columns, clipRows: 10, filterable: true,
                style: { width: 820, height: 320 } })));
}

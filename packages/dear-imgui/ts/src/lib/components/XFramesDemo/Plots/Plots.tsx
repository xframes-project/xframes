import React, { useCallback, useRef, useState } from "react";
import { PlotLineImperativeHandle } from "../../XFrames/PlotLine";
import { XFrames } from "../../XFrames";

export const Plots = () => {
    const plotRef = useRef<PlotLineImperativeHandle>(null);
    const [axisAutoFit, setAxisAutoFit] = useState(true);
    const counterRef = useRef(0);

    const appendData = useCallback(() => {
        if (plotRef.current) {
            counterRef.current += 1;
            plotRef.current.appendData(counterRef.current, counterRef.current);
        }
    }, []);

    const resetData = useCallback(() => {
        if (plotRef.current) {
            plotRef.current.resetData();
        }
    }, []);

    const toggleAxisAutoFit = useCallback(() => {
        setAxisAutoFit((val) => !val);
    }, []);

    return (
        <XFrames.Node
            style={{
                width: "100%",
                height: "100%",
                flexDirection: "column",
                gap: { row: 5 },
            }}
        >
            <XFrames.Node
                style={{
                    width: "100%",
                    flexDirection: "row",
                    alignItems: "center",
                    gap: { column: 15 },
                }}
            >
                <XFrames.Button onClick={appendData} label="Append" />
                <XFrames.Button onClick={toggleAxisAutoFit} label="Toggle Axis Auto-Fit" />
                <XFrames.Button onClick={resetData} label="Reset" />
            </XFrames.Node>

            <XFrames.PlotLine ref={plotRef} style={{ flex: 1 }} axisAutoFit={axisAutoFit} />
        </XFrames.Node>
    );
};

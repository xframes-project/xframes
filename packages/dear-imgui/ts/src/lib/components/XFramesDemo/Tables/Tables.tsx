import React, { useCallback, useMemo, useRef } from "react";
import { TableImperativeHandle } from "../../XFrames/Table";
import RWStyleSheet from "src/lib/stylesheet/stylesheet";
import { ImGuiCol } from "src/lib/wasm/wasm-app-types";
import { XFrames } from "../../XFrames";

const sampleData = [
    { id: "1", name: "Name" },
    { id: "1", name: "Name" },
    { id: "1", name: "Name" },
    { id: "1", name: "Name" },
    { id: "1", name: "Name" },
];

const styleSheet = RWStyleSheet.create({
    primaryButton: {
        colors: { [ImGuiCol.Button]: "#ff6e59" },
        // vars: { [ImGuiStyleVar.FramePadding]: [40, 3] },
        // width: 150,
    },
    secondaryButton: {
        // width: 150,
    },
});

export const Tables = () => {
    const intervalRef = useRef<NodeJS.Timeout>();
    const counterRef = useRef<number>(1);
    const tableRef1 = useRef<TableImperativeHandle>(null);
    const tableRef2 = useRef<TableImperativeHandle>(null);
    const tableRef3 = useRef<TableImperativeHandle>(null);
    const tableRef4 = useRef<TableImperativeHandle>(null);

    const tableColumns = useMemo(
        () => [
            {
                heading: "ID",
                fieldId: "id",
            },
            {
                heading: "Name",
                fieldId: "name",
            },
        ],
        [],
    );

    const handleAppendDataToTableClick = useCallback(() => {
        if (tableRef1.current && tableRef2.current && tableRef3.current && tableRef4.current) {
            intervalRef.current = setInterval(() => {
                if (
                    tableRef1.current &&
                    tableRef2.current &&
                    tableRef3.current &&
                    tableRef4.current
                ) {
                    const data = sampleData.map((sampleDataRow, index) => ({
                        ...sampleDataRow,
                        id: `${counterRef.current + index}`,
                    }));
                    counterRef.current += sampleData.length;

                    // console.log(counterRef.current);

                    tableRef1.current.appendDataToTable(data);
                    tableRef2.current.appendDataToTable(data);
                    tableRef3.current.appendDataToTable(data);
                    tableRef4.current.appendDataToTable(data);
                }
            }, 10);
        }
    }, [tableRef1, tableRef2, tableRef3]);

    const handleStopAppendingDataToTableClick = useCallback(() => {
        if (intervalRef.current) {
            clearInterval(intervalRef.current);
        }
    }, []);

    // use case: align both buttons at the right hand side of the container

    // TODO: work out `root` automatically as having multiple root nodes can lead to major layout issues - not always immediately obvious...

    return (
        <XFrames.Node
            root
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
                    height: 200,
                    flexDirection: "row",
                    gap: { column: 5 },
                }}
            >
                <XFrames.Table
                    ref={tableRef1}
                    columns={tableColumns}
                    clipRows={10}
                    style={{
                        flex: 1,
                        height: "100%",
                    }}
                />
                <XFrames.Table
                    ref={tableRef2}
                    columns={tableColumns}
                    clipRows={10}
                    style={{
                        flex: 1,
                        height: "100%",
                    }}
                />
                <XFrames.Table
                    ref={tableRef3}
                    columns={tableColumns}
                    clipRows={10}
                    style={{
                        flex: 1,
                        height: "100%",
                    }}
                />
                <XFrames.Table
                    ref={tableRef4}
                    columns={tableColumns}
                    clipRows={10}
                    style={{
                        flex: 1,
                        height: "100%",
                    }}
                />
            </XFrames.Node>
            <XFrames.Node
                style={{
                    width: "100%",
                    height: 30,
                    flexDirection: "row",
                    justifyContent: "flex-end",
                    gap: { column: 5 },
                }}
            >
                <XFrames.Button
                    onClick={handleAppendDataToTableClick}
                    label="Add data to table"
                    style={styleSheet.primaryButton}
                />
                <XFrames.Button
                    onClick={handleStopAppendingDataToTableClick}
                    label="Stop adding data"
                    style={styleSheet.secondaryButton}
                />
            </XFrames.Node>
        </XFrames.Node>
    );
};

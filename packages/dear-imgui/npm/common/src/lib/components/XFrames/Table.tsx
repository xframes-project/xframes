import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type TableImperativeHandle = {
    setTableData: (data: any[]) => void;
    appendDataToTable: (data: any[]) => void;
};

export const Table = forwardRef<TableImperativeHandle, WidgetPropsMap["Table"]>(
    (
        {
            columns,
            clipRows,
            initialData,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["Table"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useRef(widgetRegistratonService.generateId());

        useEffect(() => {
            widgetRegistratonService.registerTable(idRef.current);
        }, [widgetRegistratonService]);

        useImperativeHandle(
            ref,
            () => {
                return {
                    setTableData(data: any[]) {
                        widgetRegistratonService.setTableData(idRef.current, data);
                    },
                    appendDataToTable(data: any[]) {
                        widgetRegistratonService.appendDataToTable(idRef.current, data);
                    },
                };
            },
            [],
        );

        return (
            <di-table
                id={idRef.current}
                columns={columns}
                clipRows={clipRows}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);

import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type TableImperativeHandle = {
    setTableData: (data: any[]) => void;
    appendDataToTable: (data: any[]) => void;
    setColumnFilter: (columnIndex: number, filterText: string) => void;
    clearFilters: () => void;
};

export const Table = forwardRef<TableImperativeHandle, WidgetPropsMap["Table"]>(
    (
        {
            columns,
            clipRows,
            initialData,
            filterable,
            onSort,
            onFilter,
            onRowClick,
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
                    setColumnFilter(columnIndex: number, filterText: string) {
                        widgetRegistratonService.setColumnFilter(idRef.current, columnIndex, filterText);
                    },
                    clearFilters() {
                        widgetRegistratonService.clearTableFilters(idRef.current);
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
                filterable={filterable}
                onSort={onSort}
                onFilter={onFilter}
                onRowClick={onRowClick}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);

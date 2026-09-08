import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
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
            reorderable,
            hideable,
            onSort,
            onFilter,
            onRowClick,
            contextMenuItems,
            onItemAction,
            style,
            hoverStyle,
            activeStyle,
            disabledStyle,
        }: WidgetPropsMap["Table"],
        ref,
    ) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "table");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    setTableData(data: any[]) {
                        widgetRegistratonService.setTableData(target, data);
                    },
                    appendDataToTable(data: any[]) {
                        widgetRegistratonService.appendDataToTable(target, data);
                    },
                    setColumnFilter(columnIndex: number, filterText: string) {
                        widgetRegistratonService.setColumnFilter(target, columnIndex, filterText);
                    },
                    clearFilters() {
                        widgetRegistratonService.clearTableFilters(target);
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
                reorderable={reorderable}
                hideable={hideable}
                onSort={onSort}
                onFilter={onFilter}
                onRowClick={onRowClick}
                contextMenuItems={contextMenuItems}
                onItemAction={onItemAction}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
            />
        );
    },
);

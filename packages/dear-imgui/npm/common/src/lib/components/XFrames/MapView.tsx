import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

export type MapImperativeHandle = {
    render: (centerX: number, centerY: number, zoom: number) => void;
    prefetchTiles: (minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) => void;
};

export const MapView = forwardRef<MapImperativeHandle, WidgetPropsMap["MapView"]>(
    ({ style, hoverStyle, activeStyle, disabledStyle, onChange, onPrefetchProgress,
       tileUrlTemplate, tileRequestHeaders, attribution, minZoom, maxZoom, cachePath }: WidgetPropsMap["MapView"], ref) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useRef(widgetRegistratonService.generateId());

        useEffect(() => {
            widgetRegistratonService.registerMap(idRef.current);
        }, [widgetRegistratonService]);

        useImperativeHandle(
            ref,
            () => {
                return {
                    render(centerX: number, centerY: number, zoom: number) {
                        widgetRegistratonService.renderMap(idRef.current, centerX, centerY, zoom);
                    },
                    prefetchTiles(minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) {
                        widgetRegistratonService.prefetchMapTiles(idRef.current, minLon, minLat, maxLon, maxLat, minZoom, maxZoom);
                    },
                };
            },
            [],
        );

        return (
            <map-view
                id={idRef.current}
                style={style}
                hoverStyle={hoverStyle}
                activeStyle={activeStyle}
                disabledStyle={disabledStyle}
                onChange={onChange}
                onPrefetchProgress={onPrefetchProgress}
                tileUrlTemplate={tileUrlTemplate}
                tileRequestHeaders={tileRequestHeaders}
                attribution={attribution}
                minZoom={minZoom}
                maxZoom={maxZoom}
                cachePath={cachePath}
            />
        );
    },
);

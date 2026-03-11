import { forwardRef, useEffect, useImperativeHandle, useRef } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistrationService } from "src/lib/hooks/useWidgetRegistrationService";

/**
 * WARNING: prefetchTiles() bulk-downloads map tiles for offline use.
 * This violates the tile usage policy of OpenStreetMap's default tile servers
 * (tile.openstreetmap.org). Only use prefetchTiles() with a tile server that
 * explicitly permits bulk downloading (e.g. a self-hosted server or a
 * commercial provider whose terms allow it). Set `tileUrlTemplate` accordingly.
 * See: https://operations.osmfoundation.org/policies/tiles/
 */
export type MapMarker = {
    lat: number;
    lon: number;
    color?: string;
    label?: string;
    radius?: number;
};

export type MapImperativeHandle = {
    render: (centerX: number, centerY: number, zoom: number) => void;
    prefetchTiles: (minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) => void;
    setMarkers: (markers: MapMarker[]) => void;
    clearMarkers: () => void;
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
                    setMarkers(markers: MapMarker[]) {
                        widgetRegistratonService.setMapMarkers(idRef.current, markers);
                    },
                    clearMarkers() {
                        widgetRegistratonService.clearMapMarkers(idRef.current);
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

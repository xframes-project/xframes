import { forwardRef, useImperativeHandle } from "react";
import { WidgetPropsMap } from "./types";
import { useWidgetRegistration } from "src/lib/hooks/useWidgetRegistration";
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

export type MapPolyline = {
    points: { lat: number; lon: number }[];
    color?: string;
    thickness?: number;
    pointsLimit?: number;
};

export type MapOverlay = {
    lat: number;
    lon: number;
    radiusMeters: number;
    radiusMinorMeters?: number;
    rotation?: number;
    fillColor?: string;
    strokeColor?: string;
    strokeThickness?: number;
};

export type MapImperativeHandle = {
    render: (centerX: number, centerY: number, zoom: number) => void;
    prefetchTiles: (minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) => void;
    setMarkers: (markers: MapMarker[]) => void;
    clearMarkers: () => void;
    setPolylines: (polylines: MapPolyline[]) => void;
    clearPolylines: () => void;
    appendPolylinePoint: (polylineIndex: number, lat: number, lon: number) => void;
    setOverlays: (overlays: MapOverlay[]) => void;
    clearOverlays: () => void;
};

export const MapView = forwardRef<MapImperativeHandle, WidgetPropsMap["MapView"]>(
    ({ style, hoverStyle, activeStyle, disabledStyle, onChange, onPrefetchProgress,
       tileUrlTemplate, tileRequestHeaders, attribution, minZoom, maxZoom, cachePath }: WidgetPropsMap["MapView"], ref) => {
        const widgetRegistratonService = useWidgetRegistrationService();
        const idRef = useWidgetRegistration(widgetRegistratonService, "map");

        useImperativeHandle(
            ref,
            () => {
                const target = widgetRegistratonService.captureWidget(idRef.current);
                return {
                    render(centerX: number, centerY: number, zoom: number) {
                        widgetRegistratonService.renderMap(target, centerX, centerY, zoom);
                    },
                    prefetchTiles(minLon: number, minLat: number, maxLon: number, maxLat: number, minZoom: number, maxZoom: number) {
                        widgetRegistratonService.prefetchMapTiles(target, minLon, minLat, maxLon, maxLat, minZoom, maxZoom);
                    },
                    setMarkers(markers: MapMarker[]) {
                        widgetRegistratonService.setMapMarkers(target, markers);
                    },
                    clearMarkers() {
                        widgetRegistratonService.clearMapMarkers(target);
                    },
                    setPolylines(polylines: MapPolyline[]) {
                        widgetRegistratonService.setMapPolylines(target, polylines);
                    },
                    clearPolylines() {
                        widgetRegistratonService.clearMapPolylines(target);
                    },
                    appendPolylinePoint(polylineIndex: number, lat: number, lon: number) {
                        widgetRegistratonService.appendMapPolylinePoint(target, polylineIndex, lat, lon);
                    },
                    setOverlays(overlays: MapOverlay[]) {
                        widgetRegistratonService.setMapOverlays(target, overlays);
                    },
                    clearOverlays() {
                        widgetRegistratonService.clearMapOverlays(target);
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

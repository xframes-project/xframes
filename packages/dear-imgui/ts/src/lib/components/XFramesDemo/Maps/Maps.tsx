import React, { useCallback, useRef, useState } from "react";
import { MapImperativeHandle } from "../../XFrames/MapView";
import { XFrames } from "../../XFrames";

export const Maps = () => {
    const mapRef = useRef<MapImperativeHandle>(null);
    const [center, setCenter] = useState<[number, number]>([-74.006, 40.7128]);
    const [zoom, setZoom] = useState(15);

    const renderMap = useCallback(() => {
        if (mapRef.current) {
            mapRef.current.render(center[0], center[1], zoom);
        }
    }, [center, zoom]);

    const locate = useCallback(() => {
        if ("geolocation" in navigator) {
            navigator.geolocation.getCurrentPosition(
                (position) => {
                    setCenter([position.coords.longitude, position.coords.latitude]);
                },
                (error) => {
                    console.error("Error getting geolocation:", error);
                },
            );
        }
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
                <XFrames.UnformattedText text={`Center: ${center[0]}, ${center[1]}`} />
                <XFrames.Button onClick={locate} label="Locate" />
                <XFrames.Button onClick={renderMap} label="Render" />
            </XFrames.Node>

            <XFrames.MapView ref={mapRef} style={{ flex: 1 }} />
        </XFrames.Node>
    );
};

import React, { useMemo } from "react";
import faIconMap, { faIconKeys } from "src/lib/fa-icons";
import RWStyleSheet from "src/lib/stylesheet/stylesheet";
import { XFrames } from "../../XFrames";

export const Icons = () => {
    const styleSheet = useMemo(
        () =>
            RWStyleSheet.create({
                mainWrapperNode: {
                    width: "100%",
                    height: "auto",
                    flexDirection: "row",
                    flexWrap: "wrap",
                    gap: { row: 5, column: 5 },
                },
                iconWrapperNode: {
                    minWidth: 200,
                    maxWidth: 240,
                    flex: 1,
                    height: 100,
                    border: {
                        color: "#000",
                        thickness: 1,
                    },
                    alignItems: "center",
                },
                icon: {
                    font: { name: "roboto-regular", size: 48 },
                },
                iconKey: {
                    font: { name: "roboto-mono-regular", size: 16 },
                },
            }),
        [],
    );

    return (
        <XFrames.Node style={styleSheet.mainWrapperNode}>
            {Object.keys(faIconMap)
                // .slice(0, 60)
                .map((key) => (
                    <XFrames.Node key={key} style={styleSheet.iconWrapperNode}>
                        <XFrames.UnformattedText
                            text={faIconMap[key as faIconKeys]}
                            style={styleSheet.icon}
                        />
                        <XFrames.UnformattedText text={key} style={styleSheet.iconKey} />
                    </XFrames.Node>
                ))}
        </XFrames.Node>
    );
};

import React, { useCallback, useMemo, useState } from "react";
import RWStyleSheet from "src/lib/stylesheet/stylesheet";
import { MultiSliderChangeEvent, SliderChangeEvent } from "../../XFrames/types";
import { XFrames } from "../../XFrames";

export const Sliders = () => {
    const [singleSliderValue, setSingleSliderValue] = useState(0);
    const [angleSliderValue, setAngleSliderValue] = useState(0);
    const [doubleSliderValue, setDoubleSliderValue] = useState<[number, number]>([0, 0]);
    const [tripleSliderValue, setTripleSliderValue] = useState<[number, number, number]>([0, 0, 0]);
    const [quadSliderValue, setQuadSliderValue] = useState<[number, number, number, number]>([
        0, 0, 0, 0,
    ]);

    const handleSingleSliderValueChanged = useCallback((event: SliderChangeEvent) => {
        if (event.nativeEvent) {
            setSingleSliderValue(event.nativeEvent.value);
        }
    }, []);

    const handleAngleSliderValueChanged = useCallback((event: SliderChangeEvent) => {
        if (event.nativeEvent) {
            setAngleSliderValue(event.nativeEvent.value);
        }
    }, []);

    const handleDoubleSliderValueChanged = useCallback((event: MultiSliderChangeEvent<number>) => {
        if (event.nativeEvent) {
            setDoubleSliderValue([event.nativeEvent.values[0], event.nativeEvent.values[1]]);
        }
    }, []);

    const handleTripleSliderValueChanged = useCallback((event: MultiSliderChangeEvent<number>) => {
        if (event.nativeEvent) {
            setTripleSliderValue([
                event.nativeEvent.values[0],
                event.nativeEvent.values[1],
                event.nativeEvent.values[2],
            ]);
        }
    }, []);

    const handleQuadSliderValueChanged = useCallback((event: MultiSliderChangeEvent<number>) => {
        if (event.nativeEvent) {
            setQuadSliderValue([
                event.nativeEvent.values[0],
                event.nativeEvent.values[1],
                event.nativeEvent.values[2],
                event.nativeEvent.values[3],
            ]);
        }
    }, []);

    const styleSheet = useMemo(
        () =>
            RWStyleSheet.create({
                wrapper: { flex: 1, gap: { row: 5 } },
                row: { alignItems: "center", flexDirection: "row", gap: { column: 5 } },
                label: {
                    width: 150,
                },
                slider: {
                    width: 400,
                },
                valueLabel: {
                    width: 50,
                },
            }),
        [],
    );

    return (
        <XFrames.Node style={styleSheet.wrapper}>
            <XFrames.Node style={styleSheet.row}>
                <XFrames.UnformattedText text="Basic Slider" style={styleSheet.label} />
                <XFrames.Slider
                    defaultValue={0}
                    min={0}
                    max={10}
                    style={styleSheet.slider}
                    onChange={handleSingleSliderValueChanged}
                />
                <XFrames.UnformattedText
                    text={`Selected: ${singleSliderValue}`}
                    style={styleSheet.valueLabel}
                />
            </XFrames.Node>
            <XFrames.Node style={styleSheet.row}>
                <XFrames.UnformattedText text="Angle Slider (radians)" style={styleSheet.label} />
                <XFrames.Slider
                    defaultValue={0}
                    min={0}
                    max={10}
                    sliderType="angle"
                    onChange={handleAngleSliderValueChanged}
                    style={styleSheet.slider}
                />
                <XFrames.UnformattedText
                    text={`Selected: ${angleSliderValue}`}
                    style={styleSheet.valueLabel}
                />
            </XFrames.Node>
            <XFrames.Node style={styleSheet.row}>
                <XFrames.UnformattedText text="Multi Slider, 2 values" style={styleSheet.label} />
                <XFrames.MultiSlider
                    defaultValues={[0, 0]}
                    min={0}
                    max={10}
                    numValues={2}
                    onChange={handleDoubleSliderValueChanged}
                    style={styleSheet.slider}
                />
                <XFrames.UnformattedText
                    text={`Selected: ${doubleSliderValue[0]}, ${doubleSliderValue[1]}`}
                    style={styleSheet.valueLabel}
                />
            </XFrames.Node>
            <XFrames.Node style={styleSheet.row}>
                <XFrames.UnformattedText text="Multi Slider, 3 values" style={styleSheet.label} />
                <XFrames.MultiSlider
                    defaultValues={[0, 0, 0]}
                    min={0}
                    max={10}
                    numValues={3}
                    onChange={handleTripleSliderValueChanged}
                    style={styleSheet.slider}
                />
                <XFrames.UnformattedText
                    text={`Selected: ${tripleSliderValue[0]}, ${tripleSliderValue[1]}, ${tripleSliderValue[2]}`}
                    style={styleSheet.valueLabel}
                />
            </XFrames.Node>
            <XFrames.Node style={styleSheet.row}>
                <XFrames.UnformattedText text="Multi Slider, 4 values" style={styleSheet.label} />
                <XFrames.MultiSlider
                    defaultValues={[0, 0, 0, 0]}
                    min={0}
                    max={10}
                    numValues={4}
                    onChange={handleQuadSliderValueChanged}
                    style={styleSheet.slider}
                />
                <XFrames.UnformattedText
                    text={`Selected: ${quadSliderValue[0]}, ${quadSliderValue[1]}, ${quadSliderValue[2]}, ${quadSliderValue[3]}`}
                    style={styleSheet.valueLabel}
                />
            </XFrames.Node>
        </XFrames.Node>
    );
};

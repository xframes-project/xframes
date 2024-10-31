import * as React from "react";
import { useEffect, useMemo, useState, useRef, PropsWithChildren } from "react";
import { v4 as uuidv4 } from "uuid";
import debounce from "lodash.debounce";
// @ts-ignore
import ReactNativePrivateInterface from "../react-native/ReactNativePrivateInterface";
import { GetWasmModule, MainModule, WasmExitStatus } from "../wasm/wasm-app-types";
import { ReactNativeWrapper } from "../components/ReactNativeWrapper";
import { FontDef } from "./XFrames/types";
import { attachSubComponents } from "../attachSubComponents";
import { components } from "./XFrames/components";
import { XFramesStyleForPatching } from "../stylesheet/xframes-style";
import { useXFramesFonts, useXFramesWasm } from "../hooks";

export type MainComponentProps = PropsWithChildren & {
    containerRef?: React.RefObject<HTMLElement>;
    getWasmModule: GetWasmModule;
    wasmDataPackage: string;
    fontDefs?: FontDef[];
    defaultFont?: { name: string; size: number };
    styleOverrides?: XFramesStyleForPatching;
};

export const MainComponent: React.ComponentType<MainComponentProps> = ({
    containerRef,
    children,
    getWasmModule,
    wasmDataPackage,
    fontDefs,
    defaultFont,
    styleOverrides,
}: MainComponentProps) => {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    const isWasmModuleLoading = useRef(false);
    const [wasmModule, setWasmModule] = useState<MainModule | undefined>();

    const canvasId = useMemo(() => `canvas-${uuidv4()}`, []);

    const { eventHandlers } = useXFramesWasm(ReactNativePrivateInterface);
    const fonts = useXFramesFonts(fontDefs);

    useEffect(() => {
        if (canvasRef.current) {
            if (!isWasmModuleLoading.current) {
                isWasmModuleLoading.current = true;

                let localModule: MainModule;

                const load = async () => {
                    const args: any[] = [
                        `#${canvasId}`,
                        JSON.stringify({ defs: fonts, defaultFont }),
                    ];

                    if (styleOverrides) {
                        args.push(JSON.stringify(styleOverrides));
                    }

                    console.time("init");
                    eventHandlers.onInit = () => {
                        console.timeEnd("init");
                        console.log("ready");
                        setWasmModule(localModule);
                    };

                    const moduleArg: any = {
                        canvas: canvasRef.current,
                        arguments: args,
                        locateFile: (_path: string) => {
                            return wasmDataPackage;
                        },
                        eventHandlers,
                    };

                    try {
                        localModule = await getWasmModule(moduleArg);
                    } catch (exception) {
                        console.log("Unable to initialize the WASM correctly", exception);
                    }
                };

                load();
            }

            return () => {
                if (wasmModule) {
                    try {
                        wasmModule.exit();
                    } catch (error) {
                        if ((error as WasmExitStatus).status !== 0) {
                            // TODO: report error?
                        }
                    }
                }
            };
        } else {
            return () => {};
        }
    }, [canvasId, canvasRef, fonts, wasmModule, defaultFont, styleOverrides]);

    useEffect(() => {
        if (wasmModule) {
            if (containerRef?.current) {
                try {
                    wasmModule.resizeWindow(
                        containerRef.current.clientWidth,
                        containerRef.current.clientHeight - 62,
                    );
                } catch (exception) {
                    console.log("Unable to set initial window size");
                }
            }
        }
    }, [wasmModule]);

    useEffect(() => {
        if (wasmModule && containerRef?.current) {
            const resizeObserver = new ResizeObserver(
                debounce(() => {
                    if (containerRef.current) {
                        try {
                            wasmModule.resizeWindow(
                                containerRef.current.clientWidth - 1,
                                containerRef.current.clientHeight - 10,
                            );
                        } catch (exception) {
                            console.log("Unable to resize window");
                        }
                    }
                }, 20),
            );

            resizeObserver.observe(containerRef.current);

            return () => resizeObserver.disconnect(); // clean up
        } else {
            return () => {};
        }
    }, [wasmModule, containerRef]);

    return (
        <>
            {wasmModule && (
                <ReactNativeWrapper wasmModule={wasmModule}>{children}</ReactNativeWrapper>
            )}
            <canvas ref={canvasRef} id={canvasId} />
        </>
    );
};

export const XFrames = attachSubComponents("XFrames", MainComponent, components);

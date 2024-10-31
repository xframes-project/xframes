import * as React from "react";
import { useEffect, useRef, PropsWithChildren } from "react";
import ReactFabricProdInitialiser from "../src/lib/react-native/ReactFabric-prod";
import ReactNativePrivateInterface from "../src/lib/react-native/ReactNativePrivateInterface";
import { WidgetRegistrationService } from "../src/lib/widgetRegistrationService";
import { WidgetRegistrationServiceContext } from "../src/lib";

export const ReactFabricProd = ReactFabricProdInitialiser(ReactNativePrivateInterface);

export type ReactNativeWrapperProps = PropsWithChildren & {
    nodeImgui: any;
};

export const ReactNativeWrapper: React.ComponentType<ReactNativeWrapperProps> = ({
    nodeImgui,
    children,
}: ReactNativeWrapperProps) => {
    const widgetRegistrationServiceRef = useRef(new WidgetRegistrationService(nodeImgui));
    const initialisedRef = useRef(false);

    useEffect(() => {
        if (!initialisedRef.current) {
            // setTimeout(() => {
            //     console.log(wasmModule.getStyle());
            // }, 2000);

            initialisedRef.current = true;

            // todo: inject via Context
            ReactNativePrivateInterface.nativeFabricUIManager.init(
                nodeImgui,
                widgetRegistrationServiceRef.current,
            );

            ReactFabricProd.render(
                <WidgetRegistrationServiceContext.Provider
                    value={widgetRegistrationServiceRef.current}
                >
                    {children}
                </WidgetRegistrationServiceContext.Provider>,
                0, // containerTag,
                () => {
                    // console.log("initialised");
                },
                1,
            );
        }
    }, [nodeImgui, initialisedRef]);

    return null;
};

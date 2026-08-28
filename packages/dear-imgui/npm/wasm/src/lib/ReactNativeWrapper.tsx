import * as React from "react";
import { useEffect, useRef, PropsWithChildren } from "react";
import {
  WidgetRegistrationServiceContext,
  WidgetRegistrationService,
  ReactFabricInitialiser,
  ReactNativePrivateInterface,
} from "@xframes/common";
import { MainModule } from "./wasm-app-types";

const ReactFabric = ReactFabricInitialiser(ReactNativePrivateInterface);

export type ReactNativeWrapperProps = PropsWithChildren & {
  wasmModule: MainModule;
};

export const ReactNativeWrapper: React.ComponentType<
  ReactNativeWrapperProps
> = ({ wasmModule, children }: ReactNativeWrapperProps) => {
  const widgetRegistrationServiceRef = useRef(
    new WidgetRegistrationService(wasmModule),
  );
  const initialisedRef = useRef(false);

  useEffect(() => {
    if (wasmModule && !initialisedRef.current) {
      // setTimeout(() => {
      //     console.log(wasmModule.getStyle());
      // }, 2000);

      initialisedRef.current = true;

      // todo: inject via Context
      ReactNativePrivateInterface.nativeFabricUIManager.init(
        wasmModule,
        widgetRegistrationServiceRef.current,
      );

      ReactFabric.render(
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
        undefined,
      );
    }
  }, [wasmModule, initialisedRef]);

  return null;
};

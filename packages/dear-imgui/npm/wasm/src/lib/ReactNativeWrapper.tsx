import * as React from "react";
import { useEffect, useRef, PropsWithChildren } from "react";
import {
  WidgetRegistrationServiceContext,
  WidgetRegistrationService,
  ReactFabricInitialiser,
  ReactNativePrivateInterface,
  createReactNativeHost,
} from "@xframes/common";
import { MainModule } from "./wasm-app-types";

export type ReactNativeWrapperProps = PropsWithChildren & {
  wasmModule: MainModule;
  host?: typeof ReactNativePrivateInterface;
  onUnmount?: () => void;
};

export const ReactNativeWrapper: React.ComponentType<ReactNativeWrapperProps> =
({ wasmModule, children, host = ReactNativePrivateInterface, onUnmount }) => {
  const onUnmountRef = useRef(onUnmount);
  onUnmountRef.current = onUnmount;
  const session = useRef<{
    renderer: ReturnType<typeof ReactFabricInitialiser>;
    service: WidgetRegistrationService;
    generation: number;
  } | undefined>(undefined);

  useEffect(() => {
    if (!session.current) {
      if (host.nativeFabricUIManager.getDiagnostics().subscriptionClosed) {
        Object.defineProperty(host, "nativeFabricUIManager", {
          value: createReactNativeHost().nativeFabricUIManager, configurable: true,
        });
      }
      const service = new WidgetRegistrationService(wasmModule);
      host.nativeFabricUIManager.init(wasmModule, service);
      session.current = { service, renderer: ReactFabricInitialiser(host), generation: 0 };
    }
    const current = session.current;
    const manager = host.nativeFabricUIManager;
    const generation = ++current.generation;
    return () => {
      current.renderer.render(null, 0, () => {
        // Strict Mode may have set up the same still-mounted surface again.
        if (current.generation !== generation) return;
        current.renderer.stopSurface(0);
        manager.destroy();
        if (session.current === current) session.current = undefined;
        onUnmountRef.current?.();
      }, 1, undefined);
    };
  }, [wasmModule, host]);

  useEffect(() => {
    const current = session.current!;
    current.renderer.render(
      <WidgetRegistrationServiceContext.Provider value={current.service}>
        {children}
      </WidgetRegistrationServiceContext.Provider>,
      0, () => {}, 1, undefined,
    );
  }, [wasmModule, host, children]);

  return null;
};

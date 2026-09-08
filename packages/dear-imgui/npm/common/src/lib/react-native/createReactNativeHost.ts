import NativeFabricUIManager from "./nativeFabricUiManager";
import ReactNativePrivateInterface from "./ReactNativePrivateInterface";

/** Each embedded surface owns its bridge and event routing references. */
export function createReactNativeHost(): typeof ReactNativePrivateInterface {
    const host = Object.create(ReactNativePrivateInterface);
    Object.defineProperty(host, "nativeFabricUIManager", {
        value: new NativeFabricUIManager(), writable: true, configurable: true,
    });
    return host;
}

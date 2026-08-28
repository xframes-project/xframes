import assert from "node:assert/strict";
import ReactNativePrivateInterface from "../src/lib/react-native/ReactNativePrivateInterface.js";

const validAttributes = {
    label: true,
    onClick: true,
    style: true,
};

assert.deepEqual(
    ReactNativePrivateInterface.createAttributePayload(
        {
            ignored: "not registered",
            label: "Before",
            onClick: () => {},
            style: [{ width: 100 }, { height: 50 }],
        },
        validAttributes,
    ),
    {
        label: "Before",
        onClick: true,
        style: [{ width: 100 }, { height: 50 }],
    },
);

assert.deepEqual(
    ReactNativePrivateInterface.diffAttributePayloads(
        { label: "Before", onClick: () => {}, style: { width: 100 } },
        { label: "After", onClick: () => {}, style: { width: 120 } },
        validAttributes,
    ),
    { label: "After", style: { width: 120 } },
);

assert.deepEqual(
    ReactNativePrivateInterface.flattenStyle([{ width: 100, height: 20 }, null, [{ width: 120 }]]),
    { width: 120, height: 20 },
);

assert.deepEqual(ReactNativePrivateInterface.createPublicRootInstance(7), {
    containerTag: 7,
});
assert.equal(
    ReactNativePrivateInterface.nativeFabricUIManager.unstable_getCurrentEventPriority(),
    null,
);

console.log("React Native Fabric host adapter checks passed");

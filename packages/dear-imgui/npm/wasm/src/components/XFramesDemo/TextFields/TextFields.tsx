import { WidgetReactElement } from "@xframes/common";
import React, {
  SyntheticEvent,
  useCallback,
  useMemo,
  useRef,
  useState,
} from "react";
import { XFrames } from "src/lib";

export const TextFields = () => {
  const [text, setText] = useState("Hello, world!");
  const handleInputTextChanged = useCallback(
    (
      event: SyntheticEvent<WidgetReactElement<"InputText">, { value: string }>
    ) => {
      if (event?.nativeEvent) {
        setText(String(event?.nativeEvent.value));
      }
    },
    []
  );

  return (
    <>
      <XFrames.UnformattedText text="test" />
    </>
  );
};

import React, {
  SyntheticEvent,
  useCallback,
  useMemo,
  useRef,
  useState,
} from "react";
import { ImGuiCol } from "@xframes/common";
import { XFrames } from "src/lib";

export const Images = () => {
  return (
    <XFrames.Node
      style={{
        flexDirection: "row",
        gap: { column: 5 },
        alignItems: "center",
        border: {
          color: "red",
          thickness: 2,
        },
        width: "auto",
      }}
    >
      <XFrames.Image
        url="https://images.ctfassets.net/hrltx12pl8hq/28ECAQiPJZ78hxatLTa7Ts/2f695d869736ae3b0de3e56ceaca3958/free-nature-images.jpg?fit=fill&w=1200&h=630"
        width={40}
        height={40}
      />

      <XFrames.Node
        style={{
          width: 100,
          height: 100,
          backgroundColor: "#000000",
          border: {
            color: "lightgreen",
            thickness: 5,
          },
          rounding: 5,
          roundCorners: ["topLeft"],
        }}
      >
        <XFrames.UnformattedText
          text="Inside"
          style={{ colors: { [ImGuiCol.Text]: "#FFFFFF" } }}
        />
        <XFrames.Image
          url="https://images.ctfassets.net/hrltx12pl8hq/28ECAQiPJZ78hxatLTa7Ts/2f695d869736ae3b0de3e56ceaca3958/free-nature-images.jpg?fit=fill&w=1200&h=630"
          style={{
            flex: 1,
          }}
        />
      </XFrames.Node>
    </XFrames.Node>
  );
};

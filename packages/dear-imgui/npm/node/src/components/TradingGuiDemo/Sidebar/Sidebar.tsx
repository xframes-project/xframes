import React, {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import {
  faIconMap,
  ImGuiCol,
  ImGuiStyleVar,
  RWStyleSheet,
} from "@xframes/common";
import { XFrames } from "src/lib/XFrames";

export const Sidebar = () => {
  const styleSheet = useMemo(
    () =>
      RWStyleSheet.create({
        sidebarNode: {
          flexBasis: 60,
          height: "100%",
          border: {
            color: "#1B1D20",
            thickness: 1,
          },
        },
        sideBarItem: {
          width: 58,
          height: 58,
          justifyContent: "center",
          alignItems: "center",
        },
        logo: {
          font: { name: "roboto-regular", size: 36 },
        },
        icon: {
          width: 48,
          height: 48,
          font: { name: "roboto-regular", size: 36 },
          colors: {
            [ImGuiCol.ButtonHovered]: "#001033",
            [ImGuiCol.ButtonActive]: "#001033",
          },
          vars: { [ImGuiStyleVar.FrameRounding]: 24 },
        },
        iconActive: {
          width: 48,
          height: 48,
          font: { name: "roboto-regular", size: 36 },
          vars: { [ImGuiStyleVar.FrameRounding]: 24 },
          colors: {
            [ImGuiCol.Text]: "#588AF5",
            [ImGuiCol.Button]: "#001033",
            [ImGuiCol.ButtonHovered]: "#001033",
            [ImGuiCol.ButtonActive]: "#001033",
          },
        },
      }),
    []
  );

  return (
    <XFrames.Node style={styleSheet.sidebarNode}>
      <XFrames.Node style={styleSheet.sideBarItem}>
        <XFrames.UnformattedText
          text={faIconMap.otter}
          style={styleSheet.logo}
        />
      </XFrames.Node>

      <XFrames.Node style={styleSheet.sideBarItem}>
        <XFrames.Button
          label={faIconMap["arrow-trend-up"]}
          style={styleSheet.iconActive}
          hoverStyle={styleSheet.iconActive}
        />
      </XFrames.Node>
      <XFrames.Node style={styleSheet.sideBarItem}>
        <XFrames.Button
          label={faIconMap["wallet"]}
          style={styleSheet.icon}
          hoverStyle={styleSheet.iconActive}
        />
      </XFrames.Node>
      <XFrames.Node style={styleSheet.sideBarItem}>
        <XFrames.Button
          label={faIconMap["chart-pie"]}
          style={styleSheet.icon}
          hoverStyle={styleSheet.iconActive}
        />
      </XFrames.Node>
    </XFrames.Node>
  );
};

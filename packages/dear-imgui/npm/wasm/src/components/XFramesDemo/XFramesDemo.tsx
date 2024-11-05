import React, { useCallback, useMemo, useState } from "react";
import {
  useWidgetRegistrationService,
  TreeViewItem,
  RWStyleSheet,
  ImGuiCol,
  faIconMap,
} from "@xframes/common";
// import { HelpMarker } from "./HelpMarker/HelpMarker";
import { Tables } from "./Tables/Tables";
import { Maps } from "./Maps/Maps";
import { Plots } from "./Plots/Plots";
import { Images } from "./Images/Images";
import { Icons } from "./Icons/Icons";
import { TextFields } from "./TextFields/TextFields";
import { ClippedMultiLineTextRenderers } from "./ClippedMultiLineTextRenderers/ClippedMultiLineTextRenderers";
import { Sliders } from "./Sliders/Sliders";
import { XFrames } from "src/lib";

const componentMap = {
  textField: TextFields,
  icons: Icons,
  images: Images,
  sliders: Sliders,
  maps: Maps,
  plots: Plots,
  tables: Tables,
  clippedMultiLineTextRenderers: ClippedMultiLineTextRenderers,
};

type ComponentKeys = keyof typeof componentMap;

export const XFramesDemo = () => {
  const widgetRegistratonService = useWidgetRegistrationService();

  const [selectedItemIds, setSelectedItemIds] = useState<ComponentKeys[]>([
    "plots",
  ]);

  const treeViewItems: TreeViewItem[] = useMemo(() => {
    return [
      {
        itemId: "textField",
        label: "Text Field",
      },
      {
        itemId: "icons",
        label: "Icons",
      },
      {
        itemId: "images",
        label: "Images",
      },
      {
        itemId: "sliders",
        label: "Sliders",
      },
      {
        itemId: "maps",
        label: "Maps",
      },
      {
        itemId: "plots",
        label: "Plots",
      },
      {
        itemId: "tables",
        label: "Tables",
      },
      {
        itemId: "clippedMultiLineTextRenderers",
        label: "ClippedMultiLineTextRenderers",
      },
    ];
  }, []);

  const styleSheet = useMemo(
    () =>
      RWStyleSheet.create({
        rootNode: {
          height: "100%",
          padding: {
            all: 10,
          },
          gap: { row: 12 },
        },
        mainLayoutNode: {
          flex: 1,
          flexDirection: "row",
          gap: { column: 12 },
        },
        sidebarNode: {
          flexBasis: 200,
          height: "100%",
          border: {
            color: "#000",
            thickness: 1,
          },
        },
        contentNode: {
          flex: 1,
          height: "100%",
          border: {
            color: "#000",
            thickness: 1,
          },
          padding: { all: 5 },
        },
        title: {
          colors: { [ImGuiCol.Text]: "#ff6e59" },
          font: { name: "roboto-regular", size: 24 },
        },
        debugButton: {
          positionType: "absolute",
          position: { right: 15, bottom: 15 },
          flexDirection: "row",
          gap: { column: 10 },
        },
      }),
    []
  );

  const debugModeBtnClicked = useCallback(() => {
    widgetRegistratonService.setDebug(true);
  }, []);

  const onToggleItemSelection = useCallback(
    (itemId: string, selected: boolean) => {
      setSelectedItemIds((selection) => {
        if (selected) {
          return [itemId as ComponentKeys];
        } else {
          return selection.filter((item) => item !== itemId);
        }
      });
    },
    []
  );

  const Component = componentMap[selectedItemIds[0]];

  return (
    <XFrames.Node root style={styleSheet.rootNode}>
      <XFrames.UnformattedText
        text="XFrames bindings"
        style={styleSheet.title}
      />

      <XFrames.Node style={styleSheet.mainLayoutNode}>
        <XFrames.Node style={styleSheet.sidebarNode}>
          <XFrames.TreeView
            items={treeViewItems}
            selectedItemIds={selectedItemIds}
            onToggleItemSelection={onToggleItemSelection}
          />
        </XFrames.Node>
        <XFrames.Node style={styleSheet.contentNode} cull>
          {Component && <Component />}
        </XFrames.Node>
      </XFrames.Node>

      <XFrames.Node style={styleSheet.debugButton}>
        <XFrames.Button label={faIconMap.bug} onClick={debugModeBtnClicked} />
      </XFrames.Node>
    </XFrames.Node>
  );
};

import React, { useEffect, useMemo, useRef } from 'react';
import { PanelProps } from '@grafana/data';
import { SimpleOptions } from 'types';
import { css, cx } from '@emotion/css';
import { useStyles2 } from '@grafana/ui';
import { PanelDataErrorView } from '@grafana/runtime';
import { XFrames } from '@xframes/wasm';
import {
  XFramesStyleForPatching,
  ImGuiCol,
  PlotLineImperativeHandle,
  RWStyleSheet,
  ImPlotScale,
  ImPlotMarker,
} from '@xframes/common';

// @ts-ignore abc
import getWasmModule from '@xframes/wasm/dist/xframes.mjs';
// @ts-ignore aer
import wasmDataPackage from '@xframes/wasm/dist/xframes.data';

export const theme2Colors = {
  darkestGrey: '#141f2c',
  darkerGrey: '#2a2e39',
  darkGrey: '#363b4a',
  lightGrey: '#5a5a5a',
  lighterGrey: '#7A818C',
  evenLighterGrey: '#8491a3',
  black: '#0A0B0D',
  green: '#75f986',
  red: '#ff0062',
  white: '#fff',
};

export const theme2: XFramesStyleForPatching = {
  // frameBorderSize: 1,
  // framePadding: [0, 0],
  colors: {
    [ImGuiCol.Text]: [theme2Colors.white, 1],
    [ImGuiCol.TextDisabled]: [theme2Colors.lighterGrey, 1],
    [ImGuiCol.WindowBg]: [theme2Colors.black, 1],
    [ImGuiCol.ChildBg]: [theme2Colors.black, 1],
    [ImGuiCol.PopupBg]: [theme2Colors.white, 1],
    [ImGuiCol.Border]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.BorderShadow]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.FrameBg]: [theme2Colors.black, 1],
    [ImGuiCol.FrameBgHovered]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.FrameBgActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TitleBg]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TitleBgActive]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.TitleBgCollapsed]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.MenuBarBg]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ScrollbarBg]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ScrollbarGrab]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ScrollbarGrabHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ScrollbarGrabActive]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.CheckMark]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.SliderGrab]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.SliderGrabActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.Button]: [theme2Colors.black, 1],
    [ImGuiCol.ButtonHovered]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ButtonActive]: [theme2Colors.black, 1],
    [ImGuiCol.Header]: [theme2Colors.black, 1],
    [ImGuiCol.HeaderHovered]: [theme2Colors.black, 1],
    [ImGuiCol.HeaderActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.Separator]: [theme2Colors.darkestGrey, 1],
    [ImGuiCol.SeparatorHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.SeparatorActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ResizeGrip]: [theme2Colors.black, 1],
    [ImGuiCol.ResizeGripHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.ResizeGripActive]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.Tab]: [theme2Colors.black, 1],
    [ImGuiCol.TabHovered]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.TabActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TabUnfocused]: [theme2Colors.black, 1],
    [ImGuiCol.TabUnfocusedActive]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.PlotLines]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.PlotLinesHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.PlotHistogram]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.PlotHistogramHovered]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TableHeaderBg]: [theme2Colors.black, 1],
    [ImGuiCol.TableBorderStrong]: [theme2Colors.lightGrey, 1],
    [ImGuiCol.TableBorderLight]: [theme2Colors.darkerGrey, 1],
    [ImGuiCol.TableRowBg]: [theme2Colors.darkGrey, 1],
    [ImGuiCol.TableRowBgAlt]: [theme2Colors.darkerGrey, 1],
    [ImGuiCol.TextSelectedBg]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.DragDropTarget]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.NavHighlight]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.NavWindowingHighlight]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.NavWindowingDimBg]: [theme2Colors.darkerGrey, 1], //
    [ImGuiCol.ModalWindowDimBg]: [theme2Colors.darkerGrey, 1], //
  },
};

interface Props extends PanelProps<SimpleOptions> {}

const getStyles = () => {
  return {
    wrapper: css`
      font-family: Open Sans;
      position: relative;
    `,
  };
};

export const SimplePanel: React.FC<Props> = ({ options, data, width, height, fieldConfig, id }) => {
  const styles = useStyles2(getStyles);

  const containerRef = useRef<HTMLDivElement>(null);
  const plotRef = useRef<PlotLineImperativeHandle>(null);

  const fontDefs = useMemo(
    () => [
      { name: 'roboto-regular', sizes: [16] },
      { name: 'roboto-bold', sizes: [16] },
      { name: 'roboto-mono-regular', sizes: [16] },
    ],
    []
  );

  const defaultFont = useMemo(() => ({ name: 'roboto-regular', size: 16 }), []);

  const styleOverrides: XFramesStyleForPatching = useMemo(() => theme2, []);

  const styleSheet = useMemo(
    () =>
      RWStyleSheet.create({
        rootNode: {
          height: '100%',
        },
      }),
    []
  );

  useEffect(() => {
    if (plotRef.current && data.series.length > 0 && data.series[0].fields.length === 2) {
      plotRef.current.resetData();

      for (let i = 0; i < data.series[0].length; i++) {
        plotRef.current.appendData(data.series[0].fields[0].values[i] / 1000, data.series[0].fields[1].values[i]);
      }
    }
  }, [plotRef, data]);

  if (data.series.length === 0) {
    return <PanelDataErrorView fieldConfig={fieldConfig} panelId={id} data={data} needsStringField />;
  }

  return (
    <div
      ref={containerRef}
      className={cx(
        styles.wrapper,
        css`
          width: ${width}px;
          height: ${height}px;
        `
      )}
    >
      <XFrames
        wasmDataPackage={wasmDataPackage}
        getWasmModule={getWasmModule}
        containerRef={containerRef}
        fontDefs={fontDefs}
        defaultFont={defaultFont}
        styleOverrides={styleOverrides}
      >
        <XFrames.Node root style={styleSheet.rootNode}>
          <XFrames.PlotLine
            xAxisScale={ImPlotScale.Time}
            ref={plotRef}
            markerStyle={ImPlotMarker.None}
            style={{ width: '100%', height: '100%' }}
            axisAutoFit
          />
        </XFrames.Node>
      </XFrames>
    </div>
  );
};

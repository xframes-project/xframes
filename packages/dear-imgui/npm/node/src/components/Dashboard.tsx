import * as React from "react";
import { useCallback, useEffect, useRef, useState } from "react";
import { XFrames } from "../lib/XFrames";
import {
  RWStyleSheet,
  TableImperativeHandle,
  PlotLineImperativeHandle,
  PlotBarImperativeHandle,
  PlotHeatmapImperativeHandle,
  PlotScatterImperativeHandle,
  PlotCandlestickImperativeHandle,
  PlotHistogramImperativeHandle,
  PlotPieChartImperativeHandle,
  MapImperativeHandle,
  PlotCandlestickDataItem,
  TabItemChangeEvent,
  InputTextChangeEvent,
  ComboChangeEvent,
  SliderChangeEvent,
  MapZoomChangeEvent,
  CheckboxChangeEvent,
  TableRowClickEvent,
  TableItemActionEvent,
  WidgetPropsMap,
  useWidgetRegistrationService,
} from "@xframes/common";
import { theme1, theme2, theme3 } from "../themes";

const themeList = [
  { name: "Dark", theme: theme2 },
  { name: "Light", theme: theme1 },
  { name: "Ocean", theme: theme3 },
] as const;

// --- Sample data: 50 cities ---
const cityData = [
  { city: "Tokyo", country: "Japan", population: 37400068, area: 2191, megacity: true },
  { city: "Delhi", country: "India", population: 30290936, area: 1484, megacity: true },
  { city: "Shanghai", country: "China", population: 27058480, area: 6341, megacity: true },
  { city: "Sao Paulo", country: "Brazil", population: 22043028, area: 1521, megacity: true },
  { city: "Mexico City", country: "Mexico", population: 21782378, area: 1485, megacity: true },
  { city: "Cairo", country: "Egypt", population: 20901000, area: 3085, megacity: true },
  { city: "Mumbai", country: "India", population: 20411274, area: 603, megacity: true },
  { city: "Beijing", country: "China", population: 20384000, area: 16411, megacity: true },
  { city: "Dhaka", country: "Bangladesh", population: 21006000, area: 306, megacity: true },
  { city: "Osaka", country: "Japan", population: 19281000, area: 225, megacity: true },
  { city: "New York", country: "USA", population: 18819000, area: 783, megacity: true },
  { city: "Karachi", country: "Pakistan", population: 16094000, area: 3780, megacity: true },
  { city: "Buenos Aires", country: "Argentina", population: 15154000, area: 203, megacity: true },
  { city: "Chongqing", country: "China", population: 15872000, area: 82403, megacity: true },
  { city: "Istanbul", country: "Turkey", population: 15462000, area: 5461, megacity: true },
  { city: "Kolkata", country: "India", population: 14850000, area: 205, megacity: true },
  { city: "Manila", country: "Philippines", population: 13923000, area: 43, megacity: true },
  { city: "Lagos", country: "Nigeria", population: 13463000, area: 1172, megacity: true },
  { city: "Rio de Janeiro", country: "Brazil", population: 13458000, area: 1221, megacity: true },
  { city: "Tianjin", country: "China", population: 13215000, area: 11917, megacity: true },
  { city: "Kinshasa", country: "DR Congo", population: 14342000, area: 9965, megacity: true },
  { city: "Guangzhou", country: "China", population: 13081000, area: 7434, megacity: true },
  { city: "Los Angeles", country: "USA", population: 12458000, area: 1302, megacity: true },
  { city: "Moscow", country: "Russia", population: 12538000, area: 2511, megacity: true },
  { city: "Shenzhen", country: "China", population: 12357000, area: 1998, megacity: true },
  { city: "Lahore", country: "Pakistan", population: 12188000, area: 1772, megacity: true },
  { city: "Bangalore", country: "India", population: 11440000, area: 741, megacity: true },
  { city: "Paris", country: "France", population: 11017000, area: 105, megacity: true },
  { city: "Bogota", country: "Colombia", population: 10978000, area: 1587, megacity: true },
  { city: "Jakarta", country: "Indonesia", population: 10770000, area: 662, megacity: true },
  { city: "Chennai", country: "India", population: 10456000, area: 426, megacity: true },
  { city: "Lima", country: "Peru", population: 10391000, area: 2672, megacity: true },
  { city: "Bangkok", country: "Thailand", population: 10156000, area: 1569, megacity: true },
  { city: "Seoul", country: "South Korea", population: 9963000, area: 605, megacity: false },
  { city: "Nagoya", country: "Japan", population: 9507000, area: 326, megacity: false },
  { city: "Hyderabad", country: "India", population: 9482000, area: 650, megacity: false },
  { city: "London", country: "UK", population: 9046000, area: 1572, megacity: false },
  { city: "Tehran", country: "Iran", population: 8896000, area: 730, megacity: false },
  { city: "Chicago", country: "USA", population: 8864000, area: 606, megacity: false },
  { city: "Chengdu", country: "China", population: 8813000, area: 14378, megacity: false },
  { city: "Nanjing", country: "China", population: 8505000, area: 6587, megacity: false },
  { city: "Wuhan", country: "China", population: 8364000, area: 8494, megacity: false },
  { city: "Ho Chi Minh", country: "Vietnam", population: 8145000, area: 2096, megacity: false },
  { city: "Luanda", country: "Angola", population: 8045000, area: 18826, megacity: false },
  { city: "Ahmedabad", country: "India", population: 7681000, area: 464, megacity: false },
  { city: "Kuala Lumpur", country: "Malaysia", population: 7564000, area: 243, megacity: false },
  { city: "Hong Kong", country: "China", population: 7500000, area: 1104, megacity: false },
  { city: "Hangzhou", country: "China", population: 7236000, area: 16854, megacity: false },
  { city: "Riyadh", country: "Saudi Arabia", population: 7089000, area: 1913, megacity: false },
  { city: "Surat", country: "India", population: 6936000, area: 327, megacity: false },
];

const tableColumns: WidgetPropsMap["Table"]["columns"] = [
  { heading: "City", fieldId: "city", noHide: true },
  { heading: "Country", fieldId: "country" },
  { heading: "Population", fieldId: "population", type: "number" },
  { heading: "Area (km²)", fieldId: "area", type: "number", defaultHide: true },
  { heading: "Megacity", fieldId: "megacity", type: "boolean" },
];

const comboOptions = ["All", "Asia", "Europe", "Americas", "Africa"];

function generateCandlestickData(): PlotCandlestickDataItem[] {
  const data: PlotCandlestickDataItem[] = [];
  let price = 150;
  const startDate = new Date("2025-01-01").getTime() / 1000;
  for (let i = 0; i < 90; i++) {
    const date = startDate + i * 86400;
    const open = price;
    const change = (Math.random() - 0.48) * 5;
    const close = open + change;
    const low = Math.min(open, close) - Math.random() * 3;
    const high = Math.max(open, close) + Math.random() * 3;
    data.push({ date, open, close, low, high });
    price = close;
  }
  return data;
}

const styles = RWStyleSheet.create({
  scrollContainer: {
    width: "100%",
    flex: 1,
    overflow: "scroll",
  },
  row: {
    flexDirection: "row",
    width: "100%",
    height: 350,
    flexShrink: 0,
  },
  leftColumn: {
    flex: 1,
    padding: { all: 8 },
  },
  rightColumn: {
    flex: 1,
    padding: { all: 8 },
  },
  title: {
    padding: { all: 8 },
    font: { name: "roboto-regular", size: 24 },
  },
  formField: {
    margin: { bottom: 4 },
  },
  plotArea: {
    width: "100%",
    flex: 1,
  },
  tableArea: {
    width: "100%",
    flex: 1,
  },
  headerBar: {
    flexDirection: "row",
    alignItems: "center",
    gap: { column: 8 },
    padding: { left: 8, right: 8, bottom: 4 },
  },
  statusPanel: {
    padding: { all: 4 },
  },
  indicatorRow: {
    flexDirection: "row",
    gap: { column: 8 },
    margin: { bottom: 4 },
    alignItems: "center",
  },
  indicator: {
    width: 16,
    height: 16,
  },
});

export const Dashboard = () => {
  const widgetRegistrationService = useWidgetRegistrationService();

  const tableRef = useRef<TableImperativeHandle>(null);
  const plotRef = useRef<PlotLineImperativeHandle>(null);
  const barRef = useRef<PlotBarImperativeHandle>(null);
  const heatmapRef = useRef<PlotHeatmapImperativeHandle>(null);
  const scatterRef = useRef<PlotScatterImperativeHandle>(null);
  const candlestickRef = useRef<PlotCandlestickImperativeHandle>(null);
  const histogramRef = useRef<PlotHistogramImperativeHandle>(null);
  const pieChartRef = useRef<PlotPieChartImperativeHandle>(null);
  const mapRef = useRef<MapImperativeHandle>(null);

  const [dataPointCount, setDataPointCount] = useState(0);
  const [mapZoom, setMapZoom] = useState(13);
  const [frequency, setFrequency] = useState(3);
  const [themeIndex, setThemeIndex] = useState(0);
  const [inputValue, setInputValue] = useState("");
  const [selectedCategory, setSelectedCategory] = useState("All");
  const [featureEnabled, setFeatureEnabled] = useState(false);
  const [selectedColor, setSelectedColor] = useState("");
  const [showNotesTab, setShowNotesTab] = useState(true);

  // Load initial table data
  useEffect(() => {
    const timer = setTimeout(() => {
      if (tableRef.current) {
        tableRef.current.setTableData(cityData);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Load bar chart data
  useEffect(() => {
    const timer = setTimeout(() => {
      if (barRef.current) {
        barRef.current.setData([
          { x: 1, y: 45 },
          { x: 2, y: 72 },
          { x: 3, y: 58 },
          { x: 4, y: 91 },
          { x: 5, y: 36 },
          { x: 6, y: 67 },
          { x: 7, y: 83 },
        ]);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Load scatter plot data
  useEffect(() => {
    const timer = setTimeout(() => {
      if (scatterRef.current) {
        const data = [];
        for (let i = 0; i < 100; i++) {
          data.push({
            x: Math.cos(i * 0.1) * 10 + Math.random() * 4 - 2,
            y: Math.sin(i * 0.1) * 10 + Math.random() * 4 - 2,
          });
        }
        scatterRef.current.setData(data);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Load heatmap data (8x8 gradient)
  useEffect(() => {
    const timer = setTimeout(() => {
      if (heatmapRef.current) {
        const rows = 8;
        const cols = 8;
        const values: number[] = [];
        for (let r = 0; r < rows; r++) {
          for (let c = 0; c < cols; c++) {
            values.push(Math.sin(r * 0.5) * Math.cos(c * 0.5) * 50 + 50);
          }
        }
        heatmapRef.current.setData(rows, cols, values);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Load histogram data (normal distribution via Box-Muller)
  useEffect(() => {
    const timer = setTimeout(() => {
      if (histogramRef.current) {
        const values: number[] = [];
        for (let i = 0; i < 500; i++) {
          const u1 = Math.random();
          const u2 = Math.random();
          const z = Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2);
          values.push(z * 15 + 50);
        }
        histogramRef.current.setData(values);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Load pie chart data
  useEffect(() => {
    const timer = setTimeout(() => {
      if (pieChartRef.current) {
        pieChartRef.current.setData([
          { label: "Desktop", value: 62 },
          { label: "Mobile", value: 28 },
          { label: "Tablet", value: 10 },
        ]);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Render initial map (London)
  useEffect(() => {
    const timer = setTimeout(() => {
      if (mapRef.current) {
        mapRef.current.render(-0.1276, 51.5074, mapZoom);
      }
    }, 500);
    return () => clearTimeout(timer);
  }, []);

  // Load candlestick data
  useEffect(() => {
    const timer = setTimeout(() => {
      if (candlestickRef.current) {
        candlestickRef.current.setData(generateCandlestickData());
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Live sine wave plot
  useEffect(() => {
    let x = 0;
    const interval = setInterval(() => {
      if (plotRef.current) {
        plotRef.current.appendSeriesData(0, x, Math.sin(x * frequency * 0.1));
        plotRef.current.appendSeriesData(1, x, Math.cos(x * frequency * 0.1));
        x += 0.1;
        setDataPointCount((prev) => prev + 1);
      }
    }, 50);
    return () => clearInterval(interval);
  }, [frequency]);

  const handleFreqChange = useCallback((event: SliderChangeEvent) => {
    setFrequency(event.nativeEvent.value);
  }, []);

  const handleResetPlot = useCallback(() => {
    if (plotRef.current) {
      plotRef.current.resetData();
      setDataPointCount(0);
    }
  }, []);

  const handleThemeChange = useCallback((event: ComboChangeEvent) => {
    const index = event.nativeEvent.value;
    (widgetRegistrationService as any).wasmModule.patchStyle(
      JSON.stringify(themeList[index].theme),
    );
    setThemeIndex(index);
  }, [widgetRegistrationService]);

  const handleInputChange = useCallback((event: InputTextChangeEvent) => {
    setInputValue(event.nativeEvent.value);
  }, []);

  const handleComboChange = useCallback((event: ComboChangeEvent) => {
    setSelectedCategory(comboOptions[event.nativeEvent.value]);
  }, []);

  const handleCheckboxChange = useCallback((event: CheckboxChangeEvent) => {
    setFeatureEnabled(event.nativeEvent.value);
  }, []);

  const handleRowClick = useCallback((event: TableRowClickEvent) => {
    console.log(`Row clicked: index=${event.nativeEvent.rowIndex}`);
  }, []);

  const handleItemAction = useCallback((event: TableItemActionEvent) => {
    console.log(`Context menu action: row=${event.nativeEvent.rowIndex}, action=${event.nativeEvent.actionId}`);
  }, []);

  const handleResetForm = useCallback(() => {
    setInputValue("");
    setSelectedCategory("All");
    setFeatureEnabled(false);
  }, []);

  const handleRenderMap = useCallback(() => {
    if (mapRef.current) {
      mapRef.current.render(-0.1276, 51.5074, mapZoom);
    }
  }, [mapZoom]);

  const handleZoomChange = useCallback((event: SliderChangeEvent) => {
    setMapZoom(event.nativeEvent.value);
  }, []);

  const handleMapZoomChange = useCallback((event: MapZoomChangeEvent) => {
    setMapZoom(event.nativeEvent.value);
  }, []);

  const handleTabClose = useCallback((event: TabItemChangeEvent) => {
    if (!event.nativeEvent.value) {
      setShowNotesTab(false);
    }
  }, []);

  return (
    <>
      <XFrames.UnformattedText
        style={styles.title}
        text="XFrames Dashboard"
      />

      <XFrames.Node style={styles.headerBar}>
        <XFrames.UnformattedText text="Theme:" />
        <XFrames.Combo
          options={themeList.map((t) => t.name)}
          initialSelectedIndex={0}
          onChange={handleThemeChange}
        />
      </XFrames.Node>

      <XFrames.Node style={styles.scrollContainer}>
        {/* Top row: Table + Plot */}
        <XFrames.Node style={styles.row}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="World Cities (sort, filter & select)" />
            <XFrames.Table
              ref={tableRef}
              columns={tableColumns}
              clipRows={20}
              filterable
              reorderable
              hideable
              onRowClick={handleRowClick}
              contextMenuItems={[
                { id: "view", label: "View Details" },
                { id: "delete", label: "Delete" },
              ]}
              onItemAction={handleItemAction}
              style={styles.tableArea}
            />
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            <XFrames.UnformattedText text="Live Sine Wave" />
            <XFrames.PlotLine
              ref={plotRef}
              axisAutoFit
              xAxisLabel="Time"
              yAxisLabel="Amplitude"
              showLegend
              series={[{ label: "Sin" }, { label: "Cos" }]}
              style={styles.plotArea}
            />
            <XFrames.UnformattedText text={`Frequency: ${frequency}`} />
            <XFrames.Slider
              defaultValue={3}
              min={1}
              max={10}
              onChange={handleFreqChange}
            />
            <XFrames.Button label="Reset Plot" onClick={handleResetPlot} />
          </XFrames.Node>
        </XFrames.Node>

        {/* Bottom row: Form + Status */}
        <XFrames.Node style={styles.row}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="Form Controls" />
            <XFrames.InputText
              hint="Type something..."
              onChange={handleInputChange}
              style={styles.formField}
            />
            {inputValue ? (
              <XFrames.UnformattedText text={`You typed: ${inputValue}`} />
            ) : null}
            <XFrames.InputText
              hint="Password..."
              password
              onChange={handleInputChange}
              style={styles.formField}
            />
            <XFrames.InputText
              defaultValue="This is read-only"
              readOnly
              style={styles.formField}
            />
            <XFrames.InputText
              hint="Numbers only..."
              numericOnly
              style={styles.formField}
            />
            <XFrames.Combo
              options={comboOptions}
              initialSelectedIndex={0}
              onChange={handleComboChange}
              style={styles.formField}
            />
            <XFrames.Checkbox
              label="Enable feature"
              onChange={handleCheckboxChange}
              style={styles.formField}
            />
            <XFrames.UnformattedText
              text={`Feature: ${featureEnabled ? "ON" : "OFF"}`}
            />
            <XFrames.UnformattedText text="Pick a color" />
            <XFrames.ColorPicker
              defaultColor="#26a69a"
              onChange={(e) => setSelectedColor(e.nativeEvent.value)}
              style={styles.formField}
            />
            {selectedColor ? (
              <XFrames.UnformattedText text={`Color: ${selectedColor}`} />
            ) : null}
            <XFrames.Button label="Reset Form" onClick={handleResetForm} />
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            <XFrames.UnformattedText text="Status Indicators" />
            <XFrames.Node style={styles.indicatorRow}>
              <XFrames.ColorIndicator color="#26a69a" shape="circle" style={styles.indicator} />
              <XFrames.UnformattedText text="Connected" />
            </XFrames.Node>
            <XFrames.Node style={styles.indicatorRow}>
              <XFrames.ColorIndicator color="#ff9800" shape="circle" style={styles.indicator} />
              <XFrames.UnformattedText text="Waiting" />
            </XFrames.Node>
            <XFrames.Node style={styles.indicatorRow}>
              <XFrames.ColorIndicator color="#ef5350" shape="rect" style={styles.indicator} />
              <XFrames.UnformattedText text="Error" />
            </XFrames.Node>

            <XFrames.UnformattedText text="Progress" />
            <XFrames.ProgressBar fraction={0.75} overlay="75%" />
            <XFrames.ProgressBar fraction={0.42} overlay="Uploading..." />
            <XFrames.ProgressBar fraction={1.0} />

            <XFrames.UnformattedText text="Status" />
            <XFrames.Node style={styles.statusPanel}>
              <XFrames.UnformattedText text={`Category: ${selectedCategory}`} />
              <XFrames.UnformattedText text={`Data points: ${dataPointCount}`} />
            </XFrames.Node>
          </XFrames.Node>
        </XFrames.Node>

        {/* Third row: Bar Chart + Scatter Plot */}
        <XFrames.Node style={styles.row}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="Bar Chart" />
            <XFrames.PlotBar
              ref={barRef}
              axisAutoFit
              xAxisLabel="Category"
              yAxisLabel="Value"
              style={styles.plotArea}
            />
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            <XFrames.UnformattedText text="Scatter Plot" />
            <XFrames.PlotScatter
              ref={scatterRef}
              axisAutoFit
              xAxisLabel="X"
              yAxisLabel="Y"
              style={styles.plotArea}
            />
          </XFrames.Node>
        </XFrames.Node>

        {/* Fourth row: Heatmap + Candlestick */}
        <XFrames.Node style={styles.row}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="Heatmap" />
            <XFrames.PlotHeatmap
              ref={heatmapRef}
              axisAutoFit
              style={styles.plotArea}
            />
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            <XFrames.UnformattedText text="Candlestick Chart (synthetic data)" />
            <XFrames.PlotCandlestick
              ref={candlestickRef}
              axisAutoFit
              bullColor="#26a69a"
              bearColor="#ef5350"
              style={styles.plotArea}
            />
          </XFrames.Node>
        </XFrames.Node>

        {/* Fifth row: Histogram */}
        <XFrames.Node style={styles.row}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="Histogram (normal distribution)" />
            <XFrames.PlotHistogram
              ref={histogramRef}
              axisAutoFit
              xAxisLabel="Value"
              yAxisLabel="Count"
              style={styles.plotArea}
            />
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            <XFrames.UnformattedText text="Pie Chart (device breakdown)" />
            <XFrames.PlotPieChart
              ref={pieChartRef}
              normalize
              showLegend
              style={styles.plotArea}
            />
          </XFrames.Node>
        </XFrames.Node>

        {/* Sixth row: Map View */}
        <XFrames.Node style={{ ...styles.row, height: 400 }}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="Map View (London)" />
            <XFrames.MapView
              ref={mapRef}
              style={styles.plotArea}
              onChange={handleMapZoomChange}
            />
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            <XFrames.UnformattedText text="Map Controls" />
            <XFrames.UnformattedText text="Center: -0.1276, 51.5074 (London)" />
            <XFrames.UnformattedText text={`Zoom: ${mapZoom}`} />
            <XFrames.Slider
              defaultValue={13}
              min={1}
              max={17}
              onChange={handleZoomChange}
            />
            <XFrames.Button label="Render Map" onClick={handleRenderMap} />
          </XFrames.Node>
        </XFrames.Node>

        {/* Seventh row: Tabs demo */}
        <XFrames.Node style={{ ...styles.row, height: 200 }}>
          <XFrames.Node style={styles.leftColumn}>
            <XFrames.UnformattedText text="Tabs (reorderable + closeable)" />
            <XFrames.TabBar reorderable style={{ width: "100%", flex: 1 }}>
              <XFrames.TabItem label="Overview" style={{ width: "100%", height: "100%" }}>
                <XFrames.UnformattedText text="Drag tabs to reorder them." />
              </XFrames.TabItem>
              <XFrames.TabItem label="Details" style={{ width: "100%", height: "100%" }}>
                <XFrames.UnformattedText text="This is the Details tab." />
              </XFrames.TabItem>
              {showNotesTab ? (
                <XFrames.TabItem label="Notes" closeable onChange={handleTabClose} style={{ width: "100%", height: "100%" }}>
                  <XFrames.UnformattedText text="Close this tab with the X button." />
                </XFrames.TabItem>
              ) : null}
            </XFrames.TabBar>
          </XFrames.Node>

          <XFrames.Node style={styles.rightColumn}>
            {!showNotesTab ? (
              <XFrames.Button label="Restore Notes Tab" onClick={() => setShowNotesTab(true)} />
            ) : null}
          </XFrames.Node>
        </XFrames.Node>
      </XFrames.Node>
    </>
  );
};

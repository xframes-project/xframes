import * as React from "react";
import { useCallback, useEffect, useRef, useState } from "react";
import { XFrames } from "../lib/XFrames";
import {
  RWStyleSheet,
  TableImperativeHandle,
  PlotLineImperativeHandle,
  InputTextChangeEvent,
  ComboChangeEvent,
  SliderChangeEvent,
  CheckboxChangeEvent,
  useWidgetRegistrationService,
} from "@xframes/common";
import { theme1, theme2 } from "../themes";

// --- Sample data: 50 cities ---
const cityData = [
  { city: "Tokyo", country: "Japan", population: "37400068", area: "2191" },
  { city: "Delhi", country: "India", population: "30290936", area: "1484" },
  { city: "Shanghai", country: "China", population: "27058480", area: "6341" },
  { city: "Sao Paulo", country: "Brazil", population: "22043028", area: "1521" },
  { city: "Mexico City", country: "Mexico", population: "21782378", area: "1485" },
  { city: "Cairo", country: "Egypt", population: "20901000", area: "3085" },
  { city: "Mumbai", country: "India", population: "20411274", area: "603" },
  { city: "Beijing", country: "China", population: "20384000", area: "16411" },
  { city: "Dhaka", country: "Bangladesh", population: "21006000", area: "306" },
  { city: "Osaka", country: "Japan", population: "19281000", area: "225" },
  { city: "New York", country: "USA", population: "18819000", area: "783" },
  { city: "Karachi", country: "Pakistan", population: "16094000", area: "3780" },
  { city: "Buenos Aires", country: "Argentina", population: "15154000", area: "203" },
  { city: "Chongqing", country: "China", population: "15872000", area: "82403" },
  { city: "Istanbul", country: "Turkey", population: "15462000", area: "5461" },
  { city: "Kolkata", country: "India", population: "14850000", area: "205" },
  { city: "Manila", country: "Philippines", population: "13923000", area: "43" },
  { city: "Lagos", country: "Nigeria", population: "13463000", area: "1172" },
  { city: "Rio de Janeiro", country: "Brazil", population: "13458000", area: "1221" },
  { city: "Tianjin", country: "China", population: "13215000", area: "11917" },
  { city: "Kinshasa", country: "DR Congo", population: "14342000", area: "9965" },
  { city: "Guangzhou", country: "China", population: "13081000", area: "7434" },
  { city: "Los Angeles", country: "USA", population: "12458000", area: "1302" },
  { city: "Moscow", country: "Russia", population: "12538000", area: "2511" },
  { city: "Shenzhen", country: "China", population: "12357000", area: "1998" },
  { city: "Lahore", country: "Pakistan", population: "12188000", area: "1772" },
  { city: "Bangalore", country: "India", population: "11440000", area: "741" },
  { city: "Paris", country: "France", population: "11017000", area: "105" },
  { city: "Bogota", country: "Colombia", population: "10978000", area: "1587" },
  { city: "Jakarta", country: "Indonesia", population: "10770000", area: "662" },
  { city: "Chennai", country: "India", population: "10456000", area: "426" },
  { city: "Lima", country: "Peru", population: "10391000", area: "2672" },
  { city: "Bangkok", country: "Thailand", population: "10156000", area: "1569" },
  { city: "Seoul", country: "South Korea", population: "9963000", area: "605" },
  { city: "Nagoya", country: "Japan", population: "9507000", area: "326" },
  { city: "Hyderabad", country: "India", population: "9482000", area: "650" },
  { city: "London", country: "UK", population: "9046000", area: "1572" },
  { city: "Tehran", country: "Iran", population: "8896000", area: "730" },
  { city: "Chicago", country: "USA", population: "8864000", area: "606" },
  { city: "Chengdu", country: "China", population: "8813000", area: "14378" },
  { city: "Nanjing", country: "China", population: "8505000", area: "6587" },
  { city: "Wuhan", country: "China", population: "8364000", area: "8494" },
  { city: "Ho Chi Minh", country: "Vietnam", population: "8145000", area: "2096" },
  { city: "Luanda", country: "Angola", population: "8045000", area: "18826" },
  { city: "Ahmedabad", country: "India", population: "7681000", area: "464" },
  { city: "Kuala Lumpur", country: "Malaysia", population: "7564000", area: "243" },
  { city: "Hong Kong", country: "China", population: "7500000", area: "1104" },
  { city: "Hangzhou", country: "China", population: "7236000", area: "16854" },
  { city: "Riyadh", country: "Saudi Arabia", population: "7089000", area: "1913" },
  { city: "Surat", country: "India", population: "6936000", area: "327" },
];

const tableColumns = [
  { heading: "City", fieldId: "city" },
  { heading: "Country", fieldId: "country" },
  { heading: "Population", fieldId: "population" },
  { heading: "Area (km²)", fieldId: "area" },
];

const comboOptions = ["All", "Asia", "Europe", "Americas", "Africa"];

const styles = RWStyleSheet.create({
  row: {
    flexDirection: "row",
    width: "100%",
    flex: 1,
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
  statusPanel: {
    padding: { all: 4 },
  },
});

export const Dashboard = () => {
  const widgetRegistrationService = useWidgetRegistrationService();

  const tableRef = useRef<TableImperativeHandle>(null);
  const plotRef = useRef<PlotLineImperativeHandle>(null);

  const [dataPointCount, setDataPointCount] = useState(0);
  const [frequency, setFrequency] = useState(3);
  const [themeName, setThemeName] = useState("Dark");
  const [inputValue, setInputValue] = useState("");
  const [selectedCategory, setSelectedCategory] = useState("All");
  const [featureEnabled, setFeatureEnabled] = useState(false);

  // Load initial table data
  useEffect(() => {
    const timer = setTimeout(() => {
      if (tableRef.current) {
        tableRef.current.setTableData(cityData);
      }
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Live sine wave plot
  useEffect(() => {
    let x = 0;
    const interval = setInterval(() => {
      if (plotRef.current) {
        const y = Math.sin(x * frequency * 0.1);
        plotRef.current.appendData(x, y);
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

  const handleThemeToggle = useCallback(() => {
    const nextTheme = themeName === "Dark" ? "Light" : "Dark";
    const themeObj = nextTheme === "Dark" ? theme2 : theme1;
    (widgetRegistrationService as any).wasmModule.patchStyle(JSON.stringify(themeObj));
    setThemeName(nextTheme);
  }, [themeName, widgetRegistrationService]);

  const handleInputChange = useCallback((event: InputTextChangeEvent) => {
    setInputValue(event.nativeEvent.value);
  }, []);

  const handleComboChange = useCallback((event: ComboChangeEvent) => {
    setSelectedCategory(comboOptions[event.nativeEvent.value]);
  }, []);

  const handleCheckboxChange = useCallback((event: CheckboxChangeEvent) => {
    setFeatureEnabled(event.nativeEvent.value);
  }, []);

  const handleResetForm = useCallback(() => {
    setInputValue("");
    setSelectedCategory("All");
    setFeatureEnabled(false);
  }, []);

  return (
    <>
      <XFrames.UnformattedText
        style={styles.title}
        text="XFrames Dashboard"
      />

      {/* Top row: Table + Plot */}
      <XFrames.Node style={styles.row}>
        <XFrames.Node style={styles.leftColumn}>
          <XFrames.UnformattedText text="World Cities (sort & filter)" />
          <XFrames.Table
            ref={tableRef}
            columns={tableColumns}
            clipRows={20}
            filterable
            style={styles.tableArea}
          />
        </XFrames.Node>

        <XFrames.Node style={styles.rightColumn}>
          <XFrames.UnformattedText text="Live Sine Wave" />
          <XFrames.PlotLine
            ref={plotRef}
            axisAutoFit
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
          <XFrames.Button label="Reset Form" onClick={handleResetForm} />
        </XFrames.Node>

        <XFrames.Node style={styles.rightColumn}>
          <XFrames.UnformattedText text="Status" />
          <XFrames.Node style={styles.statusPanel}>
            <XFrames.UnformattedText text={`Theme: ${themeName}`} />
            <XFrames.UnformattedText text={`Category: ${selectedCategory}`} />
            <XFrames.UnformattedText text={`Data points: ${dataPointCount}`} />
            <XFrames.Button
              label="Toggle Theme"
              onClick={handleThemeToggle}
            />
          </XFrames.Node>
        </XFrames.Node>
      </XFrames.Node>
    </>
  );
};

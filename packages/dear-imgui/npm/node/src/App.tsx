import * as React from "react";
import { XFrames } from "./lib/XFrames";
import { Dashboard } from "./components/Dashboard";

export const App = () => {
  return (
    <XFrames.Node root style={{ width: "100%", height: "100%" }}>
      <Dashboard />
    </XFrames.Node>
  );
};

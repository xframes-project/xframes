#!/usr/bin/env node

import os from "os";
import path from "path";
import fs from "fs";
import { execSync, execFileSync } from "child_process";
import { input } from "@inquirer/prompts";

const projectName = (
  await input({ message: "Enter your project name" })
).trim();

if (projectName === "") {
  console.log("Please enter a project name");
  process.exit(1);
}

const currentPath = process.cwd();
const projectPath = path.join(currentPath, projectName);
const srcPath = path.join(projectPath, "src");
const assetsPath = path.join(projectPath, "assets");
const fontsPath = path.join(assetsPath, "fonts");

const indexTsxPath = path.join(srcPath, "index.tsx");
const themesTsPath = path.join(srcPath, "themes.ts");

const tsConfigPath = path.join(projectPath, "tsconfig.json");
const robotoRegularPath = path.join(fontsPath, "roboto-regular.ttf");

try {
  fs.mkdirSync(projectPath);
} catch (err) {
  if (err.code === "EEXIST") {
    console.log(
      `The folder ${projectName} already exists, please choose a different name.`
    );
  } else {
    console.log(error);
  }
  process.exit(1);
}

try {
  fs.mkdirSync(srcPath);
} catch (err) {
  console.log(`Unable create src path: ${error}`);
  process.exit(1);
}

try {
  fs.mkdirSync(fontsPath, { recursive: true });
} catch (err) {
  console.log(`Unable create src path: ${error}`);
  process.exit(1);
}

const packageJson = {
  name: projectName,
  version: "0.1.0",
  private: true,
  dependencies: {
    "@xframes/common": "0.0.13",
    "@xframes/node": "0.0.18",
    react: "18.3.1",
    tsx: "4.19.2",
  },
  scripts: {
    start: "tsx ./src/index.tsx",
  },
};
fs.writeFileSync(
  path.join(projectPath, "package.json"),
  JSON.stringify(packageJson, null, 2) + os.EOL
);

const indexTsxUrl =
  "https://raw.githubusercontent.com/andreamancuso/xframes/refs/heads/main/packages/dear-imgui/npm/create-xframes-node-app/index.tsx";
const themesTsUrl =
  "https://raw.githubusercontent.com/andreamancuso/xframes/refs/heads/main/packages/dear-imgui/npm/create-xframes-node-app/themes.ts";
const tsConfigUrl =
  "https://raw.githubusercontent.com/andreamancuso/xframes/refs/heads/main/packages/dear-imgui/npm/create-xframes-node-app/tsconfig.json";
const robotoRegularUrl =
  "https://raw.githubusercontent.com/andreamancuso/xframes/refs/heads/main/packages/dear-imgui/npm/create-xframes-node-app/roboto-regular.ttf";

console.log("Downloading source file...");

execFileSync("curl", ["-o", indexTsxPath, "--silent", "-L", indexTsxUrl], {
  encoding: "utf8",
});

execFileSync("curl", ["-o", themesTsPath, "--silent", "-L", themesTsUrl], {
  encoding: "utf8",
});

execFileSync("curl", ["-o", tsConfigPath, "--silent", "-L", tsConfigUrl], {
  encoding: "utf8",
});

execFileSync(
  "curl",
  ["-o", robotoRegularPath, "--silent", "-L", robotoRegularUrl],
  {
    encoding: "utf8",
  }
);

process.chdir(projectPath);

console.log("Running npm install...");

execSync("npm install");

process.chdir(currentPath);

console.log("Assuming no errors occurred, you're all done. You may now run");

console.log(`cd ${projectName}`);

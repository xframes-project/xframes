#!/usr/bin/env node

import os from "os";
import path from "path";
import fs from "fs";
import { execSync } from "child_process";
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

// Update these versions when publishing new releases to npm
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

const templateDir = path.dirname(new URL(import.meta.url).pathname);
const normalizedTemplateDir = process.platform === "win32"
  ? templateDir.replace(/^\//, "")
  : templateDir;

console.log("Copying template files...");

fs.copyFileSync(path.join(normalizedTemplateDir, "index.tsx"), indexTsxPath);
fs.copyFileSync(path.join(normalizedTemplateDir, "themes.ts"), themesTsPath);
fs.copyFileSync(path.join(normalizedTemplateDir, "tsconfig.json"), tsConfigPath);
fs.copyFileSync(path.join(normalizedTemplateDir, "roboto-regular.ttf"), robotoRegularPath);

process.chdir(projectPath);

console.log("Running npm install...");

execSync("npm install");

process.chdir(currentPath);

console.log("All done! To start your app:\n");
console.log(`  cd ${projectName}`);
console.log("  npm start\n");

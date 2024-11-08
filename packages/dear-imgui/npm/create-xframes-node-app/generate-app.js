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
const indexJsPath = path.join(srcPath, "index.js");

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

const packageJson = {
  name: projectName,
  version: "0.1.0",
  private: true,
  dependencies: {
    "@xframes/common": "0.0.13",
    "@xframes/node": "0.0.18",
    react: "18.3.1",
  },
  scripts: {
    start: "node ./src/index.js",
  },
};
fs.writeFileSync(
  path.join(projectPath, "package.json"),
  JSON.stringify(packageJson, null, 2) + os.EOL
);

const indexJsUrl =
  "https://raw.githubusercontent.com/andreamancuso/xframes/refs/heads/main/packages/dear-imgui/npm/create-xframes-node-app/index.js";

console.log("Downloading source file...");

execFileSync("curl", ["-o", indexJsPath, "--silent", "-L", indexJsUrl], {
  encoding: "utf8",
});

process.chdir(projectPath);

console.log("Running npm install...");

execSync("npm install");

process.chdir(currentPath);

console.log("Assuming no errors occurred, you're all done. You may now run");

console.log(`cd ${projectName}`);

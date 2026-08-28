import { spawn } from "node:child_process";
import { createRequire } from "node:module";
import {
  existsSync,
  mkdirSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));
const packageRoot = path.resolve(scriptDirectory, "..");
const localRequire = createRequire(import.meta.url);
const webpackCli = localRequire.resolve("webpack-cli/bin/cli.js");
const serverUrl = "http://127.0.0.1:3000";
const debuggingPort = 9333;
const protocolTimeoutMilliseconds = 10_000;
const outputPath = path.join(packageRoot, "build", "browser-smoke.png");
const webGpuFlags =
  process.env.XFRAMES_WEBGPU_ADAPTER === "default"
    ? ["--enable-unsafe-webgpu", "--enable-webgpu-developer-features"]
    : [
        "--disable-gpu-sandbox",
        "--enable-unsafe-webgpu",
        "--use-webgpu-adapter=swiftshader",
        "--enable-dawn-features=allow_unsafe_apis",
        "--disable-dawn-features=use_dxc",
        "--enable-webgpu-developer-features",
        "--use-gpu-in-tests",
        "--enable-accelerated-2d-canvas",
      ];
const browserCandidates = [
  process.env.XFRAMES_BROWSER,
  "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
  "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
  "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
  "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
].filter(Boolean);

const browserPath = browserCandidates.find((candidate) =>
  existsSync(candidate),
);
if (!browserPath) {
  throw new Error(
    "No supported Chromium browser found. Set XFRAMES_BROWSER to Edge or Chrome.",
  );
}

const delay = (milliseconds) =>
  new Promise((resolve) => setTimeout(resolve, milliseconds));

const waitForJson = async (url, timeoutMilliseconds) => {
  const deadline = Date.now() + timeoutMilliseconds;
  let lastError;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) {
        return response.json();
      }
    } catch (error) {
      lastError = error;
    }
    await delay(250);
  }

  throw new Error(`Timed out waiting for ${url}`, { cause: lastError });
};

const waitForHttp = async (url, timeoutMilliseconds) => {
  const deadline = Date.now() + timeoutMilliseconds;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) return;
    } catch {}
    await delay(250);
  }
  throw new Error(`Timed out waiting for ${url}`);
};

const createPageTarget = async () => {
  await waitForJson(`http://127.0.0.1:${debuggingPort}/json/version`, 30_000);
  const response = await fetch(
    `http://127.0.0.1:${debuggingPort}/json/new?${encodeURIComponent("about:blank")}`,
    { method: "PUT" },
  );
  if (!response.ok) {
    throw new Error(
      `Unable to create Chromium page target: HTTP ${response.status}`,
    );
  }
  const createdTarget = await response.json();
  const deadline = Date.now() + 30_000;
  let lastTarget = createdTarget;

  while (Date.now() < deadline) {
    const targets = await waitForJson(
      `http://127.0.0.1:${debuggingPort}/json/list`,
      30_000,
    );
    lastTarget =
      targets.find((target) => target.id === createdTarget.id) ?? lastTarget;
    if (lastTarget.url === "about:blank" && lastTarget.webSocketDebuggerUrl) {
      return lastTarget;
    }
    await delay(250);
  }

  throw new Error(
    `Chromium page target did not finish initializing: ${JSON.stringify(lastTarget)}`,
  );
};

const connectToPage = async () => {
  const page = await createPageTarget();
  if (!page.webSocketDebuggerUrl) {
    throw new Error("Chromium did not return a debuggable page target");
  }

  const socket = new WebSocket(page.webSocketDebuggerUrl);
  await Promise.race([
    new Promise((resolve, reject) => {
      socket.addEventListener("open", resolve, { once: true });
      socket.addEventListener("error", reject, { once: true });
    }),
    delay(protocolTimeoutMilliseconds).then(() => {
      throw new Error("Timed out opening the Chromium DevTools WebSocket");
    }),
  ]);

  let nextId = 1;
  const pending = new Map();
  const listeners = new Set();

  socket.addEventListener("message", async ({ data }) => {
    try {
      const payload =
        typeof data === "string"
          ? data
          : typeof data?.text === "function"
            ? await data.text()
            : new TextDecoder().decode(data);
      const message = JSON.parse(payload);
      if (message.id) {
        const request = pending.get(message.id);
        if (!request) return;
        pending.delete(message.id);
        if (message.error) {
          request.reject(
            new Error(
              `Chromium command ${request.method} failed: ${message.error.message}`,
            ),
          );
        } else request.resolve(message.result);
        return;
      }

      for (const listener of listeners) listener(message);
    } catch (error) {
      for (const request of pending.values()) request.reject(error);
      pending.clear();
    }
  });

  const command = (method, params = {}) => {
    const id = nextId++;
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        pending.delete(id);
        reject(new Error(`Timed out waiting for Chromium command ${method}`));
      }, protocolTimeoutMilliseconds);

      pending.set(id, {
        method,
        resolve: (value) => {
          clearTimeout(timeout);
          resolve(value);
        },
        reject: (error) => {
          clearTimeout(timeout);
          reject(error);
        },
      });
      socket.send(JSON.stringify({ id, method, params }));
    });
  };

  return { command, listeners, socket };
};

const runBrowser = async () => {
  const profileDirectory = mkdtempSync(
    path.join(tmpdir(), "xframes-wasm-smoke-"),
  );
  let browserDiagnostics = "";
  const browser = spawn(
    browserPath,
    [
      "--headless=new",
      "--enable-logging=stderr",
      ...webGpuFlags,
      `--remote-debugging-port=${debuggingPort}`,
      `--user-data-dir=${profileDirectory}`,
      "--window-size=900,700",
      "--no-first-run",
      "about:blank",
    ],
    { stdio: ["ignore", "ignore", "pipe"], windowsHide: true },
  );
  browser.stderr.on("data", (chunk) => {
    browserDiagnostics = `${browserDiagnostics}${chunk}`.slice(-20_000);
  });
  const browserExit = new Promise((resolve) => browser.once("exit", resolve));

  try {
    const { command, listeners, socket } = await connectToPage();
    const runtimeErrors = [];
    let markReady;
    const ready = new Promise((resolve) => {
      markReady = resolve;
    });

    listeners.add((message) => {
      if (message.method === "Runtime.exceptionThrown") {
        const details = message.params.exceptionDetails;
        runtimeErrors.push(details.exception?.description ?? details.text);
      }
      if (
        message.method === "Log.entryAdded" &&
        message.params.entry.level === "error"
      ) {
        const entry = message.params.entry;
        const missingFavicon =
          entry.url === `${serverUrl}/favicon.ico` &&
          entry.text.includes("404");
        if (!missingFavicon) {
          const location = entry.url
            ? ` (${entry.url}${entry.lineNumber ? `:${entry.lineNumber}` : ""})`
            : "";
          runtimeErrors.push(`${entry.text}${location}`);
        }
      }
      if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args
          .map((argument) => argument.value ?? argument.description ?? "")
          .join(" ");
        console.log(`[browser:${message.params.type}] ${text}`);
        if (message.params.type === "error") runtimeErrors.push(text);
        if (text === "ready") markReady();
      }
    });

    await Promise.all([
      command("Page.enable"),
      command("Runtime.enable"),
      command("Log.enable"),
    ]);
    const navigation = await command("Page.navigate", { url: serverUrl });
    if (navigation.errorText) {
      throw new Error(`Browser navigation failed: ${navigation.errorText}`);
    }

    await Promise.race([
      ready,
      delay(60_000).then(() => {
        throw new Error(
          "Timed out waiting for the XFrames WASM onInit callback",
        );
      }),
    ]);
    await delay(3_000);

    if (runtimeErrors.length > 0) {
      throw new Error(`Browser runtime errors:\n${runtimeErrors.join("\n")}`);
    }

    mkdirSync(path.dirname(outputPath), { recursive: true });
    const screenshot = await command("Page.captureScreenshot", {
      format: "png",
    });
    writeFileSync(outputPath, Buffer.from(screenshot.data, "base64"));
    await command("Browser.close");
    socket.close();
    console.log(`WASM browser smoke screenshot written to ${outputPath}`);
  } catch (error) {
    if (browserDiagnostics.trim()) {
      throw new Error(
        `${error.message}\nChromium stderr (last 20 KB):\n${browserDiagnostics.trim()}`,
        { cause: error },
      );
    }
    throw error;
  } finally {
    if (browser.exitCode === null && !browser.killed) browser.kill();
    await Promise.race([browserExit, delay(5_000)]);
    try {
      rmSync(profileDirectory, {
        recursive: true,
        force: true,
        // Chromium child processes can retain profile files briefly after the
        // DevTools Browser.close acknowledgement on Windows.
        maxRetries: 60,
        retryDelay: 250,
      });
    } catch (error) {
      console.warn(
        `Unable to remove temporary browser profile: ${error.message}`,
      );
    }
  }
};

let server;
try {
  try {
    await fetch(serverUrl);
  } catch {
    server = spawn(
      process.execPath,
      [webpackCli, "serve", "--config", "webpack.config.cjs", "--no-open"],
      { cwd: packageRoot, stdio: "inherit", windowsHide: true },
    );
    await waitForHttp(serverUrl, 60_000);
  }

  await runBrowser();
} finally {
  if (server && !server.killed) server.kill();
}

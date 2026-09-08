import { spawn, spawnSync } from "node:child_process";
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
import { installListenerAudit } from "../../diagnostics/browser-listeners.mjs";

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));
const packageRoot = path.resolve(scriptDirectory, "..");
const localRequire = createRequire(import.meta.url);
const webpackCli = localRequire.resolve("webpack-cli/bin/cli.js");
const diagnostics = process.env.XFRAMES_DIAGNOSTICS === "1";
const serverUrl = diagnostics ? "http://127.0.0.1:3011" : "http://127.0.0.1:3000";
const debuggingPort = diagnostics ? 9344 : 9333;
const protocolTimeoutMilliseconds = 10_000;
const outputPath = diagnostics
  ? path.resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? path.join(packageRoot, "../build/diagnostics/wasm"), "fixture.png")
  : path.join(packageRoot, "build", "browser-smoke.png");
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
  "/usr/bin/google-chrome",
  "/usr/bin/chromium",
  "/usr/bin/chromium-browser",
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
const withTimeout = async (promise, milliseconds, message) => {
  let timer;
  try {
    return await Promise.race([promise, new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(message)), milliseconds);
    })]);
  } finally { clearTimeout(timer); }
};

const waitForJson = async (url, timeoutMilliseconds) => {
  const deadline = Date.now() + timeoutMilliseconds;
  let lastError;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url, { signal: AbortSignal.timeout(2000) });
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
      const response = await fetch(url, { signal: AbortSignal.timeout(2000) });
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
    { method: "PUT", signal: AbortSignal.timeout(protocolTimeoutMilliseconds) },
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
  await withTimeout(
    new Promise((resolve, reject) => {
      socket.addEventListener("open", resolve, { once: true });
      socket.addEventListener("error", reject, { once: true });
    }), protocolTimeoutMilliseconds, "Timed out opening the Chromium DevTools WebSocket");

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
      }, method === "Page.navigate" ? 60_000 : protocolTimeoutMilliseconds);

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

const dispatchFixtureInput = async (command, request) => {
  if (request.action === "minimize" || request.action === "restore") {
    const { windowId } = await command("Browser.getWindowForTarget");
    await command("Browser.setWindowBounds", { windowId, bounds: { windowState: request.action === "minimize" ? "minimized" : "normal" } });
  } else if (request.action === "text") {
    for (const character of request.value)
      await command("Input.dispatchKeyEvent", { type: "char", text: character, unmodifiedText: character });
  } else if (request.action === "keyDown" || request.action === "keyUp") {
    await command("Input.dispatchKeyEvent", { type: request.action === "keyDown" ? "keyDown" : "keyUp",
      key: "Backspace", code: "Backspace", windowsVirtualKeyCode: 8, nativeVirtualKeyCode: 8 });
  } else if (["move", "click", "wheel"].includes(request.action)) {
    const position = { x: request.x ?? 50, y: request.y ?? 24 };
    await command("Input.dispatchMouseEvent", { type: "mouseMoved", ...position });
    if (request.action === "click") {
      await command("Input.dispatchMouseEvent", { type: "mousePressed", ...position, button: "left", buttons: 1, clickCount: 1 });
      await command("Input.dispatchMouseEvent", { type: "mouseReleased", ...position, button: "left", buttons: 0, clickCount: 1 });
    } else if (request.action === "wheel") {
      await command("Input.dispatchMouseEvent", { type: "mouseWheel", ...position, deltaX: 0, deltaY: -120 });
    }
  } else throw new Error(`Unsupported fixture input action: ${request.action}`);
};

const runBrowser = async () => {
  const profileDirectory = mkdtempSync(
    path.join(tmpdir(), "xframes-wasm-smoke-"),
  );
  let browserDiagnostics = "";
  let pageDiagnostics = "";
  let connection;
  mkdirSync(path.dirname(outputPath), { recursive: true });
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
    connection = await connectToPage();
    const { command, listeners, socket } = connection;
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
        const fixtureBase = JSON.parse(process.env.XFRAMES_DIAGNOSTICS_OPTIONS ?? "{}").resourceFixture?.baseUrl;
        const failureAsset = diagnostics && fixtureBase && entry.url?.startsWith(`${fixtureBase}/asset?`)
          && new URL(entry.url).searchParams.get("kind") === "failure" && entry.text.includes("404");
        // These exact controlled 404s are asserted as completed resource failures
        // by the fixture. All other browser/network errors still fail the smoke.
        if (!missingFavicon && !failureAsset) {
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
        pageDiagnostics = `${pageDiagnostics}[${message.params.type}] ${text}\n`.slice(-100_000);
        if (message.params.type === "error" || text.includes("[imgui-error]")) runtimeErrors.push(text);
        if (text === "ready") markReady();
      }
    });

    await Promise.all([
      command("Page.enable"),
      command("Runtime.enable"),
      command("Log.enable"),
    ]);
    if (diagnostics) {
      await command("Emulation.setDeviceMetricsOverride", { width: 900, height: 700, deviceScaleFactor: 1, mobile: false });
      await command("Page.addScriptToEvaluateOnNewDocument", { source: `(${installListenerAudit.toString()})()` });
    }
    const navigation = await command("Page.navigate", { url: serverUrl });
    if (navigation.errorText) {
      throw new Error(`Browser navigation failed: ${navigation.errorText}`);
    }

    await withTimeout(ready, 60_000, "Timed out waiting for the XFrames WASM onInit callback");
    mkdirSync(path.dirname(outputPath), { recursive: true });
    if (diagnostics) {
      const options = JSON.parse(process.env.XFRAMES_DIAGNOSTICS_OPTIONS ?? "{}");
      const deadline = Date.now() + Math.max(60_000, (options.cycles ?? 3) * 2000 + (options.repetitions ?? 1) * 90_000);
      let complete = false;
      while (Date.now() < deadline) {
        const response = await command("Runtime.evaluate", { expression: "globalThis.__xframesDiagnostics", returnByValue: true });
        const state = response.result?.value;
        if (state) writeFileSync(path.join(path.dirname(outputPath), "result.json"), JSON.stringify(state, null, 2));
        if (state?.captureRequested && !state.captureDone) {
          const screenshot = await command("Page.captureScreenshot", { format: "png" });
          writeFileSync(outputPath, Buffer.from(screenshot.data, "base64"));
          await command("Runtime.evaluate", { expression: "globalThis.__xframesDiagnostics.captureDone = true" });
        }
        if (state?.inputRequest && state.inputDone < state.inputRequest.id) {
          await dispatchFixtureInput(command, state.inputRequest);
          await command("Runtime.evaluate", { expression: `globalThis.__xframesDiagnostics.inputDone = ${state.inputRequest.id}` });
        }
        if (state?.status === "failed") throw new Error(`Wasm diagnostics failed: ${state.error ?? state.report?.error}`);
        if (runtimeErrors.length) throw new Error(`Browser runtime errors:\n${runtimeErrors.join("\n")}`);
        if (state?.status === "complete") { complete = true; break; }
        await delay(100);
      }
      if (!complete) throw new Error("Wasm diagnostics timed out; see last saved result.json");
    } else {
      await delay(3_000);
    }

    if (runtimeErrors.length > 0) {
      throw new Error(`Browser runtime errors:\n${runtimeErrors.join("\n")}`);
    }

    mkdirSync(path.dirname(outputPath), { recursive: true });
    if (!diagnostics) {
      const screenshot = await command("Page.captureScreenshot", { format: "png" });
      writeFileSync(outputPath, Buffer.from(screenshot.data, "base64"));
    }
    await command("Browser.close");
    socket.close();
    console.log(`WASM browser smoke screenshot written to ${outputPath}`);
  } catch (error) {
    writeFileSync(path.join(path.dirname(outputPath), "failure.json"), JSON.stringify({ error: String(error) }, null, 2));
    if (browserDiagnostics.trim()) {
      throw new Error(
        `${error.message}\nChromium stderr (last 20 KB):\n${browserDiagnostics.trim()}`,
        { cause: error },
      );
    }
    throw error;
  } finally {
    writeFileSync(path.join(path.dirname(outputPath), "browser.log"), browserDiagnostics);
    writeFileSync(path.join(path.dirname(outputPath), "page.log"), pageDiagnostics);
    if (browser.exitCode === null && connection?.socket.readyState === WebSocket.OPEN) {
      try { await connection.command("Browser.close"); }
      catch (error) { console.warn(`Browser close failed; terminating owned process: ${error.message}`); }
    }
    connection?.socket.close();
    if (browser.exitCode === null && !browser.killed) {
      if (process.platform === "win32") spawnSync("taskkill", ["/PID", String(browser.pid), "/T", "/F"], { windowsHide: true, stdio: "ignore" });
      else browser.kill();
    }
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
let serverLog = "";
try {
  let existing = false;
  try {
    await fetch(serverUrl, { signal: AbortSignal.timeout(2000) });
    existing = true;
  } catch {}
  if (diagnostics && existing) throw new Error(`Diagnostics require their own fresh bundle; port 3011 is already in use (${serverUrl})`);
  if (!existing) {
    server = spawn(
      process.execPath,
      [webpackCli, "serve", "--config", "webpack.config.cjs", "--no-open"],
      { cwd: packageRoot, stdio: ["ignore", "pipe", "pipe"], windowsHide: true },
    );
    for (const pipe of [server.stdout, server.stderr]) pipe.on("data", chunk => {
      serverLog = `${serverLog}${chunk}`.slice(-100_000);
      process.stdout.write(chunk);
    });
    await waitForHttp(serverUrl, 60_000);
  }

  await runBrowser();
} finally {
  if (server && !server.killed) server.kill();
  mkdirSync(path.dirname(outputPath), { recursive: true });
  writeFileSync(path.join(path.dirname(outputPath), "webpack.log"), serverLog);
}

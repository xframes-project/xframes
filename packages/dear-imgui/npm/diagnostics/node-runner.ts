import { mkdirSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { cpus, platform, release } from "node:os";
import { execFileSync } from "node:child_process";
import { defaults, runRuntime, validateOptions, type RunOptions } from "./runtime";
import { check } from "./assertions";

const native = require("../node/build/Release/xframes.node");
const options: RunOptions = { ...defaults, ...JSON.parse(process.env.XFRAMES_DIAGNOSTICS_OPTIONS ?? "{}") };
const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? "./build/diagnostics/node");
mkdirSync(output, { recursive: true });
const write = (value: unknown) => writeFileSync(resolve(output, "result.json"), JSON.stringify(value, null, 2));
const processStartMs = performance.now();
const callbacks = ["onTextChange", "onComboChange", "onNumericValueChange", "onBooleanValueChange", "onMultiValueChange",
    "onClick", "onTableSort", "onTableFilter", "onTableRowClick", "onTableItemAction", "onPrefetchProgress", "onScriptError"];
const timeout = setTimeout(() => { console.error("Native diagnostics process watchdog expired"); process.exit(1); },
    Math.max(60_000, options.cycles * 1500 + options.repetitions * options.rates.length * (options.durationMs + options.warmupMs + 10_000)));

async function main() {
    validateOptions(options);
    check(typeof native.getDiagnostics === "function", "Rebuild the Node addon: diagnostics exports are absent");
    await new Promise<void>((resolveReady, reject) => {
        native.init({ assetsBasePath: resolve("../assets"), fontDefs: JSON.stringify({ defs: [{ name: "roboto-regular", size: 16 }] }), theme: "{}",
            ...Object.fromEntries(callbacks.map(name => [name, name === "onScriptError" ? (...args: unknown[]) => reject(new Error(JSON.stringify(args))) : () => {}])),
            onInit: resolveReady, onBeforeExit: () => { console.error("Window closed before diagnostics completed"); process.exit(1); } });
    });
    const readyMs = performance.now() - processStartMs;
    const sourceRevision = execFileSync("git", ["rev-parse", "HEAD"], { encoding: "utf8" }).trim();
    const report = await runRuntime(native, options, {
        capture: () => new Promise<void>((resolveCapture, reject) => native.captureScreenshot(resolve(output, "fixture.png"),
            (error: string | null) => error ? reject(new Error(error)) : resolveCapture())),
        report: write,
        resources: () => ({ sampledAtMs: performance.now(), rssBytes: process.memoryUsage().rss, cpuMicroseconds: process.cpuUsage() }),
        metadata: { runtime: "Node/OpenGL", sourceRevision, sourceDirty: execFileSync("git", ["status", "--porcelain"], { encoding: "utf8" }).trim().length > 0,
            node: process.version, os: `${platform()} ${release()}`, cpu: cpus()[0]?.model,
            renderer: process.env.NODE_ENV ?? "development", nativeBuild: "Release", toolchain: platform() === "win32" ? "Visual Studio 2022" : process.env.XFRAMES_TOOLCHAIN ?? "See native CMake build log",
            gpu: "Actual GL vendor/renderer/version in each native frame's backend field", readyMs, startupClock: "process performance.now at runner entry to native onInit; excludes module loading",
            assets: "repository roboto-regular.ttf, size 16", display: "900x700 initial GLFW window" },
    });
    console.log(`Node diagnostics: ${report.status}; ${report.streams.length} streaming runs; ${options.cycles} stress cycles`);
}

main().then(() => { clearTimeout(timeout); process.exit(0); }).catch(error => {
    console.error(error);
    clearTimeout(timeout);
    process.exit(1);
});

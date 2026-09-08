import { spawn } from "node:child_process";
import { createRequire } from "node:module";
import { fileURLToPath, pathToFileURL } from "node:url";
import { dirname, resolve } from "node:path";
import { mkdirSync, writeFileSync, readFileSync } from "node:fs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const commonRequire = createRequire(resolve(root, "common/package.json"));
const tsx = pathToFileURL(commonRequire.resolve("tsx")).href;
const command = process.argv[2] ?? "bridge";
const extra = process.argv.slice(3);
const options = JSON.parse(process.env.XFRAMES_DIAGNOSTICS_OPTIONS ?? "{}");
const output = resolve(process.env.XFRAMES_DIAGNOSTICS_DIR ?? resolve(root, "build/diagnostics", command));
mkdirSync(output, { recursive: true });
if (extra.includes("--stress")) options.cycles = 1000;
if (extra.includes("--baseline")) options.repetitions = 3;
if (extra.includes("--extended")) options.rows = 100000;
if (extra.includes("--baseline") && command === "wasm" && !/XFRAMES_FAST_BUILD:BOOL=OFF/.test(readFileSync(resolve(root, "../cpp/wasm/build-wasm/CMakeCache.txt"), "utf8")))
    throw new Error("Baseline requires the optimized Docker Wasm build (omit --fast)");
for (const flag of extra) if (!["--stress", "--baseline", "--extended"].includes(flag)) throw new Error(`Unknown option: ${flag}`);

const invoke = (entry, mode) => new Promise((resolveRun, reject) => {
    let log = "";
    // Preload the TS loader directly: the watched process owns the native runtime,
    // so terminating it cannot leave a tsx CLI child running after a timeout.
    const child = spawn(process.execPath, ["--import", tsx, resolve(root, entry)], {
        cwd: root, stdio: ["ignore", "pipe", "pipe"], windowsHide: true,
        env: { ...process.env, NODE_ENV: mode, TSX_TSCONFIG_PATH: resolve(root, "diagnostics/tsconfig.json"),
            XFRAMES_DIAGNOSTICS_OPTIONS: JSON.stringify(options) },
    });
    for (const pipe of [child.stdout, child.stderr]) pipe.on("data", chunk => {
        log = `${log}${chunk}`.slice(-100_000);
        process.stdout.write(chunk);
        writeFileSync(resolve(output, `${mode}.log`), log);
    });
    const timeoutMs = command === "bridge" ? 60_000 : Math.max(120_000, (options.cycles ?? 3) * 2000 + (options.repetitions ?? 1) * 90_000);
    const timeout = setTimeout(() => { child.kill("SIGKILL"); reject(new Error(`${entry} exceeded ${timeoutMs} ms`)); }, timeoutMs);
    child.once("error", error => { clearTimeout(timeout); reject(error); });
    child.once("exit", (code, signal) => {
        clearTimeout(timeout);
        writeFileSync(resolve(output, `${mode}-process.json`), JSON.stringify({ entry, code, signal }));
        if (log.includes("[imgui-error]")) reject(new Error(`${entry} reported an ImGui error; see ${mode}.log`));
        else code === 0 ? resolveRun() : reject(new Error(`${entry} exited ${code ?? signal}`));
    });
});

const resourceServer = ["node", "wasm"].includes(command)
    ? await (await import("./resource-server.mjs")).startResourceServer(root, output) : undefined;
if (resourceServer) options.resourceFixture = { baseUrl: resourceServer.baseUrl, controlUrl: resourceServer.controlUrl, assets: resourceServer.assets };
try {
if (command === "bridge") {
    for (const mode of ["development", "production"]) await invoke("diagnostics/bridge-lifecycle.tsx", mode);
} else if (command === "node") {
    await invoke("diagnostics/node-runner.ts", extra.includes("--baseline") ? "production" : process.env.NODE_ENV ?? "development");
} else if (command === "wasm") {
    process.env.XFRAMES_DIAGNOSTICS = "1";
    process.env.XFRAMES_DIAGNOSTICS_OPTIONS = JSON.stringify(options);
    if (extra.includes("--baseline")) process.env.NODE_ENV = "production";
    await import("../wasm/scripts/browser-smoke.mjs");
} else {
    throw new Error(`Unknown diagnostics command: ${command}`);
}
} finally { await resourceServer?.close(); }

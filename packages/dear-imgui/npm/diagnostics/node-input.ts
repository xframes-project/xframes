import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { resolve } from "node:path";
import type { FixtureInput } from "./input";

const execute = promisify(execFile);
export async function nativeInput(request: FixtureInput) {
    if (process.platform === "win32") {
        await execute("powershell.exe", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", resolve("diagnostics/native-window.ps1"),
            "-ProcessId", String(process.pid), "-Action", request.action, "-X", String(request.x ?? 50), "-Y", String(request.y ?? 24),
            ...(request.value === undefined ? [] : ["-Value", request.value])], { windowsHide: true, timeout: 10_000 });
        return;
    }
    if (process.platform !== "linux") throw new Error(`Native input fixture does not support ${process.platform}`);
    const { stdout } = await execute("xdotool", ["search", ...(request.action === "restore" ? [] : ["--onlyvisible"]),
        "--pid", String(process.pid)], { timeout: 5000 });
    const windowId = stdout.trim().split(/\s+/)[0];
    if (!/^\d+$/.test(windowId)) throw new Error("Fixture process has no X11 window");
    if (request.action === "close") {
        // Ask the window manager to send WM_DELETE_WINDOW; xdotool windowclose
        // destroys the X11 window directly and bypasses GLFW's close callback.
        await execute("wmctrl", ["-ic", windowId], { timeout: 5000 });
        return;
    }
    if (["minimize", "restore", "refresh"].includes(request.action)) {
        if (request.action === "refresh") {
            await execute("xdotool", ["windowunmap", "--sync", windowId, "windowmap", "--sync", windowId], { timeout: 5000 });
        } else await execute("xdotool", [request.action === "minimize" ? "windowminimize" : "windowactivate", "--sync", windowId], { timeout: 5000 });
        return;
    }
    await execute("xdotool", ["windowfocus", "--sync", windowId], { timeout: 5000 });
    const args = request.action === "text" ? ["type", "--clearmodifiers", "--delay", "0", request.value!]
        : request.action === "keyDown" ? ["keydown", "BackSpace"]
        : request.action === "keyUp" ? ["keyup", "BackSpace"]
        : ["mousemove", "--sync", "--window", windowId, String(request.x ?? 50), String(request.y ?? 24),
            ...(request.action === "click" ? ["click", "1"] : request.action === "wheel" ? ["click", "4"] : [])];
    await execute("xdotool", args, { timeout: 5000 });
}

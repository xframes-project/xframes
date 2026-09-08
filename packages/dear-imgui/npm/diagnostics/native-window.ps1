param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [Parameter(Mandatory=$true)][ValidateSet('move','click','text','keyDown','keyUp','wheel','close','minimize','restore','refresh')][string]$Action,
    [int]$X = 50,
    [int]$Y = 24,
    [string]$Value = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class XFramesFixtureWindow {
    public delegate bool EnumWindow(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindow callback, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr window, IntPtr rect, IntPtr region, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", SetLastError=true)] public static extern bool AttachThreadInput(uint first, uint second, bool attach);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint MapVirtualKeyW(uint key, uint mapType);
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
    public static IntPtr Find(uint process) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == process && IsWindowVisible(window)) { result = window; return false; }
            return true;
        }, IntPtr.Zero);
        if (result == IntPtr.Zero) throw new Exception("Fixture process has no visible native window");
        return result;
    }
    public static void Run(uint process, string action, int x, int y, string value) {
        // Match GLFW's physical client coordinates when the host uses display scaling.
        SetThreadDpiAwarenessContext(new IntPtr(-4));
        IntPtr window = Find(process);
        if (action == "close") { SendMessageW(window, 0x0010, IntPtr.Zero, IntPtr.Zero); return; }
        if (action == "minimize" || action == "restore") {
            ShowWindow(window, action == "minimize" ? 6 : 9);
            SendMessageW(window, 0, IntPtr.Zero, IntPtr.Zero);
            return;
        }
        if (action == "refresh") { RedrawWindow(window, IntPtr.Zero, IntPtr.Zero, 0x185); return; }
        SetForegroundWindow(window);
        // Cross-thread activation is delivered to the target's event queue.
        SendMessageW(window, 0, IntPtr.Zero, IntPtr.Zero);
        if (GetForegroundWindow() != window) {
            // The hidden helper can be outside the foreground input queue. Join
            // it only for the focus transition, then immediately detach.
            uint ignored;
            uint foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), out ignored);
            uint targetThread = GetWindowThreadProcessId(window, out ignored);
            uint helperThread = GetCurrentThreadId();
            bool attached = foregroundThread != 0 && foregroundThread != helperThread
                && AttachThreadInput(helperThread, foregroundThread, true);
            bool targetAttached = targetThread != helperThread && targetThread != foregroundThread
                && AttachThreadInput(helperThread, targetThread, true);
            try {
                BringWindowToTop(window);
                SetForegroundWindow(window);
                SetFocus(window);
                SendMessageW(window, 0, IntPtr.Zero, IntPtr.Zero);
            } finally {
                if (targetAttached) AttachThreadInput(helperThread, targetThread, false);
                if (attached) AttachThreadInput(helperThread, foregroundThread, false);
            }
            if (GetForegroundWindow() != window) throw new Exception(String.Format(
                "Fixture window could not acquire foreground focus (target={0}, foreground={1}, foregroundThread={2}, attached={3}, error={4})",
                window, GetForegroundWindow(), foregroundThread, attached, Marshal.GetLastWin32Error()));
        }
        var client = new IntPtr((y << 16) | (x & 0xffff));
        if (action == "move" || action == "click" || action == "wheel") {
            Point point = new Point { x = x, y = y };
            if (!ClientToScreen(window, ref point) || !SetCursorPos(point.x, point.y))
                throw new Exception("Could not position the fixture cursor");
            SendMessageW(window, 0x0200, IntPtr.Zero, client);
            if (action == "click") {
                SendMessageW(window, 0x0201, new IntPtr(1), client);
                SendMessageW(window, 0x0202, IntPtr.Zero, client);
            } else if (action == "wheel") {
                SendMessageW(window, 0x020a, new IntPtr(120 << 16), new IntPtr((point.y << 16) | (point.x & 0xffff)));
            }
        } else if (action == "text") {
            foreach (char character in value) SendMessageW(window, 0x0102, new IntPtr(character), new IntPtr(1));
        } else {
            // One key transition, with repeat generated by ImGui's own deadline.
            const uint backspace = 8;
            long flags = 1 | ((long)MapVirtualKeyW(backspace, 0) << 16);
            if (action == "keyUp") flags |= 0xc0000000L;
            SendMessageW(window, action == "keyUp" ? 0x0101u : 0x0100u, new IntPtr(backspace), new IntPtr(flags));
        }
    }
}
'@
[XFramesFixtureWindow]::Run($ProcessId, $Action, $X, $Y, $Value)

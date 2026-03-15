#pragma once

#include <string>

// Lua Canvas 2D API shim — evaluated once in LuaCanvas::InitLua() after registerDrawBindings().
// Creates a global `ctx` table with an HTML5 Canvas 2D-style API that delegates to drawXxx bindings.
// Raw drawXxx functions remain available alongside ctx.
//
// Uses runtime std::string concatenation because MSVC limits individual string literals
// to 16380 chars and applies this limit even after adjacent-literal concatenation.

inline const std::string& getLuaCanvas2DShim() {
    static const std::string s =

    // --- Part 1: helpers, state, dashed-line engine ---
    std::string(R"LUA(
local function parseColor(c)
    if type(c) ~= "string" or #c == 0 then return "rgba(0,0,0,1)" end
    return c
end

local function applyAlpha(color, alpha)
    if alpha >= 1.0 then return color end
    if alpha <= 0.0 then return "rgba(0,0,0,0)" end
    return color
end

local function defaultState()
    return {
        fillStyle = "#000000",
        strokeStyle = "#000000",
        lineWidth = 1.0,
        globalAlpha = 1.0,
        font = "10px sans-serif",
        _fontSize = 10,
        textAlign = "start",
        textBaseline = "alphabetic",
        _matrix = {1, 0, 0, 1, 0, 0},
        _lineDash = {},
        _lineDashOffset = 0,
        _clipCount = 0
    }
end

local stateStack = {}
local state = defaultState()
local subPaths = {}
local currentSubPath = nil

local function _tx(x, y)
    local m = state._matrix
    return m[1]*x + m[3]*y + m[5], m[2]*x + m[4]*y + m[6]
end

local function isIdentityOrTranslation()
    local m = state._matrix
    return m[1] == 1 and m[2] == 0 and m[3] == 0 and m[4] == 1
end

local function multiplyMatrix(a, b)
    return {
        a[1]*b[1] + a[3]*b[2],
        a[2]*b[1] + a[4]*b[2],
        a[1]*b[3] + a[3]*b[4],
        a[2]*b[3] + a[4]*b[4],
        a[1]*b[5] + a[3]*b[6] + a[5],
        a[2]*b[5] + a[4]*b[6] + a[6]
    }
end

local function copyArray(t)
    local c = {}
    for i = 1, #t do c[i] = t[i] end
    return c
end

local function distPt(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return math.sqrt(dx*dx + dy*dy)
end

local function strokeDashedPolyline(points, closed, color, thickness, dashPattern, dashOffset)
    if #dashPattern == 0 then
        local flat = {}
        for i = 1, #points, 2 do
            flat[#flat+1] = points[i]
            flat[#flat+1] = points[i+1]
        end
        drawPolyline(flat, color, closed, thickness)
        return
    end
    local pattern = copyArray(dashPattern)
    if #pattern % 2 ~= 0 then
        local n = #pattern
        for i = 1, n do pattern[#pattern+1] = pattern[i] end
    end
    local edges = {}
    for i = 1, #points - 2, 2 do
        edges[#edges+1] = {points[i], points[i+1], points[i+2], points[i+3]}
    end
    if closed and #points >= 4 then
        edges[#edges+1] = {points[#points-1], points[#points], points[1], points[2]}
    end
    local patIdx = 1
    local patRemain = pattern[1]
    local drawing = true
    local currentDash = {}
    local total = 0
    for i = 1, #pattern do total = total + pattern[i] end
    local offset = dashOffset % total
    if offset < 0 then offset = offset + total end
    while offset > 0 do
        if offset >= patRemain then
            offset = offset - patRemain
            drawing = not drawing
            patIdx = (patIdx % #pattern) + 1
            patRemain = pattern[patIdx]
        else
            patRemain = patRemain - offset
            offset = 0
        end
    end
    for e = 1, #edges do
        local edge = edges[e]
        local fx, fy, tx, ty = edge[1], edge[2], edge[3], edge[4]
        local edgeLen = distPt(fx, fy, tx, ty)
        if edgeLen > 0 then
            local dx = (tx - fx) / edgeLen
            local dy = (ty - fy) / edgeLen
            local pos = 0
            while pos < edgeLen do
                local step = math.min(patRemain, edgeLen - pos)
                local px = fx + dx * (pos + step)
                local py = fy + dy * (pos + step)
                if drawing then
                    if #currentDash == 0 then
                        currentDash[1] = fx + dx * pos
                        currentDash[2] = fy + dy * pos
                    end
                    currentDash[#currentDash+1] = px
                    currentDash[#currentDash+1] = py
                end
                pos = pos + step
                patRemain = patRemain - step
                if patRemain <= 0.0001 then
                    if drawing and #currentDash >= 4 then
                        drawPolyline(currentDash, color, false, thickness)
                    end
                    currentDash = {}
                    drawing = not drawing
                    patIdx = (patIdx % #pattern) + 1
                    patRemain = pattern[patIdx]
                end
            end
        end
    end
    if drawing and #currentDash >= 4 then
        drawPolyline(currentDash, color, false, thickness)
    end
end
)LUA")

    // --- Part 2: ctx object — properties via metatable, save/restore, basic drawing ---
    + R"LUA(
local ctxData = {}
local ctxMethods = {}

local validTextAligns = {left=true, right=true, center=true, start=true, ["end"]=true}
local validTextBaselines = {top=true, middle=true, bottom=true, alphabetic=true, hanging=true, ideographic=true}

local ctxMeta = {
    __index = function(t, k)
        if k == "canvas" then
            return {width = canvasWidth or 0, height = canvasHeight or 0}
        end
        if ctxMethods[k] then return ctxMethods[k] end
        if k == "fillStyle" then return state.fillStyle end
        if k == "strokeStyle" then return state.strokeStyle end
        if k == "lineWidth" then return state.lineWidth end
        if k == "globalAlpha" then return state.globalAlpha end
        if k == "font" then return state.font end
        if k == "textAlign" then return state.textAlign end
        if k == "textBaseline" then return state.textBaseline end
        if k == "lineDashOffset" then return state._lineDashOffset end
        return nil
    end,
    __newindex = function(t, k, v)
        if k == "fillStyle" then state.fillStyle = parseColor(v)
        elseif k == "strokeStyle" then state.strokeStyle = parseColor(v)
        elseif k == "lineWidth" then if v > 0 then state.lineWidth = v end
        elseif k == "globalAlpha" then if v >= 0 and v <= 1 then state.globalAlpha = v end
        elseif k == "font" then
            state.font = v
            local sz = string.match(v, "(%d+%.?%d*)%s*px")
            if sz then state._fontSize = tonumber(sz) end
        elseif k == "textAlign" then
            if validTextAligns[v] then state.textAlign = v end
        elseif k == "textBaseline" then
            if validTextBaselines[v] then state.textBaseline = v end
        elseif k == "lineDashOffset" then state._lineDashOffset = v
        else rawset(t, k, v)
        end
    end
}

setmetatable(ctxData, ctxMeta)

function ctxMethods.save()
    stateStack[#stateStack+1] = {
        fillStyle = state.fillStyle, strokeStyle = state.strokeStyle,
        lineWidth = state.lineWidth, globalAlpha = state.globalAlpha,
        font = state.font, _fontSize = state._fontSize,
        textAlign = state.textAlign, textBaseline = state.textBaseline,
        _matrix = copyArray(state._matrix),
        _lineDash = copyArray(state._lineDash), _lineDashOffset = state._lineDashOffset,
        _clipCount = state._clipCount
    }
    state._clipCount = 0
end

function ctxMethods.restore()
    for i = 1, state._clipCount do popClipRect() end
    if #stateStack > 0 then
        local prev = stateStack[#stateStack]
        stateStack[#stateStack] = nil
        state.fillStyle = prev.fillStyle; state.strokeStyle = prev.strokeStyle
        state.lineWidth = prev.lineWidth; state.globalAlpha = prev.globalAlpha
        state.font = prev.font; state._fontSize = prev._fontSize
        state.textAlign = prev.textAlign; state.textBaseline = prev.textBaseline
        state._matrix = prev._matrix
        state._lineDash = prev._lineDash; state._lineDashOffset = prev._lineDashOffset
        state._clipCount = prev._clipCount
    end
end

function ctxMethods.clearRect(x, y, w, h)
    if isIdentityOrTranslation() then
        local px, py = _tx(x, y)
        drawRectFilled(px, py, w, h, "#000000")
    else
        local p1x,p1y = _tx(x,y)
        local p2x,p2y = _tx(x+w,y)
        local p3x,p3y = _tx(x+w,y+h)
        local p4x,p4y = _tx(x,y+h)
        drawConvexPolyFilled({p1x,p1y,p2x,p2y,p3x,p3y,p4x,p4y}, "#000000")
    end
end

function ctxMethods.fillRect(x, y, w, h)
    local c = applyAlpha(state.fillStyle, state.globalAlpha)
    if isIdentityOrTranslation() then
        local px, py = _tx(x, y)
        drawRectFilled(px, py, w, h, c)
    else
        local p1x,p1y = _tx(x,y)
        local p2x,p2y = _tx(x+w,y)
        local p3x,p3y = _tx(x+w,y+h)
        local p4x,p4y = _tx(x,y+h)
        drawConvexPolyFilled({p1x,p1y,p2x,p2y,p3x,p3y,p4x,p4y}, c)
    end
end

function ctxMethods.strokeRect(x, y, w, h)
    local c = applyAlpha(state.strokeStyle, state.globalAlpha)
    if isIdentityOrTranslation() then
        local px, py = _tx(x, y)
        drawRect(px, py, w, h, c, state.lineWidth)
    else
        local p1x,p1y = _tx(x,y)
        local p2x,p2y = _tx(x+w,y)
        local p3x,p3y = _tx(x+w,y+h)
        local p4x,p4y = _tx(x,y+h)
        local flat = {p1x,p1y,p2x,p2y,p3x,p3y,p4x,p4y}
        if #state._lineDash > 0 then
            strokeDashedPolyline(flat, true, c, state.lineWidth, state._lineDash, state._lineDashOffset)
        else
            drawPolyline(flat, c, true, state.lineWidth)
        end
    end
end

function ctxMethods.fillText(text, x, y)
    local c = applyAlpha(state.fillStyle, state.globalAlpha)
    local xOff, yOff = 0, 0
    if state.textAlign ~= "left" and state.textAlign ~= "start" then
        local met = measureText(text)
        if state.textAlign == "center" then xOff = -met.width / 2
        elseif state.textAlign == "right" or state.textAlign == "end" then xOff = -met.width end
    end
    if state.textBaseline ~= "alphabetic" and state.textBaseline ~= "ideographic" then
        local met2 = measureText(text)
        if state.textBaseline == "middle" then yOff = -met2.height / 2
        elseif state.textBaseline == "bottom" then yOff = -met2.height end
    end
    local px, py = _tx(x + xOff, y + yOff)
    drawText(px, py, c, text)
end
)LUA"

    // --- Part 3: path API, stroke, fill ---
    + R"LUA(
function ctxMethods.beginPath()
    subPaths = {}
    currentSubPath = nil
end

function ctxMethods.moveTo(x, y)
    if currentSubPath then subPaths[#subPaths+1] = currentSubPath end
    currentSubPath = {points = {x, y}, closed = false}
end

function ctxMethods.lineTo(x, y)
    if not currentSubPath then
        currentSubPath = {points = {x, y}, closed = false}
    else
        local pts = currentSubPath.points
        pts[#pts+1] = x
        pts[#pts+1] = y
    end
end

function ctxMethods.closePath()
    if currentSubPath and #currentSubPath.points >= 4 then
        currentSubPath.closed = true
        subPaths[#subPaths+1] = currentSubPath
        local firstX = currentSubPath.points[1]
        local firstY = currentSubPath.points[2]
        currentSubPath = {points = {firstX, firstY}, closed = false}
    end
end

function ctxMethods.arc(cx, cy, radius, startAngle, endAngle, ccw)
    if radius < 0 then return end
    ccw = ccw and true or false
    local TWO_PI = 2 * math.pi
    local segs = math.max(12, math.min(128, math.ceil(radius * 0.5)))
    local startA = startAngle
    local endA = endAngle
    if ccw then
        while endA >= startA do endA = endA - TWO_PI end
    else
        while endA <= startA do endA = endA + TWO_PI end
    end
    local sweep = endA - startA
    if math.abs(sweep) > TWO_PI then sweep = ccw and -TWO_PI or TWO_PI end
    local n = math.max(1, math.ceil(math.abs(sweep) / TWO_PI * segs))
    for i = 0, n do
        local t = startA + sweep * i / n
        local px = cx + math.cos(t) * radius
        local py = cy + math.sin(t) * radius
        if not currentSubPath then
            currentSubPath = {points = {px, py}, closed = false}
        else
            local pts = currentSubPath.points
            pts[#pts+1] = px
            pts[#pts+1] = py
        end
    end
end

function ctxMethods.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y)
    if not currentSubPath then
        currentSubPath = {points = {cp1x, cp1y}, closed = false}
    end
    local pts = currentSubPath.points
    local lastX = pts[#pts-1]
    local lastY = pts[#pts]
    for i = 1, 20 do
        local t = i / 20
        local u = 1 - t
        pts[#pts+1] = u*u*u*lastX + 3*u*u*t*cp1x + 3*u*t*t*cp2x + t*t*t*x
        pts[#pts+1] = u*u*u*lastY + 3*u*u*t*cp1y + 3*u*t*t*cp2y + t*t*t*y
    end
end

function ctxMethods.quadraticCurveTo(cpx, cpy, x, y)
    if not currentSubPath then
        currentSubPath = {points = {cpx, cpy}, closed = false}
    end
    local pts = currentSubPath.points
    local lastX = pts[#pts-1]
    local lastY = pts[#pts]
    local cp1x = lastX + 2/3*(cpx-lastX)
    local cp1y = lastY + 2/3*(cpy-lastY)
    local cp2x = x + 2/3*(cpx-x)
    local cp2y = y + 2/3*(cpy-y)
    ctxMethods.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y)
end

function ctxMethods.rect(x, y, w, h)
    ctxMethods.moveTo(x,y)
    ctxMethods.lineTo(x+w,y)
    ctxMethods.lineTo(x+w,y+h)
    ctxMethods.lineTo(x,y+h)
    ctxMethods.closePath()
end

function ctxMethods.stroke()
    local c = applyAlpha(state.strokeStyle, state.globalAlpha)
    local all = {}
    for i = 1, #subPaths do all[#all+1] = subPaths[i] end
    if currentSubPath and #currentSubPath.points >= 4 then
        all[#all+1] = currentSubPath
    end
    for p = 1, #all do
        local sp = all[p]
        if #sp.points >= 4 then
            local tr = {}
            for i = 1, #sp.points, 2 do
                local tx, ty = _tx(sp.points[i], sp.points[i+1])
                tr[#tr+1] = tx
                tr[#tr+1] = ty
            end
            if #state._lineDash > 0 then
                strokeDashedPolyline(tr, sp.closed, c, state.lineWidth, state._lineDash, state._lineDashOffset)
            else
                drawPolyline(tr, c, sp.closed, state.lineWidth)
            end
        end
    end
end

function ctxMethods.fill()
    local c = applyAlpha(state.fillStyle, state.globalAlpha)
    local all = {}
    for i = 1, #subPaths do all[#all+1] = subPaths[i] end
    if currentSubPath and #currentSubPath.points >= 6 then
        all[#all+1] = currentSubPath
    end
    for p = 1, #all do
        local sp = all[p]
        if #sp.points >= 6 then
            local flat = {}
            for i = 1, #sp.points, 2 do
                local tx, ty = _tx(sp.points[i], sp.points[i+1])
                flat[#flat+1] = tx
                flat[#flat+1] = ty
            end
            drawConvexPolyFilled(flat, c)
        end
    end
end
)LUA"

    // --- Part 4: transforms, text measurement, dashed lines, clip, stubs ---
    + R"LUA(
function ctxMethods.translate(x, y)
    state._matrix = multiplyMatrix(state._matrix, {1, 0, 0, 1, x, y})
end

function ctxMethods.rotate(angle)
    local cos = math.cos(angle)
    local sin = math.sin(angle)
    state._matrix = multiplyMatrix(state._matrix, {cos, sin, -sin, cos, 0, 0})
end

function ctxMethods.scale(x, y)
    state._matrix = multiplyMatrix(state._matrix, {x, 0, 0, y, 0, 0})
end

function ctxMethods.transform(a, b, c, d, e, f)
    state._matrix = multiplyMatrix(state._matrix, {a, b, c, d, e, f})
end

function ctxMethods.setTransform(a, b, c, d, e, f)
    if type(a) == "table" then
        state._matrix = {a.a or a.m11 or 1, a.b or a.m12 or 0, a.c or a.m21 or 0, a.d or a.m22 or 1, a.e or a.m41 or 0, a.f or a.m42 or 0}
    else
        state._matrix = {a, b, c, d, e, f}
    end
end

function ctxMethods.resetTransform()
    state._matrix = {1, 0, 0, 1, 0, 0}
end

function ctxMethods.getTransform()
    local m = state._matrix
    return {a=m[1], b=m[2], c=m[3], d=m[4], e=m[5], f=m[6]}
end

function ctxMethods.measureText(text)
    local r = measureText(tostring(text))
    return {width = r.width}
end

function ctxMethods.setLineDash(segs)
    if type(segs) ~= "table" then return end
    for i = 1, #segs do
        if type(segs[i]) ~= "number" or segs[i] < 0 then return end
    end
    state._lineDash = copyArray(segs)
end

function ctxMethods.getLineDash()
    return copyArray(state._lineDash)
end

function ctxMethods.clip()
    local all = {}
    for i = 1, #subPaths do all[#all+1] = subPaths[i] end
    if currentSubPath and #currentSubPath.points >= 2 then
        all[#all+1] = currentSubPath
    end
    if #all == 0 then return end
    local minX, minY = math.huge, math.huge
    local maxX, maxY = -math.huge, -math.huge
    for p = 1, #all do
        local pts = all[p].points
        for i = 1, #pts, 2 do
            local tx, ty = _tx(pts[i], pts[i+1])
            if tx < minX then minX = tx end
            if ty < minY then minY = ty end
            if tx > maxX then maxX = tx end
            if ty > maxY then maxY = ty end
        end
    end
    if minX ~= math.huge and minY ~= math.huge then
        pushClipRect(minX, minY, maxX, maxY)
        state._clipCount = state._clipCount + 1
    end
end

-- Stubs for unsupported operations
function ctxMethods.createLinearGradient() return {addColorStop = function() end} end
function ctxMethods.createRadialGradient() return {addColorStop = function() end} end
function ctxMethods.createPattern() return nil end
function ctxMethods.isPointInPath() return false end
function ctxMethods.isPointInStroke() return false end

ctx = ctxData
)LUA";
    return s;
}

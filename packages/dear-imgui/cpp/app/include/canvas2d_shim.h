#pragma once

#include <string>

// Canvas 2D API shim — evaluated once in InitQuickJS() after registerDrawBindings().
// Creates globalThis.ctx with an HTML5 Canvas 2D-style API that delegates to drawXxx bindings.
// Raw drawXxx functions remain available alongside ctx.
//
// Uses runtime std::string concatenation because MSVC limits individual string literals
// to 16380 chars and applies this limit even after adjacent-literal concatenation.

inline const std::string& getCanvas2DShim() {
    static const std::string s =

    // --- Part 1: helpers, state, dashed-line engine ---
    std::string(R"JS(
(function() {
"use strict";

function parseColor(c) {
    if (typeof c !== 'string' || c.length === 0) return 'rgba(0,0,0,1)';
    return c;
}

function applyAlpha(color, alpha) {
    if (alpha >= 1.0) return color;
    if (alpha <= 0.0) return 'rgba(0,0,0,0)';
    return color;
}

function defaultState() {
    return {
        fillStyle: '#000000',
        strokeStyle: '#000000',
        lineWidth: 1.0,
        globalAlpha: 1.0,
        font: '10px sans-serif',
        _fontSize: 10,
        textAlign: 'start',
        textBaseline: 'alphabetic',
        _matrix: [1, 0, 0, 1, 0, 0],
        _lineDash: [],
        _lineDashOffset: 0,
        _clipCount: 0
    };
}

var stateStack = [];
var state = defaultState();
var subPaths = [];
var currentSubPath = null;

function _tx(x, y) {
    var m = state._matrix;
    return {
        x: m[0] * x + m[2] * y + m[4],
        y: m[1] * x + m[3] * y + m[5]
    };
}

function isIdentityOrTranslation() {
    var m = state._matrix;
    return m[0] === 1 && m[1] === 0 && m[2] === 0 && m[3] === 1;
}

function multiplyMatrix(a, b) {
    return [
        a[0]*b[0] + a[2]*b[1],
        a[1]*b[0] + a[3]*b[1],
        a[0]*b[2] + a[2]*b[3],
        a[1]*b[2] + a[3]*b[3],
        a[0]*b[4] + a[2]*b[5] + a[4],
        a[1]*b[4] + a[3]*b[5] + a[5]
    ];
}

function distPt(a, b) {
    var dx = b.x - a.x, dy = b.y - a.y;
    return Math.sqrt(dx*dx + dy*dy);
}

function strokeDashedPolyline(points, closed, color, thickness, dashPattern, dashOffset) {
    if (dashPattern.length === 0) {
        var flat = [];
        for (var i = 0; i < points.length; i++) flat.push(points[i].x, points[i].y);
        drawPolyline(flat, color, closed, thickness);
        return;
    }
    var pattern = dashPattern.slice();
    if (pattern.length % 2 !== 0) pattern = pattern.concat(pattern);
    var edges = [];
    for (var i = 0; i < points.length - 1; i++) edges.push({from: points[i], to: points[i+1]});
    if (closed && points.length > 1) edges.push({from: points[points.length-1], to: points[0]});
    var patIdx = 0, patRemain = pattern[0], drawing = true, currentDash = [];
    var total = 0;
    for (var i = 0; i < pattern.length; i++) total += pattern[i];
    var offset = dashOffset % total;
    if (offset < 0) offset += total;
    while (offset > 0) {
        if (offset >= patRemain) {
            offset -= patRemain;
            drawing = !drawing;
            patIdx = (patIdx + 1) % pattern.length;
            patRemain = pattern[patIdx];
        } else { patRemain -= offset; offset = 0; }
    }
    for (var e = 0; e < edges.length; e++) {
        var edge = edges[e];
        var edgeLen = distPt(edge.from, edge.to);
        if (edgeLen === 0) continue;
        var dx = (edge.to.x - edge.from.x) / edgeLen;
        var dy = (edge.to.y - edge.from.y) / edgeLen;
        var pos = 0;
        while (pos < edgeLen) {
            var step = Math.min(patRemain, edgeLen - pos);
            var px = edge.from.x + dx * (pos + step);
            var py = edge.from.y + dy * (pos + step);
            if (drawing) {
                if (currentDash.length === 0)
                    currentDash.push({x: edge.from.x + dx * pos, y: edge.from.y + dy * pos});
                currentDash.push({x: px, y: py});
            }
            pos += step;
            patRemain -= step;
            if (patRemain <= 0.0001) {
                if (drawing && currentDash.length >= 2) {
                    var fl = [];
                    for (var k = 0; k < currentDash.length; k++) fl.push(currentDash[k].x, currentDash[k].y);
                    drawPolyline(fl, color, false, thickness);
                }
                currentDash = [];
                drawing = !drawing;
                patIdx = (patIdx + 1) % pattern.length;
                patRemain = pattern[patIdx];
            }
        }
    }
    if (drawing && currentDash.length >= 2) {
        var fl = [];
        for (var k = 0; k < currentDash.length; k++) fl.push(currentDash[k].x, currentDash[k].y);
        drawPolyline(fl, color, false, thickness);
    }
}
)JS")

    // --- Part 2: ctx object — properties, save/restore, basic drawing, fillText ---
    + R"JS(
var ctx = {
    canvas: {
        get width() { return globalThis.__canvasWidth || 0; },
        get height() { return globalThis.__canvasHeight || 0; }
    },
    get fillStyle() { return state.fillStyle; },
    set fillStyle(v) { state.fillStyle = parseColor(v); },
    get strokeStyle() { return state.strokeStyle; },
    set strokeStyle(v) { state.strokeStyle = parseColor(v); },
    get lineWidth() { return state.lineWidth; },
    set lineWidth(v) { if (v > 0) state.lineWidth = v; },
    get globalAlpha() { return state.globalAlpha; },
    set globalAlpha(v) { if (v >= 0 && v <= 1) state.globalAlpha = v; },
    get font() { return state.font; },
    set font(v) {
        state.font = v;
        var m = /(\d+(?:\.\d+)?)\s*px/.exec(v);
        if (m) state._fontSize = parseFloat(m[1]);
    },
    get textAlign() { return state.textAlign; },
    set textAlign(v) {
        if (v==='left'||v==='right'||v==='center'||v==='start'||v==='end') state.textAlign = v;
    },
    get textBaseline() { return state.textBaseline; },
    set textBaseline(v) {
        if (v==='top'||v==='middle'||v==='bottom'||v==='alphabetic'||v==='hanging'||v==='ideographic')
            state.textBaseline = v;
    },
    get lineDashOffset() { return state._lineDashOffset; },
    set lineDashOffset(v) { state._lineDashOffset = v; },

    save: function() {
        stateStack.push({
            fillStyle: state.fillStyle, strokeStyle: state.strokeStyle,
            lineWidth: state.lineWidth, globalAlpha: state.globalAlpha,
            font: state.font, _fontSize: state._fontSize,
            textAlign: state.textAlign, textBaseline: state.textBaseline,
            _matrix: state._matrix.slice(),
            _lineDash: state._lineDash.slice(), _lineDashOffset: state._lineDashOffset,
            _clipCount: state._clipCount
        });
        state._clipCount = 0;
    },
    restore: function() {
        for (var i = 0; i < state._clipCount; i++) __popClipRect();
        if (stateStack.length > 0) {
            var prev = stateStack.pop();
            state.fillStyle = prev.fillStyle; state.strokeStyle = prev.strokeStyle;
            state.lineWidth = prev.lineWidth; state.globalAlpha = prev.globalAlpha;
            state.font = prev.font; state._fontSize = prev._fontSize;
            state.textAlign = prev.textAlign; state.textBaseline = prev.textBaseline;
            state._matrix = prev._matrix;
            state._lineDash = prev._lineDash; state._lineDashOffset = prev._lineDashOffset;
            state._clipCount = prev._clipCount;
        }
    },
    clearRect: function(x, y, w, h) {
        if (isIdentityOrTranslation()) {
            var p = _tx(x, y); drawRectFilled(p.x, p.y, w, h, '#000000');
        } else {
            var p1=_tx(x,y),p2=_tx(x+w,y),p3=_tx(x+w,y+h),p4=_tx(x,y+h);
            drawConvexPolyFilled([p1.x,p1.y,p2.x,p2.y,p3.x,p3.y,p4.x,p4.y], '#000000');
        }
    },
    fillRect: function(x, y, w, h) {
        var c = applyAlpha(state.fillStyle, state.globalAlpha);
        if (isIdentityOrTranslation()) {
            var p = _tx(x, y); drawRectFilled(p.x, p.y, w, h, c);
        } else {
            var p1=_tx(x,y),p2=_tx(x+w,y),p3=_tx(x+w,y+h),p4=_tx(x,y+h);
            drawConvexPolyFilled([p1.x,p1.y,p2.x,p2.y,p3.x,p3.y,p4.x,p4.y], c);
        }
    },
    strokeRect: function(x, y, w, h) {
        var c = applyAlpha(state.strokeStyle, state.globalAlpha);
        if (isIdentityOrTranslation()) {
            var p = _tx(x, y); drawRect(p.x, p.y, w, h, c, state.lineWidth);
        } else {
            var p1=_tx(x,y),p2=_tx(x+w,y),p3=_tx(x+w,y+h),p4=_tx(x,y+h);
            var flat = [p1.x,p1.y,p2.x,p2.y,p3.x,p3.y,p4.x,p4.y];
            if (state._lineDash.length > 0) {
                var pts = [{x:p1.x,y:p1.y},{x:p2.x,y:p2.y},{x:p3.x,y:p3.y},{x:p4.x,y:p4.y}];
                strokeDashedPolyline(pts, true, c, state.lineWidth, state._lineDash, state._lineDashOffset);
            } else drawPolyline(flat, c, true, state.lineWidth);
        }
    },
    fillText: function(text, x, y) {
        var c = applyAlpha(state.fillStyle, state.globalAlpha);
        var xOff = 0, yOff = 0;
        if (state.textAlign !== 'left' && state.textAlign !== 'start') {
            var met = __measureText(text);
            if (state.textAlign === 'center') xOff = -met.width / 2;
            else if (state.textAlign === 'right' || state.textAlign === 'end') xOff = -met.width;
        }
        if (state.textBaseline !== 'alphabetic' && state.textBaseline !== 'ideographic') {
            var met2 = __measureText(text);
            if (state.textBaseline === 'middle') yOff = -met2.height / 2;
            else if (state.textBaseline === 'bottom') yOff = -met2.height;
        }
        var p = _tx(x + xOff, y + yOff);
        drawText(p.x, p.y, c, text);
    },
)JS"

    // --- Part 3: path API, stroke, fill ---
    + R"JS(
    beginPath: function() { subPaths = []; currentSubPath = null; },
    moveTo: function(x, y) {
        if (currentSubPath) subPaths.push(currentSubPath);
        currentSubPath = { points: [{x:x,y:y}], closed: false };
    },
    lineTo: function(x, y) {
        if (!currentSubPath) currentSubPath = { points: [{x:x,y:y}], closed: false };
        else currentSubPath.points.push({x:x,y:y});
    },
    closePath: function() {
        if (currentSubPath && currentSubPath.points.length > 1) {
            currentSubPath.closed = true;
            subPaths.push(currentSubPath);
            var first = currentSubPath.points[0];
            currentSubPath = { points: [{x:first.x,y:first.y}], closed: false };
        }
    },
    arc: function(cx, cy, radius, startAngle, endAngle, ccw) {
        if (radius < 0) return;
        ccw = !!ccw;
        var TWO_PI = 2 * Math.PI;
        var segs = Math.max(12, Math.min(128, Math.ceil(radius * 0.5)));
        var start = startAngle, end = endAngle;
        if (ccw) { while (end >= start) end -= TWO_PI; }
        else { while (end <= start) end += TWO_PI; }
        var sweep = end - start;
        if (Math.abs(sweep) > TWO_PI) sweep = ccw ? -TWO_PI : TWO_PI;
        var n = Math.max(1, Math.ceil(Math.abs(sweep) / TWO_PI * segs));
        for (var i = 0; i <= n; i++) {
            var t = start + sweep * i / n;
            var px = cx + Math.cos(t) * radius, py = cy + Math.sin(t) * radius;
            if (i === 0 && !currentSubPath)
                currentSubPath = { points: [{x:px,y:py}], closed: false };
            else if (!currentSubPath)
                currentSubPath = { points: [{x:px,y:py}], closed: false };
            else currentSubPath.points.push({x:px,y:py});
        }
    },
    bezierCurveTo: function(cp1x, cp1y, cp2x, cp2y, x, y) {
        if (!currentSubPath) currentSubPath = { points: [{x:cp1x,y:cp1y}], closed: false };
        var last = currentSubPath.points[currentSubPath.points.length - 1];
        for (var i = 1; i <= 20; i++) {
            var t = i / 20, u = 1 - t;
            currentSubPath.points.push({
                x: u*u*u*last.x + 3*u*u*t*cp1x + 3*u*t*t*cp2x + t*t*t*x,
                y: u*u*u*last.y + 3*u*u*t*cp1y + 3*u*t*t*cp2y + t*t*t*y
            });
        }
    },
    quadraticCurveTo: function(cpx, cpy, x, y) {
        if (!currentSubPath) currentSubPath = { points: [{x:cpx,y:cpy}], closed: false };
        var last = currentSubPath.points[currentSubPath.points.length - 1];
        var cp1x = last.x + 2/3*(cpx-last.x), cp1y = last.y + 2/3*(cpy-last.y);
        var cp2x = x + 2/3*(cpx-x), cp2y = y + 2/3*(cpy-y);
        ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
    },
    rect: function(x, y, w, h) {
        ctx.moveTo(x,y); ctx.lineTo(x+w,y); ctx.lineTo(x+w,y+h); ctx.lineTo(x,y+h); ctx.closePath();
    },
    stroke: function() {
        var c = applyAlpha(state.strokeStyle, state.globalAlpha);
        var all = subPaths.slice();
        if (currentSubPath && currentSubPath.points.length > 1) all.push(currentSubPath);
        for (var p = 0; p < all.length; p++) {
            var sp = all[p];
            if (sp.points.length < 2) continue;
            var tr = [];
            for (var i = 0; i < sp.points.length; i++) tr.push(_tx(sp.points[i].x, sp.points[i].y));
            if (state._lineDash.length > 0)
                strokeDashedPolyline(tr, sp.closed, c, state.lineWidth, state._lineDash, state._lineDashOffset);
            else {
                var flat = [];
                for (var i = 0; i < tr.length; i++) flat.push(tr[i].x, tr[i].y);
                drawPolyline(flat, c, sp.closed, state.lineWidth);
            }
        }
    },
    fill: function() {
        var c = applyAlpha(state.fillStyle, state.globalAlpha);
        var all = subPaths.slice();
        if (currentSubPath && currentSubPath.points.length > 2) all.push(currentSubPath);
        for (var p = 0; p < all.length; p++) {
            var sp = all[p];
            if (sp.points.length < 3) continue;
            var flat = [];
            for (var i = 0; i < sp.points.length; i++) {
                var tp = _tx(sp.points[i].x, sp.points[i].y);
                flat.push(tp.x, tp.y);
            }
            drawConvexPolyFilled(flat, c);
        }
    },
)JS"

    // --- Part 4: transforms, text measurement, dashed lines, clip, stubs ---
    + R"JS(
    translate: function(x, y) {
        state._matrix = multiplyMatrix(state._matrix, [1, 0, 0, 1, x, y]);
    },
    rotate: function(angle) {
        var cos = Math.cos(angle), sin = Math.sin(angle);
        state._matrix = multiplyMatrix(state._matrix, [cos, sin, -sin, cos, 0, 0]);
    },
    scale: function(x, y) {
        state._matrix = multiplyMatrix(state._matrix, [x, 0, 0, y, 0, 0]);
    },
    transform: function(a, b, c, d, e, f) {
        state._matrix = multiplyMatrix(state._matrix, [a, b, c, d, e, f]);
    },
    setTransform: function(a, b, c, d, e, f) {
        if (typeof a === 'object')
            state._matrix = [a.a||a.m11||1, a.b||a.m12||0, a.c||a.m21||0, a.d||a.m22||1, a.e||a.m41||0, a.f||a.m42||0];
        else state._matrix = [a, b, c, d, e, f];
    },
    resetTransform: function() { state._matrix = [1, 0, 0, 1, 0, 0]; },
    getTransform: function() {
        var m = state._matrix;
        return {a:m[0], b:m[1], c:m[2], d:m[3], e:m[4], f:m[5]};
    },
    measureText: function(text) {
        var r = __measureText(String(text));
        return { width: r.width };
    },
    setLineDash: function(segs) {
        if (!Array.isArray(segs)) return;
        for (var i = 0; i < segs.length; i++)
            if (!isFinite(segs[i]) || segs[i] < 0) return;
        state._lineDash = segs.slice();
    },
    getLineDash: function() { return state._lineDash.slice(); },
    clip: function() {
        var all = subPaths.slice();
        if (currentSubPath && currentSubPath.points.length > 0) all.push(currentSubPath);
        if (all.length === 0) return;
        var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        for (var p = 0; p < all.length; p++) {
            var pts = all[p].points;
            for (var i = 0; i < pts.length; i++) {
                var tp = _tx(pts[i].x, pts[i].y);
                if (tp.x < minX) minX = tp.x; if (tp.y < minY) minY = tp.y;
                if (tp.x > maxX) maxX = tp.x; if (tp.y > maxY) maxY = tp.y;
            }
        }
        if (isFinite(minX) && isFinite(minY)) { __pushClipRect(minX, minY, maxX, maxY); state._clipCount++; }
    },
    createLinearGradient: function() { return {addColorStop: function(){}}; },
    createRadialGradient: function() { return {addColorStop: function(){}}; },
    createPattern: function() { return null; },
    isPointInPath: function() { return false; },
    isPointInStroke: function() { return false; }
};

globalThis.ctx = ctx;

})();
)JS";
    return s;
}

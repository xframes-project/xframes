var w = ctx.canvas.width;
var h = ctx.canvas.height;
var r = Math.min(w, h) / 2 - 20;
var cx = w / 2;
var cy = h / 2;
var d = globalThis.data || {};
var now = d.time || 0;
var sec = now % 60;
var min = Math.floor(now / 60) % 60;
var hr = Math.floor(now / 3600) % 12;

// Background
ctx.fillStyle = '#1a1a2e';
ctx.fillRect(0, 0, w, h);

// Title
ctx.fillStyle = '#e0e0e0';
ctx.font = '16px roboto-regular';
ctx.textAlign = 'center';
ctx.fillText('Canvas 2D API Demo', cx, 20);

// Clock face
ctx.save();
ctx.translate(cx, cy + 10);

ctx.beginPath();
ctx.arc(0, 0, r, 0, Math.PI * 2);
ctx.fillStyle = '#16213e';
ctx.fill();
ctx.strokeStyle = '#0f3460';
ctx.lineWidth = 3;
ctx.stroke();

// Hour markers
for (var i = 0; i < 12; i++) {
  ctx.save();
  ctx.rotate((i * Math.PI) / 6);
  ctx.fillStyle = '#e94560';
  ctx.fillRect(-2, -r + 5, 4, 15);
  ctx.restore();
}

// Minute markers
for (var i = 0; i < 60; i++) {
  if (i % 5 !== 0) {
    ctx.save();
    ctx.rotate((i * Math.PI) / 30);
    ctx.fillStyle = '#533483';
    ctx.fillRect(-1, -r + 8, 2, 8);
    ctx.restore();
  }
}

// Hour hand
ctx.save();
ctx.rotate((hr + min / 60) * (Math.PI / 6));
ctx.strokeStyle = '#e94560';
ctx.lineWidth = 4;
ctx.beginPath();
ctx.moveTo(0, 10);
ctx.lineTo(0, -r * 0.5);
ctx.stroke();
ctx.restore();

// Minute hand
ctx.save();
ctx.rotate((min + sec / 60) * (Math.PI / 30));
ctx.strokeStyle = '#0f3460';
ctx.lineWidth = 3;
ctx.beginPath();
ctx.moveTo(0, 15);
ctx.lineTo(0, -r * 0.7);
ctx.stroke();
ctx.restore();

// Second hand
ctx.save();
ctx.rotate(sec * (Math.PI / 30));
ctx.strokeStyle = '#e94560';
ctx.lineWidth = 1;
ctx.beginPath();
ctx.moveTo(0, 20);
ctx.lineTo(0, -r * 0.85);
ctx.stroke();
ctx.restore();

// Center dot
ctx.beginPath();
ctx.arc(0, 0, 5, 0, Math.PI * 2);
ctx.fillStyle = '#e94560';
ctx.fill();

ctx.restore();

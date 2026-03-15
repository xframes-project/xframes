local w = ctx.canvas.width
local h = ctx.canvas.height
local r = math.min(w, h) / 2 - 20
local cx = w / 2
local cy = h / 2
local d = data or {}
local now = d.time or 0
local sec = now % 60
local min = math.floor(now / 60) % 60
local hr = math.floor(now / 3600) % 12

-- Background
ctx.fillStyle = '#1a1a2e'
ctx.fillRect(0, 0, w, h)

-- Title
ctx.fillStyle = '#e0e0e0'
ctx.font = '16px roboto-regular'
ctx.textAlign = 'center'
ctx.fillText('Lua Canvas 2D API Demo', cx, 20)

-- Clock face
ctx.save()
ctx.translate(cx, cy + 10)

ctx.beginPath()
ctx.arc(0, 0, r, 0, math.pi * 2)
ctx.fillStyle = '#16213e'
ctx.fill()
ctx.strokeStyle = '#0f3460'
ctx.lineWidth = 3
ctx.stroke()

-- Hour markers
for i = 0, 11 do
  ctx.save()
  ctx.rotate(i * math.pi / 6)
  ctx.fillStyle = '#e94560'
  ctx.fillRect(-2, -r + 5, 4, 15)
  ctx.restore()
end

-- Minute markers
for i = 0, 59 do
  if i % 5 ~= 0 then
    ctx.save()
    ctx.rotate(i * math.pi / 30)
    ctx.fillStyle = '#533483'
    ctx.fillRect(-1, -r + 8, 2, 8)
    ctx.restore()
  end
end

-- Hour hand
ctx.save()
ctx.rotate((hr + min / 60) * (math.pi / 6))
ctx.strokeStyle = '#e94560'
ctx.lineWidth = 4
ctx.beginPath()
ctx.moveTo(0, 10)
ctx.lineTo(0, -r * 0.5)
ctx.stroke()
ctx.restore()

-- Minute hand
ctx.save()
ctx.rotate((min + sec / 60) * (math.pi / 30))
ctx.strokeStyle = '#0f3460'
ctx.lineWidth = 3
ctx.beginPath()
ctx.moveTo(0, 15)
ctx.lineTo(0, -r * 0.7)
ctx.stroke()
ctx.restore()

-- Second hand
ctx.save()
ctx.rotate(sec * (math.pi / 30))
ctx.strokeStyle = '#e94560'
ctx.lineWidth = 1
ctx.beginPath()
ctx.moveTo(0, 20)
ctx.lineTo(0, -r * 0.85)
ctx.stroke()
ctx.restore()

-- Center dot
ctx.beginPath()
ctx.arc(0, 0, 5, 0, math.pi * 2)
ctx.fillStyle = '#e94560'
ctx.fill()

ctx.restore()

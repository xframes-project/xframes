(def w canvas-width)
(def h canvas-height)
(def r (- (/ (min w h) 2) 20))
(def cx (/ w 2))
(def cy (/ h 2))
(def d (if (nil? data) @{} data))
(def now (or (get d :time) 0))
(def sec (% now 60))
(def mn (% (math/floor (/ now 60)) 60))
(def hr (% (math/floor (/ now 3600)) 12))

# Background
(put ctx :fill-style "#1a1a2e")
(ctx-fill-rect 0 0 w h)

# Title
(put ctx :fill-style "#e0e0e0")
(put ctx :font "16px roboto-regular")
(put ctx :text-align "center")
(ctx-fill-text "Janet Canvas 2D API Demo" cx 20)

# Clock face
(ctx-save)
(ctx-translate cx (+ cy 10))

(ctx-begin-path)
(ctx-arc 0 0 r 0 (* math/pi 2))
(put ctx :fill-style "#16213e")
(ctx-fill)
(put ctx :stroke-style "#0f3460")
(put ctx :line-width 3)
(ctx-stroke)

# Hour markers
(for i 0 12
  (ctx-save)
  (ctx-rotate (* i (/ math/pi 6)))
  (put ctx :fill-style "#e94560")
  (ctx-fill-rect -2 (+ (- r) 5) 4 15)
  (ctx-restore))

# Minute markers
(for i 0 60
  (when (not= (% i 5) 0)
    (ctx-save)
    (ctx-rotate (* i (/ math/pi 30)))
    (put ctx :fill-style "#533483")
    (ctx-fill-rect -1 (+ (- r) 8) 2 8)
    (ctx-restore)))

# Hour hand
(ctx-save)
(ctx-rotate (* (+ hr (/ mn 60)) (/ math/pi 6)))
(put ctx :stroke-style "#e94560")
(put ctx :line-width 4)
(ctx-begin-path)
(ctx-move-to 0 10)
(ctx-line-to 0 (* r -0.5))
(ctx-stroke)
(ctx-restore)

# Minute hand
(ctx-save)
(ctx-rotate (* (+ mn (/ sec 60)) (/ math/pi 30)))
(put ctx :stroke-style "#0f3460")
(put ctx :line-width 3)
(ctx-begin-path)
(ctx-move-to 0 15)
(ctx-line-to 0 (* r -0.7))
(ctx-stroke)
(ctx-restore)

# Second hand
(ctx-save)
(ctx-rotate (* sec (/ math/pi 30)))
(put ctx :stroke-style "#e94560")
(put ctx :line-width 1)
(ctx-begin-path)
(ctx-move-to 0 20)
(ctx-line-to 0 (* r -0.85))
(ctx-stroke)
(ctx-restore)

# Center dot
(ctx-begin-path)
(ctx-arc 0 0 5 0 (* math/pi 2))
(put ctx :fill-style "#e94560")
(ctx-fill)

(ctx-restore)

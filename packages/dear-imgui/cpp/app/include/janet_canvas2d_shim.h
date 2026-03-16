#pragma once

#include <string>

// Janet Canvas 2D API shim — evaluated once in JanetCanvas::InitJanet() after registerDrawBindings().
// Creates a global `ctx` table with Canvas 2D-style properties and top-level ctx-xxx functions.
// Raw draw-xxx functions remain available alongside ctx.
//
// Properties: (ctx :fill-style), (put ctx :fill-style "#ff0000")
// Methods: (ctx-fill-rect 0 0 100 50), (ctx-save), (ctx-arc 0 0 r 0 (* math/pi 2))
// Canvas dimensions: use canvas-width / canvas-height directly (global vars set per-frame)
//
// Uses runtime std::string concatenation because MSVC limits individual string literals
// to 16380 chars and applies this limit even after adjacent-literal concatenation.

inline const std::string& getJanetCanvas2DShim() {
    static const std::string s =

    // --- Part 1: helpers, dashed-line engine ---
    std::string(R"JANET(
(defn- parse-color [c]
  (if (and (string? c) (> (length c) 0)) c "rgba(0,0,0,1)"))

(defn- apply-alpha [color alpha]
  (cond
    (>= alpha 1.0) color
    (<= alpha 0.0) "rgba(0,0,0,0)"
    color))

# ctx IS the state table — no separate state variable needed (Janet has no metatables)
(var ctx @{:fill-style "#000000"
           :stroke-style "#000000"
           :line-width 1.0
           :global-alpha 1.0
           :font "10px sans-serif"
           :font-size 10
           :text-align "start"
           :text-baseline "alphabetic"
           :matrix @[1 0 0 1 0 0]
           :line-dash @[]
           :line-dash-offset 0
           :clip-count 0})

(var state-stack @[])
(var sub-paths @[])
(var current-sub-path nil)

(defn- tx [x y]
  (def m (ctx :matrix))
  (def px (+ (* (m 0) x) (* (m 2) y) (m 4)))
  (def py (+ (* (m 1) x) (* (m 3) y) (m 5)))
  [px py])

(defn- identity-or-translation? []
  (def m (ctx :matrix))
  (and (= (m 0) 1) (= (m 1) 0) (= (m 2) 0) (= (m 3) 1)))

(defn- multiply-matrix [a b]
  @[(+ (* (a 0) (b 0)) (* (a 2) (b 1)))
    (+ (* (a 1) (b 0)) (* (a 3) (b 1)))
    (+ (* (a 0) (b 2)) (* (a 2) (b 3)))
    (+ (* (a 1) (b 2)) (* (a 3) (b 3)))
    (+ (* (a 0) (b 4)) (* (a 2) (b 5)) (a 4))
    (+ (* (a 1) (b 4)) (* (a 3) (b 5)) (a 5))])

(defn- copy-array [t]
  (def c @[])
  (each v t (array/push c v))
  c)

(defn- dist-pt [ax ay bx by]
  (def dx (- bx ax))
  (def dy (- by ay))
  (math/sqrt (+ (* dx dx) (* dy dy))))

(defn- stroke-dashed-polyline [points closed color thickness dash-pattern dash-offset]
  (if (= (length dash-pattern) 0)
    (do
      (def flat @[])
      (var i 0)
      (while (< i (length points))
        (array/push flat (points i))
        (array/push flat (points (+ i 1)))
        (+= i 2))
      (draw-polyline flat color closed thickness))
    (do
      (def pattern (copy-array dash-pattern))
      (when (not= (% (length pattern) 2) 0)
        (def n (length pattern))
        (for i 0 n (array/push pattern (pattern i))))
      (def edges @[])
      (var i 0)
      (while (< i (- (length points) 2))
        (array/push edges @[(points i) (points (+ i 1)) (points (+ i 2)) (points (+ i 3))])
        (+= i 2))
      (when (and closed (>= (length points) 4))
        (array/push edges @[(points (- (length points) 2)) (points (- (length points) 1)) (points 0) (points 1)]))
      (var pat-idx 0)
      (var pat-remain (pattern 0))
      (var drawing true)
      (var current-dash @[])
      (var total 0)
      (each v pattern (+= total v))
      (var offset (% dash-offset total))
      (when (< offset 0) (+= offset total))
      (while (> offset 0)
        (if (>= offset pat-remain)
          (do
            (-= offset pat-remain)
            (set drawing (not drawing))
            (set pat-idx (% (+ pat-idx 1) (length pattern)))
            (set pat-remain (pattern pat-idx)))
          (do
            (-= pat-remain offset)
            (set offset 0))))
      (each edge edges
        (def fx (edge 0))
        (def fy (edge 1))
        (def tox (edge 2))
        (def toy (edge 3))
        (def edge-len (dist-pt fx fy tox toy))
        (when (> edge-len 0)
          (def dx (/ (- tox fx) edge-len))
          (def dy (/ (- toy fy) edge-len))
          (var pos 0)
          (while (< pos edge-len)
            (def step (min pat-remain (- edge-len pos)))
            (def px (+ fx (* dx (+ pos step))))
            (def py (+ fy (* dy (+ pos step))))
            (when drawing
              (when (= (length current-dash) 0)
                (array/push current-dash (+ fx (* dx pos)))
                (array/push current-dash (+ fy (* dy pos))))
              (array/push current-dash px)
              (array/push current-dash py))
            (+= pos step)
            (-= pat-remain step)
            (when (<= pat-remain 0.0001)
              (when (and drawing (>= (length current-dash) 4))
                (draw-polyline current-dash color false thickness))
              (set current-dash @[])
              (set drawing (not drawing))
              (set pat-idx (% (+ pat-idx 1) (length pattern)))
              (set pat-remain (pattern pat-idx))))))
      (when (and drawing (>= (length current-dash) 4))
        (draw-polyline current-dash color false thickness)))))
)JANET")

    // --- Part 2: save/restore, basic drawing ---
    + R"JANET(
(defn ctx-save []
  (array/push state-stack
    @{:fill-style (ctx :fill-style) :stroke-style (ctx :stroke-style)
      :line-width (ctx :line-width) :global-alpha (ctx :global-alpha)
      :font (ctx :font) :font-size (ctx :font-size)
      :text-align (ctx :text-align) :text-baseline (ctx :text-baseline)
      :matrix (copy-array (ctx :matrix))
      :line-dash (copy-array (ctx :line-dash)) :line-dash-offset (ctx :line-dash-offset)
      :clip-count (ctx :clip-count)})
  (put ctx :clip-count 0))

(defn ctx-restore []
  (for i 0 (ctx :clip-count) (pop-clip-rect))
  (when (> (length state-stack) 0)
    (def prev (array/pop state-stack))
    (put ctx :fill-style (prev :fill-style))
    (put ctx :stroke-style (prev :stroke-style))
    (put ctx :line-width (prev :line-width))
    (put ctx :global-alpha (prev :global-alpha))
    (put ctx :font (prev :font))
    (put ctx :font-size (prev :font-size))
    (put ctx :text-align (prev :text-align))
    (put ctx :text-baseline (prev :text-baseline))
    (put ctx :matrix (prev :matrix))
    (put ctx :line-dash (prev :line-dash))
    (put ctx :line-dash-offset (prev :line-dash-offset))
    (put ctx :clip-count (prev :clip-count))))

(defn ctx-clear-rect [x y w h]
  (if (identity-or-translation?)
    (let [[px py] (tx x y)]
      (draw-rect-filled px py w h "#000000"))
    (let [[p1x p1y] (tx x y)
          [p2x p2y] (tx (+ x w) y)
          [p3x p3y] (tx (+ x w) (+ y h))
          [p4x p4y] (tx x (+ y h))]
      (draw-convex-poly-filled @[p1x p1y p2x p2y p3x p3y p4x p4y] "#000000"))))

(defn ctx-fill-rect [x y w h]
  (def c (apply-alpha (ctx :fill-style) (ctx :global-alpha)))
  (if (identity-or-translation?)
    (let [[px py] (tx x y)]
      (draw-rect-filled px py w h c))
    (let [[p1x p1y] (tx x y)
          [p2x p2y] (tx (+ x w) y)
          [p3x p3y] (tx (+ x w) (+ y h))
          [p4x p4y] (tx x (+ y h))]
      (draw-convex-poly-filled @[p1x p1y p2x p2y p3x p3y p4x p4y] c))))

(defn ctx-stroke-rect [x y w h]
  (def c (apply-alpha (ctx :stroke-style) (ctx :global-alpha)))
  (if (identity-or-translation?)
    (let [[px py] (tx x y)]
      (draw-rect px py w h c (ctx :line-width)))
    (let [[p1x p1y] (tx x y)
          [p2x p2y] (tx (+ x w) y)
          [p3x p3y] (tx (+ x w) (+ y h))
          [p4x p4y] (tx x (+ y h))]
      (def flat @[p1x p1y p2x p2y p3x p3y p4x p4y])
      (if (> (length (ctx :line-dash)) 0)
        (stroke-dashed-polyline flat true c (ctx :line-width) (ctx :line-dash) (ctx :line-dash-offset))
        (draw-polyline flat c true (ctx :line-width))))))

(defn ctx-fill-text [text x y]
  (def c (apply-alpha (ctx :fill-style) (ctx :global-alpha)))
  (var x-off 0)
  (var y-off 0)
  (when (not (or (= (ctx :text-align) "left") (= (ctx :text-align) "start")))
    (def met (measure-text text))
    (cond
      (= (ctx :text-align) "center") (set x-off (/ (- (met :width)) 2))
      (or (= (ctx :text-align) "right") (= (ctx :text-align) "end")) (set x-off (- (met :width)))))
  (when (not (or (= (ctx :text-baseline) "alphabetic") (= (ctx :text-baseline) "ideographic")))
    (def met2 (measure-text text))
    (cond
      (= (ctx :text-baseline) "middle") (set y-off (/ (- (met2 :height)) 2))
      (= (ctx :text-baseline) "bottom") (set y-off (- (met2 :height)))))
  (def [px py] (tx (+ x x-off) (+ y y-off)))
  (draw-text px py c text))
)JANET"

    // --- Part 3: path API, stroke, fill ---
    + R"JANET(
(defn ctx-begin-path []
  (set sub-paths @[])
  (set current-sub-path nil))

(defn ctx-move-to [x y]
  (when current-sub-path (array/push sub-paths current-sub-path))
  (set current-sub-path @{:points @[x y] :closed false}))

(defn ctx-line-to [x y]
  (if (nil? current-sub-path)
    (set current-sub-path @{:points @[x y] :closed false})
    (do
      (array/push (current-sub-path :points) x)
      (array/push (current-sub-path :points) y))))

(defn ctx-close-path []
  (when (and current-sub-path (>= (length (current-sub-path :points)) 4))
    (put current-sub-path :closed true)
    (array/push sub-paths current-sub-path)
    (def first-x ((current-sub-path :points) 0))
    (def first-y ((current-sub-path :points) 1))
    (set current-sub-path @{:points @[first-x first-y] :closed false})))

(defn ctx-arc [cx cy radius start-angle end-angle &opt ccw]
  (default ccw false)
  (when (< radius 0) (break))
  (def TWO-PI (* 2 math/pi))
  (def segs (max 12 (min 128 (math/ceil (* radius 0.5)))))
  (var start-a start-angle)
  (var end-a end-angle)
  (if ccw
    (while (>= end-a start-a) (-= end-a TWO-PI))
    (while (<= end-a start-a) (+= end-a TWO-PI)))
  (var sweep (- end-a start-a))
  (when (> (math/abs sweep) TWO-PI)
    (set sweep (if ccw (- TWO-PI) TWO-PI)))
  (def n (max 1 (math/ceil (* (/ (math/abs sweep) TWO-PI) segs))))
  (for i 0 (+ n 1)
    (def t (+ start-a (* sweep (/ i n))))
    (def px (+ cx (* (math/cos t) radius)))
    (def py (+ cy (* (math/sin t) radius)))
    (if (nil? current-sub-path)
      (set current-sub-path @{:points @[px py] :closed false})
      (do
        (array/push (current-sub-path :points) px)
        (array/push (current-sub-path :points) py)))))

(defn ctx-bezier-curve-to [cp1x cp1y cp2x cp2y x y]
  (when (nil? current-sub-path)
    (set current-sub-path @{:points @[cp1x cp1y] :closed false}))
  (def pts (current-sub-path :points))
  (def last-x (pts (- (length pts) 2)))
  (def last-y (pts (- (length pts) 1)))
  (for i 1 21
    (def t (/ i 20))
    (def u (- 1 t))
    (array/push pts (+ (* u u u last-x) (* 3 u u t cp1x) (* 3 u t t cp2x) (* t t t x)))
    (array/push pts (+ (* u u u last-y) (* 3 u u t cp1y) (* 3 u t t cp2y) (* t t t y)))))

(defn ctx-quadratic-curve-to [cpx cpy x y]
  (when (nil? current-sub-path)
    (set current-sub-path @{:points @[cpx cpy] :closed false}))
  (def pts (current-sub-path :points))
  (def last-x (pts (- (length pts) 2)))
  (def last-y (pts (- (length pts) 1)))
  (def cp1x (+ last-x (* (/ 2 3) (- cpx last-x))))
  (def cp1y (+ last-y (* (/ 2 3) (- cpy last-y))))
  (def cp2x (+ x (* (/ 2 3) (- cpx x))))
  (def cp2y (+ y (* (/ 2 3) (- cpy y))))
  (ctx-bezier-curve-to cp1x cp1y cp2x cp2y x y))

(defn ctx-rect [x y w h]
  (ctx-move-to x y)
  (ctx-line-to (+ x w) y)
  (ctx-line-to (+ x w) (+ y h))
  (ctx-line-to x (+ y h))
  (ctx-close-path))

(defn ctx-stroke []
  (def c (apply-alpha (ctx :stroke-style) (ctx :global-alpha)))
  (def all @[])
  (each sp sub-paths (array/push all sp))
  (when (and current-sub-path (>= (length (current-sub-path :points)) 4))
    (array/push all current-sub-path))
  (each sp all
    (when (>= (length (sp :points)) 4)
      (def tr @[])
      (var i 0)
      (while (< i (length (sp :points)))
        (def [px py] (tx ((sp :points) i) ((sp :points) (+ i 1))))
        (array/push tr px)
        (array/push tr py)
        (+= i 2))
      (if (> (length (ctx :line-dash)) 0)
        (stroke-dashed-polyline tr (sp :closed) c (ctx :line-width) (ctx :line-dash) (ctx :line-dash-offset))
        (draw-polyline tr c (sp :closed) (ctx :line-width))))))

(defn ctx-fill []
  (def c (apply-alpha (ctx :fill-style) (ctx :global-alpha)))
  (def all @[])
  (each sp sub-paths (array/push all sp))
  (when (and current-sub-path (>= (length (current-sub-path :points)) 6))
    (array/push all current-sub-path))
  (each sp all
    (when (>= (length (sp :points)) 6)
      (def flat @[])
      (var i 0)
      (while (< i (length (sp :points)))
        (def [px py] (tx ((sp :points) i) ((sp :points) (+ i 1))))
        (array/push flat px)
        (array/push flat py)
        (+= i 2))
      (draw-convex-poly-filled flat c))))
)JANET"

    // --- Part 4: transforms, text measurement, dashed lines, clip ---
    + R"JANET(
(defn ctx-translate [x y]
  (put ctx :matrix (multiply-matrix (ctx :matrix) @[1 0 0 1 x y])))

(defn ctx-rotate [angle]
  (def cos-a (math/cos angle))
  (def sin-a (math/sin angle))
  (put ctx :matrix (multiply-matrix (ctx :matrix) @[cos-a sin-a (- sin-a) cos-a 0 0])))

(defn ctx-scale [x y]
  (put ctx :matrix (multiply-matrix (ctx :matrix) @[x 0 0 y 0 0])))

(defn ctx-transform [a b c d e f]
  (put ctx :matrix (multiply-matrix (ctx :matrix) @[a b c d e f])))

(defn ctx-set-transform [a &opt b c d e f]
  (if (dictionary? a)
    (put ctx :matrix @[(or (a :a) (a :m11) 1) (or (a :b) (a :m12) 0)
                       (or (a :c) (a :m21) 0) (or (a :d) (a :m22) 1)
                       (or (a :e) (a :m41) 0) (or (a :f) (a :m42) 0)])
    (put ctx :matrix @[a b c d e f])))

(defn ctx-reset-transform []
  (put ctx :matrix @[1 0 0 1 0 0]))

(defn ctx-get-transform []
  (def m (ctx :matrix))
  {:a (m 0) :b (m 1) :c (m 2) :d (m 3) :e (m 4) :f (m 5)})

(defn ctx-measure-text [text]
  (def r (measure-text (string text)))
  {:width (r :width)})

(defn ctx-set-line-dash [segs]
  (when (not (indexed? segs)) (break))
  (each v segs
    (when (or (not (number? v)) (< v 0)) (break)))
  (put ctx :line-dash (copy-array segs)))

(defn ctx-get-line-dash []
  (copy-array (ctx :line-dash)))

(defn ctx-clip []
  (def all @[])
  (each sp sub-paths (array/push all sp))
  (when (and current-sub-path (>= (length (current-sub-path :points)) 2))
    (array/push all current-sub-path))
  (when (= (length all) 0) (break))
  (var min-x math/inf)
  (var min-y math/inf)
  (var max-x (- math/inf))
  (var max-y (- math/inf))
  (each sp all
    (def pts (sp :points))
    (var i 0)
    (while (< i (length pts))
      (def [px py] (tx (pts i) (pts (+ i 1))))
      (when (< px min-x) (set min-x px))
      (when (< py min-y) (set min-y py))
      (when (> px max-x) (set max-x px))
      (when (> py max-y) (set max-y py))
      (+= i 2)))
  (when (and (not= min-x math/inf) (not= min-y math/inf))
    (push-clip-rect min-x min-y max-x max-y)
    (put ctx :clip-count (+ (ctx :clip-count) 1))))
)JANET";
    return s;
}

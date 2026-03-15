var d = globalThis.data;
if (d && d.values) {
  var w = 400, h = 300;
  drawRectFilled(0, 0, w, h, '#16213e');
  drawText(10, 8, '#e0e0e0', 'Live Data (' + d.values.length + ' bars)');
  var barW = 35, gap = 8, startX = 15, maxH = 220;
  var maxVal = 0;
  for (var i = 0; i < d.values.length; i++) {
    if (d.values[i] > maxVal) maxVal = d.values[i];
  }
  if (maxVal === 0) maxVal = 1;
  for (var i = 0; i < d.values.length; i++) {
    var barH = (d.values[i] / maxVal) * maxH;
    var x = startX + i * (barW + gap);
    var y = h - 30 - barH;
    drawRectFilled(x, y, barW, barH, d.colors[i]);
    drawText(x + 4, h - 24, '#aaa', d.labels[i]);
  }
}

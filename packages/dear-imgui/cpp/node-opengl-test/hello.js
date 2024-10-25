const nodeImgui = require("./node-imgui");

nodeImgui.init();

let flag = true;
(function keepProcessRunning() {
  setTimeout(() => flag && keepProcessRunning(), 1000);
})();

// Emscripten 5.0.2 Browser.init installs pointer-lock listeners outside JSEvents
// and GLFW.terminate. Retain just those handles for this module's disposal.
addToLibrary({
    $xframesBrowserLifetime__deps: ['$Browser'],
    $xframesBrowserLifetime__postset: 'xframesBrowserLifetime.install();',
    $xframesBrowserLifetime: {
        listeners: [],
        install: function() {
            var initialize = Browser.init;
            Browser.init = function() {
                Browser.init = initialize;
                var targets = [document, Module['canvas']].filter(Boolean);
                var restore = targets.map(function(target) {
                    var descriptor = Object.getOwnPropertyDescriptor(target, 'addEventListener');
                    var add = target.addEventListener;
                    target.addEventListener = function(type, listener, options) {
                        if ((target === document && type === 'pointerlockchange')
                            || (target === Module['canvas'] && type === 'click')) {
                            if (xframesBrowserLifetime.listeners.length >= 2) throw new Error('Unexpected Browser.init listener growth');
                            xframesBrowserLifetime.listeners.push([target, type, listener, options]);
                        }
                        return add.call(target, type, listener, options);
                    };
                    return function() {
                        if (descriptor) Object.defineProperty(target, 'addEventListener', descriptor);
                        else delete target.addEventListener;
                    };
                });
                try { return initialize.call(Browser); }
                finally { restore.forEach(function(restoreTarget) { restoreTarget(); }); }
            };
        },
        dispose: function() {
            xframesBrowserLifetime.listeners.splice(0).forEach(function(record) {
                record[0].removeEventListener(record[1], record[2], record[3]);
            });
            if (document.pointerLockElement === Module['canvas']) document.exitPointerLock();
            Browser.pointerLock = false;
        },
    },
    xframes_browser_lifetime_dispose__deps: ['$xframesBrowserLifetime'],
    xframes_browser_lifetime_dispose: function() { xframesBrowserLifetime.dispose(); },
});

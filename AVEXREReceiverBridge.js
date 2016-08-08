// Based on xre-receiver project's code

var XREReceiverProto = Object.create(HTMLElement.prototype);
XREReceiverProto.Props = {};      // Blank local property array
XREReceiverProto.Signals = {};      // Blank local signals array

XREReceiverProto.sendToContainerApp = function (message) {
    console.log('To Receiver: "' + JSON.stringify(message) + '"');
    if (typeof window.wpeQuery !== 'undefined') {
        window.wpeQuery({
            request: JSON.stringify(message),
            persistent: false,
            onSuccess: function (response) { },
            onFailure: function (error_code, error_message) {
                console.log("sendToContainerApp Failure Error: " + error_message + " (" + error_code + ")");
            }
        });
    }
    else {
        console.log('XREReceiverObject: window.wpeQuery is not avaliable, skipping message : "'
                    + JSON.stringify(message) + '"');
    }
};
XREReceiverProto.emit = function (signal, params) {
    try {
        var args = Array.prototype.slice.call(arguments, 1);
        console.log("XREReceiverProto.emit " + signal + " params:" + params + " args:" + args);
        XREReceiverProto.Signals[signal].apply(this, args);
    }
    catch (err) {
        console.log("emit: signal exception -> " + signal + " error = " + err);
    }
}
XREReceiverProto.logMessage = function (message) {
    this.callNativeMethod('logMessage', message);
}
XREReceiverProto.callNativeMethod = function (method, args) {
    var cmd = {
        'cmd': method,
        'arg': args
    };
    this.sendToContainerApp(cmd);
};
XREReceiverProto.setNativeProperty = function (prop, value) {
    var cmd = {
        'cmd': 'set',
        'arg': [prop, value]
    };
    this.sendToContainerApp(cmd);
    this.Props[prop] = value;
};
XREReceiverProto.defineNativeMethod = function (method) {
    Object.defineProperty(this, method, {
        value: function () {
            var args = Array.prototype.slice.call(arguments, 0);
            this.callNativeMethod(method, args);
        }
    });
};
XREReceiverProto.defineNativeProperty = function (prop) {
    Object.defineProperty(this, prop, {
        get: function () {
            return this.Props[prop]; // for debug purpose only
        },
        set: function (value) {
            this.setNativeProperty(prop, value);
        }
    });
};
XREReceiverProto.defineSignal = function (signal) {
    Object.defineProperty(this, signal, {
        value: {
            connect: function (callback) {
                console.log("defineSignal adding callback " + callback + " for signal " + signal);
                XREReceiverProto.Signals[signal] = callback;
            }
        }
    });
}


XREReceiverProto.defineNativeMethod('onReady');
XREReceiverProto.defineNativeMethod('getRedirectionURL');
XREReceiverProto.defineNativeMethod('onEvent');
XREReceiverProto.defineSignal('gotRedirectionURL');
XREReceiverProto.defineSignal('callMethod');

var XREReceiver = Object.create(XREReceiverProto);

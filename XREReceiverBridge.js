/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
// Based on xre-receiver project's code

var XREReceiverProto = Object.create(HTMLElement.prototype);
XREReceiverProto.Props = {};      // Blank local property array
XREReceiverProto.Signals = {};      // Blank local signals array

XREReceiverProto.sendToContainerApp = function (message) {
    // console.log('To Receiver: "' + JSON.stringify(message) + '"');
    if (typeof window.wpeQuery !== 'undefined') {
        window.wpeQuery({
            request: JSON.stringify(message),
            persistent: false,
            onSuccess: null,
            onFailure: null
        });
    }
    else {
        console.error('XREReceiverObject: window.wpeQuery is not avaliable, skipping message : "'
                    + JSON.stringify(message) + '"');
    }
};
XREReceiverProto.emit = function (signal, params) {
    try {
        var args = Array.prototype.slice.call(arguments, 1);
        // console.log("XREReceiverProto.emit " + signal + " params:" + params + " args:" + args);
        XREReceiverProto.Signals[signal].apply(this, args);
    }
    catch (err) {
        console.error("emit: signal exception -> " + signal + " error = " + err);
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

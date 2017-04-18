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
/*******************************************************************************
  The JS Bridge consists of 2 parts:
    - execution backend (written in C++)
    - client interface to execution backend (this one)

  Each part consists of 2 layers:
   - object interface (methods calls)
   - general messaging (serialized messages)

  Each method call JS => C++ is serialized into JSON, trasfered via IPC,
  received on another side, mapped into the corresponding method call.
  So, the route is:

  C++                        JS
  -------------------       -------------------
  object interface  <      < object interface
  ------------------ |     |-------------------
  general messaging  |     | general messaging
  ------------------ |     |-------------------
  IPC                 <===<  IPC

  Responses and events are transferred in opposite direction (C++ => JS)

*******************************************************************************/

window.ServiceManager = {};

////////////////////////////////////////////////////////////////////////////////
// The function is used to generate methods for JS objects at runtime.
//
// This method will serialize its arguments into JSON and send over IPC
// to execution backend (C++).
// @param objectName By this name object has to be registered on backend.
// @param methodName Name of object's the method to execute.
//
window.ServiceManager.generateMethod = function (objectName, methodName)
{
    console.log("Generating method '" + objectName + "::" + methodName + "()");

    function generateObject(objectName)
    {
        var proxy = new Proxy(
            {
                _objectName: objectName
            },
            {
                get:
                    function (target, _methodName) {
                        var callMethod = window.ServiceManager.generateMethod(target._objectName, _methodName);
                        return function() {
                            callMethod.apply(this, arguments)
                        }
                    },
                set:
                    function (target, _methodName, _value) {
                        var callMethod = window.ServiceManager.generateMethod(target._objectName, _methodName);
                        callMethod(_value);
                        return true;
                    }
            });

        return proxy;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Forwards received response to 'passedCallback'.
    // If response is of object type, JS object with ability to call methods
    // is created.
    //
    function forwardResponseCallback(passedCallback)
    {
        return function (response)
        {
            var responseObj = JSON.parse(response);
            var result = responseObj.objectName ? generateObject(responseObj.objectName) : responseObj.value;
            passedCallback(result);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Prints received response to console
    //
    function dumpResponseCallback(description)
    {
        return function (response)
        {
            var responseStr = response;
            if (typeof response === 'object')
            {
                responseStr = JSON.stringify(response);
            }

            console.log("Error: " + description + ": " + responseStr);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // The function will be available in glogal context as 'objectName.methodName(...)'
    // The function packs list of params and sends them to execution backend.
    // Response is delivered into callback passed as last parameter or into
    // a generated dummy one.
    //
    return function()
    {
        var argv = [];
        var max_idx = arguments.length - 1;
        var onSuccess = dumpResponseCallback("Dummy success callback");
        var onFailure = dumpResponseCallback("Failure callback");
        if (typeof arguments[max_idx] === 'function')
        {
            onSuccess = arguments[max_idx];
            --max_idx;
        }

        for (var i = 0; i <= max_idx; ++i)
        {
            argv.push(arguments[i]);
        }

        message = JSON.stringify({
            'objectName': objectName,
            'methodName': methodName,
            'argv': argv
        });

        console.log(message);

        window.ServiceManager.sendQuery({
            request: message,
            onSuccess: forwardResponseCallback(onSuccess),
            onFailure: dumpResponseCallback("Failure callback")});
    }
}

// If exists used to define that window.ServiceManager uses async api.
window.ServiceManager.version = "2.0";
window.ServiceManager.getServiceForJavaScript = window.ServiceManager.generateMethod('ServiceManager', 'getServiceForJavaScript');

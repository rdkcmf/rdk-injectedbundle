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

////////////////////////////////////////////////////////////////////////////////
// UnPack/pack message to/from its object representation
//
ServiceManager.packMessage = function (objectName, methodName, argv)
{
    return JSON.stringify({'objectName': objectName,
                           'methodName': methodName,
                           'argv': argv});
}

ServiceManager.unpackMessage = function (message)
{
    return JSON.parse(message);
}

////////////////////////////////////////////////////////////////////////////////
// The function is used to generate methods for JS objects at runtime.
//
// This method will serialize its arguments into JSON and send over IPC
// to execution backend (C++).
// @param objectName By this name object has to be registered on backend.
// @param methodName Name of object's the method to execute.
//
ServiceManager.generateMethod = function (objectName, methodName)
{
    console.log("generating method '" + methodName  + "' for " + objectName);

    function generateObject(objectName)
    {
        var proxy = new Proxy(
            {
                objectName: objectName
            },
            {
                get:
                    function (target, methodName) {
                        return function () {
                            var callMethod = ServiceManager.generateMethod(target.objectName, methodName);
                            callMethod.apply(this, arguments)
                        }
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
            console.log(response);
            var responseObj = ServiceManager.unpackMessage(response);
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

            console.log("Failure: " + description + ": " + responseStr);
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
        var localCb = dumpResponseCallback("Dummy success callback");
        if (typeof arguments[max_idx] === 'function')
        {
            localCb = arguments[max_idx];
            --max_idx;
        }

        for (var i = 0; i <= max_idx; ++i)
        {
            argv.push(arguments[i]);
        }

        message = ServiceManager.packMessage(objectName, methodName, argv);
        console.log("Call from method '" + methodName + "': " + message);

        ServiceManager.sendQuery({
            request: message,
            onSuccess: forwardResponseCallback(localCb),
            onFailure: dumpResponseCallback("Failure callback")});
    }
};

// If exists used to define that ServiceManager uses async api.
ServiceManager.version = "2.0";
ServiceManager.getServiceForJavaScript = ServiceManager.generateMethod('ServiceManager', 'getServiceForJavaScript');
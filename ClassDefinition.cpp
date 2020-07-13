/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include "ClassDefinition.h"
#include <memory.h>
#include <algorithm>
#include <assert.h>
#include <JavaScriptCore/JSContextRef.h>
#include "logger.h"

namespace WPEFramework {
namespace JavaScript {

    // Constructor: uses identifier to build class and extension name.
    ClassDefinition::ClassDefinition(const std::string& identifier)
        : _customFunctions()
        , _className(identifier.c_str())
        , _extName(_className)
    {
        // Make upper case.
        transform(_className.begin(), _className.end(), _className.begin(), ::toupper);

        // Make lower case.
        transform(_extName.begin(), _extName.end(), _extName.begin(), ::tolower);
    }
    /* static */ ClassDefinition::ClassMap& ClassDefinition::getClassMap()
    {
        static ClassDefinition::ClassMap singleton;
        return singleton;
    }

    /* static */ ClassDefinition& ClassDefinition::Instance(const std::string& className)
    {
        ClassDefinition::ClassMap& _classes = getClassMap();
        ClassDefinition* result = nullptr;
        ClassMap::iterator index(_classes.find(className));

        if (index != _classes.end()) {
            result = &(index->second);
        } else {
            RDKLOG_INFO("Before Classdefinition ingest: %s", className.c_str());
            std::pair<ClassMap::iterator, bool> entry(_classes.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(className),
                std::forward_as_tuple(className)));
            RDKLOG_INFO("After Classdefinition ingest - 3: %s", className.c_str());

            result = &(entry.first->second);
        }

        return (*result);
    }

    // Adds JS function to class.
    void ClassDefinition::Add(const JavaScriptFunction* javaScriptFunction)
    {
        assert(std::find(_customFunctions.begin(), _customFunctions.end(), javaScriptFunction) == _customFunctions.end());

        _customFunctions.push_back(javaScriptFunction);
    }

    // Removes JS function from class.
    void ClassDefinition::Remove(const JavaScriptFunction* javaScriptFunction)
    {
        // Try to find function in class.
        FunctionVector::iterator index(find(_customFunctions.begin(), _customFunctions.end(), javaScriptFunction));

        assert(index != _customFunctions.end());

        if (index != _customFunctions.end()) {
            // Remove function from function vector.
            _customFunctions.erase(index);
        }
    }

    void ClassDefinition::InjectInJSWorld(ClassDefinition& classDef, JSGlobalContextRef context)
    {
        auto functions = classDef.GetFunctions();
        uint32_t functionCount = std::distance(functions.begin(), functions.end());

        // We need an extra entry that we set to all zeroes, to signal end of data.
        std::vector<JSStaticFunction> staticFunctions;
        staticFunctions.reserve(functionCount + 1);

        int index = 0;
        for(auto function : functions) {
            staticFunctions[index++] = function->BuildJSStaticFunction();
        }

        staticFunctions[functionCount] = { nullptr, nullptr, 0 };

        JSClassDefinition jsClassDefinition = {
            0, // version
            kJSClassAttributeNone, //attributes
            classDef.GetClassName().c_str(), // className
            0, // parentClass
            nullptr, // staticValues
            staticFunctions.data(), // staticFunctions
            nullptr, //initialize
            nullptr, //finalize
            nullptr, //hasProperty
            nullptr, //getProperty
            nullptr, //setProperty
            nullptr, //deleteProperty
            nullptr, //getPropertyNames
            nullptr, //callAsFunction
            nullptr, //callAsConstructor
            nullptr, //hasInstance
            nullptr, //convertToType
        };

        JSClassRef jsClass = JSClassCreate(&jsClassDefinition);
        JSValueRef jsObject = JSObjectMake(context, jsClass, nullptr);
        JSClassRelease(jsClass);

        // @Zan: can we make extension name same as ClassName?
        JSStringRef extensionString = JSStringCreateWithUTF8CString(classDef.GetExtName().c_str());
        JSObjectSetProperty(context, JSContextGetGlobalObject(context), extensionString, jsObject,
            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(extensionString);
    }

    void ClassDefinition::InjectJSClass(JSGlobalContextRef context)
    {
        // Add JS classes to JS world.
        auto& classMap  = getClassMap();
        for (auto& it : classMap) {
            ClassDefinition* result = nullptr;
            result = &(it.second);
            InjectInJSWorld(*result, context);
        }
    }

    void injectJSClass(JSGlobalContextRef context)
    {
        ClassDefinition::InjectJSClass(context);
    }
}
}

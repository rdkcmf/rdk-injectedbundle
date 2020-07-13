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

#ifndef __JAVASCRIPTFUNCTION_H
#define __JAVASCRIPTFUNCTION_H

#include <cassert>
#include <memory>
#include <string.h>

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>

namespace WPEFramework {
namespace JavaScript {

    class JavaScriptFunction {
    public:
        JSStaticFunction BuildJSStaticFunction() const;

        std::string GetName() const
        {
            return Name;
        }

        virtual ~JavaScriptFunction() {}

    protected:
        JavaScriptFunction(const std::string& name, const JSObjectCallAsFunctionCallback callback, bool shouldNotEnum = false);

    private:
        JavaScriptFunction() = delete;

    private:
        std::string Name;
        const JSObjectCallAsFunctionCallback Callback;
        bool ShouldNotEnum;

        // Temporary string for "char *" version of Name, required by JS API.
        std::string NameString;
    };
} // namespace JavaScript
} // namespace WPEFramework

#endif // __JAVASCRIPTFUNCTION_H

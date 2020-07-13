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

#ifndef __CUSTOMCLASS_H
#define __CUSTOMCLASS_H


#include "JavaScriptFunction.h"

#include <vector>
#include <map>

namespace WPEFramework {
namespace JavaScript {

    class ClassDefinition {
    private:
        ClassDefinition() = delete;
        ClassDefinition(const ClassDefinition& copy) = delete;
        ClassDefinition& operator=(const ClassDefinition& copy) = delete;

        typedef std::map<std::string, ClassDefinition> ClassMap;
        typedef std::vector<const JavaScriptFunction*> FunctionVector;

    public:

        // These are the only viable constructor, but only via the static creation method !!!
        ClassDefinition(const std::string& identifier);
        ~ClassDefinition() {}

    public:
        static ClassMap& getClassMap();
        static void InjectJSClass(JSGlobalContextRef context);
        static void InjectInJSWorld(ClassDefinition& classDef, JSGlobalContextRef context);
        static ClassDefinition& Instance(const std::string& className);

        void Add(const JavaScriptFunction* javaScriptFunction);
        void Remove(const JavaScriptFunction* javaScriptFunction);

    private:
        const FunctionVector& GetFunctions()
        {
            return _customFunctions;
        }
        const std::string& GetClassName() const
        {
            return (_className);
        }
        const std::string& GetExtName() const
        {
            return (_extName);
        }

        FunctionVector _customFunctions;

        // JavaScript class properties.
        std::string _className;
        std::string _extName;

        // Instance declared in main.cpp, as this needs to be initialized before
        // a static in this cpp unit is called!!!
        //static ClassMap _classes;
    };

    void injectJSClass(JSGlobalContextRef context);
}
}
#endif // __CUSTOMCLASS_H

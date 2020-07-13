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
#ifndef UTILS_H
#define UTILS_H

#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKString.h>
#include <memory>
#include <string>
#include <fstream>


namespace Utils
{

/**
 * Converts WKStringRef to std::string
 */
static inline std::string toStdString(WKStringRef string)
{
    size_t size = WKStringGetMaximumUTF8CStringSize(string);
    auto buffer = std::make_unique<char[]>(size);
    size_t len = WKStringGetUTF8CString(string, buffer.get(), size);

    return len ? std::string(buffer.get(), len - 1) : "";
}

/**
 * Reads content of the file.
 * @return true If success.
 */
static inline bool readFile(const char* path, std::string& result)
{
    std::ifstream file;
    file.open(path);
    if (!file.is_open())
        return false;

    result.assign((std::istreambuf_iterator<char>(file)),(std::istreambuf_iterator<char>()));
    return true;
}

/**
 * Evaluates user script in specific JavaScript context.
 * @return Result from JavaScript.
 */
static inline JSValueRef evaluateUserScript(JSContextRef context, const std::string& script, JSValueRef* exc)
{
    JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString(script.c_str()));

    return JSEvaluateScript(context, str.get(), nullptr, nullptr, 0, exc);
}

std::string GetURL();

} // namespace Utils

#endif // UTILS_H


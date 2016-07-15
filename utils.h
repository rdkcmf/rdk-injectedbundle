#ifndef JSBRIDGE_UTILS_H
#define JSBRIDGE_UTILS_H

#include <JavaScriptCore/JSRetainPtr.h>
#include <string>
#include <fstream>

namespace JSBridge
{

namespace Utils
{

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

} // namespace Utils

} // namespace JSBridge

#endif // JSBRIDGE_UTILS_H


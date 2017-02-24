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

} // namespace Utils

#endif // UTILS_H


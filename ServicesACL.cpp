#include "ServicesACL.h"
#include "utils.h"

namespace JSBridge
{

namespace
{

// Escapes some characters to avoid JS injection.
std::string escape(const std::string& str)
{
    std::string result;
    std::for_each(str.begin(), str.end(),
        [&result](char ch)
        {
            if (ch == '\'' || ch == '\\')
            {
                result.push_back('\\');
            }
            result.push_back(ch);
        }
    );

    return result;
}

} // namespace

ServicesACL::ServicesACL()
    : m_active(false)
    , m_jsContextRef(adopt(JSGlobalContextCreate(nullptr)))
{
    const char* jsFile = "/usr/share/injectedbundle/ServicesACL.js";
    if (!Utils::readFile(jsFile, m_jsACL))
    {
        fprintf(stderr, "%s:%d Error: Could not read file '%s'.\n", __func__, __LINE__, jsFile);
        return;
    }

    JSValueRef exc = 0;
    Utils::evaluateUserScript(m_jsContextRef.get(), m_jsACL, &exc);
    if (exc)
    {
        fprintf(stderr, "%s:%d Error: Could not evaluate user script = '%s'\n", __func__, __LINE__, jsFile);
    }
}

bool ServicesACL::set(const std::string& acl)
{
    if (acl.empty() || acl == "{}")
    {
        return true;
    }

    m_active = true;
    return evaluate("acl.set('" + escape(acl) + "');");
}

bool ServicesACL::isServiceAllowed(const std::string& serviceName, const std::string& url) const
{
    return evaluate("acl.isServiceAllowed('" + escape(serviceName) + "', '" + escape(url) + "');");
}

bool ServicesACL::isServiceManagerAllowed(const std::string& url) const
{
    return evaluate("acl.isServiceManagerAllowed('" + escape(url) + "');");
}

bool ServicesACL::evaluate(const std::string& js) const
{
    if (!m_active)
        return true;

    JSValueRef exc = 0;
    JSValueRef res = Utils::evaluateUserScript(m_jsContextRef.get(), js, &exc);
    if (exc)
    {
        fprintf(stderr, "%s:%d Error: Could not evaluate user script = '%s'\n", __func__, __LINE__, js.c_str());
        return false;
    }

    if (!JSValueIsBoolean(m_jsContextRef.get(), res))
    {
        fprintf(stderr, "%s:%d Error: Wrong return type from JavaScript function\n", __func__, __LINE__);
        return false;
    }

    return JSValueToBoolean(m_jsContextRef.get(), res);
}

void ServicesACL::clear()
{
    m_active = false;
}

} // namespace JSBridge
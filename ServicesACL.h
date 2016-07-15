#ifndef JSBRIDGE_ACL_H
#define JSBRIDGE_ACL_H

#include <JavaScriptCore/JSRetainPtr.h>
#include <string>

namespace JSBridge
{

/**
 * Implements Access Control List to limit
 * access to ServiceManager services from javascript.
 */
class ServicesACL
{
public:

    /**
     * Default ctor.
     */
    ServicesACL();

    /**
     * Set ACL and replaces old one.
     */
    bool set(const std::string& acl);

    /**
     * Checks if url there is a service that allowed for this url.
     */
    bool isServiceManagerAllowed(const std::string& url) const;

    /**
     * Checks if service is allowed by url.
     */
    bool isServiceAllowed(const std::string& serviceName, const std::string& url) const;

    /**
     * Clears current ACL.
     */
     void clear();

private:

    /**
     * Evaluates JavaScript
     */
    bool evaluate(const std::string& js) const;

    /**
     * Custom JavaScript context to run js.
     */
    JSRetainPtr<JSGlobalContextRef> m_jsContextRef;

    /**
     * JavaScript implementation of ACL.
     */
    std::string m_jsACL;

    /**
     * Defines if ACL is valid and can be used.
     */
    bool m_active;
};

} // namespace JSBridge

#endif // JSBRIDGE_ACL_H


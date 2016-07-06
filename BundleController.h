#ifndef JSBRIDGE_BUNDLE_CONTROLLER_H
#define JSBRIDGE_BUNDLE_CONTROLLER_H

#include <WebKit/WKBundle.h>

namespace JSBridge
{

/**
 * Initializes bundle, injects proper JavaScripts wrappers,
 * installs wpeQuery window function.
 * @param Bundle.
 * @param Initialization user data.
 */
void initialize(WKBundleRef, WKTypeRef);

} // namespace BundleController

#endif // JSBRIDGE_BUNDLE_CONTROLLER_H

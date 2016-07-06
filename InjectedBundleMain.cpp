#include "BundleController.h"
#include <WebKit/WKBundleInitialize.h>

extern "C" void WKBundleInitialize(WKBundleRef bundle, WKTypeRef initializationUserData)
{
    JSBridge::initialize(bundle, initializationUserData);
}

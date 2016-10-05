#include "BundleController.h"
#include "AVESupport.h"
#include "logger.h"

#include <WebKit/WKBundleInitialize.h>

#include <stdlib.h>
#include <stdio.h>

extern "C" void WKBundleInitialize(WKBundleRef bundle, WKTypeRef initializationUserData)
{
    RDK::logger_init();

    JSBridge::initialize(bundle, initializationUserData);
    AVESupport::initialize();
}

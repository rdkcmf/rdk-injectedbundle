#include "BundleController.h"
#include "AVESupport.h"

#include <WebKit/WKBundleInitialize.h>

#include <stdlib.h>
#include <stdio.h>

extern "C" void WKBundleInitialize(WKBundleRef bundle, WKTypeRef initializationUserData)
{
    if (getenv("SYNC_STDOUT"))
        setvbuf(stdout, NULL, _IOLBF, 0);

    JSBridge::initialize(bundle, initializationUserData);
    AVESupport::initialize();
}

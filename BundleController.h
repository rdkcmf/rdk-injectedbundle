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

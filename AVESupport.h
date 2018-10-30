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
#ifndef AVESUPPORT_H
#define AVESUPPORT_H

namespace AVESupport
{

void initialize();

void enable(bool on);

bool enabled();

void didCreatePage(WKBundlePageRef page);

void didCommitLoad(WKBundlePageRef page, WKBundleFrameRef frame);

void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame);

bool didReceiveMessageToPage(WKStringRef messageName, WKTypeRef messageBody);

void setClient(WKBundlePageRef bundle);

};

#endif // AVESUPPORT_H

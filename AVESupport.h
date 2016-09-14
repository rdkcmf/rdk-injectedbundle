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

};

#endif // AVESUPPORT_H
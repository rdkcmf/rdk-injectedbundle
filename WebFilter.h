#ifndef WEBFILTER_H
#define WEBFILTER_H

#include <WebKit/WKBundlePage.h>
#include <WebKit/WKURLRequest.h>

void addWebFiltersForPage(WKBundlePageRef, WKTypeRef);

void removeWebFiltersForPage(WKBundlePageRef);

bool filterRequest(WKBundlePageRef, WKURLRequestRef);

#endif

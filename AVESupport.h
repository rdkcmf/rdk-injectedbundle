#ifndef AVESUPPORT_H
#define AVESUPPORT_H

namespace AVESupport
{

void initialize();

void enable(bool on);

bool enabled();

void onCreatePage(WKBundlePageRef page);

};

#endif // AVESUPPORT_H
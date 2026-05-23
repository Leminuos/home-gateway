#ifndef __NETWORK_INFO_H__
#define __NETWORK_INFO_H__

#include <QString>

class NetworkInfo {
    public:
        static QString readMacAddress(const QString &iface);
};

#endif // __NETWORK_INFO_H__

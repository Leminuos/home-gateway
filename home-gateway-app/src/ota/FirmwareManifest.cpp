#include "FirmwareManifest.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

FirmwareManifest FirmwareManifest::fromJson(const QByteArray &json)
{
    FirmwareManifest m;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return m; // invalid
    }

    const QJsonObject obj = doc.object();
    m.version = obj.value("version").toString();
    m.url     = obj.value("url").toString();
    m.sha256  = obj.value("sha256").toString().toLower();
    m.size    = static_cast<qint64>(obj.value("size").toDouble(0));

    return m;
}

int FirmwareManifest::compareVersion(const QString &a, const QString &b)
{
    auto normalize = [](const QString &s) -> QString {
        QString t = s.trimmed();
        if (t.startsWith('v') || t.startsWith('V')) {
            t.remove(0, 1);
        }
        return t;
    };

    const QStringList pa = normalize(a).split('.');
    const QStringList pb = normalize(b).split('.');
    const int n = qMax(pa.size(), pb.size());

    for (int i = 0; i < n; ++i) {
        // Phần số đầu mỗi thành phần (vd "3-broken" -> 3).
        auto numAt = [](const QStringList &parts, int idx) -> int {
            if (idx >= parts.size()) return 0;
            const QString &p = parts.at(idx);
            int v = 0;
            int j = 0;
            while (j < p.size() && p.at(j).isDigit()) {
                v = v * 10 + (p.at(j).digitValue());
                ++j;
            }
            return v;
        };

        const int va = numAt(pa, i);
        const int vb = numAt(pb, i);
        if (va != vb) {
            return va < vb ? -1 : 1;
        }
    }
    return 0;
}

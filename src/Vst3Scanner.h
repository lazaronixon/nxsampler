#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct PluginInfo
{
    QString name;
    QString vendor;
    QString path;     // .vst3 bundle
    QString classId;  // VST3 class UID as a string

    QString displayName() const;
};

namespace Vst3Scanner {

// Loads every VST3 bundle in the standard macOS plug-in folders and returns the
// classes that are instruments. Bundles that fail to load are reported in `errors`.
QList<PluginInfo> scan(QStringList* errors = nullptr);

// QSettings cache, so the slow scan only runs when the user asks for it.
QList<PluginInfo> loadCached();
void saveCache(const QList<PluginInfo>& plugins);

} // namespace Vst3Scanner

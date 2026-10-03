#include "Vst3Scanner.h"

#include "public.sdk/source/vst/hosting/module.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

#include <QSettings>

#include <algorithm>

QString PluginInfo::displayName() const
{
    return vendor.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, vendor);
}

namespace Vst3Scanner {

namespace {

bool isInstrument(const VST3::Hosting::ClassInfo& info)
{
    if (info.category() != kVstAudioEffectClass)
        return false;
    const auto& subs = info.subCategories();
    return std::any_of(subs.begin(), subs.end(), [](const std::string& sub) {
        return sub == Steinberg::Vst::PlugType::kInstrument;
    });
}

} // namespace

QList<PluginInfo> scan(QStringList* errors)
{
    QList<PluginInfo> result;

    for (const auto& path : VST3::Hosting::Module::getModulePaths())
    {
        std::string error;
        auto module = VST3::Hosting::Module::create(path, error);
        if (!module)
        {
            if (errors)
                errors->append(QStringLiteral("%1: %2").arg(QString::fromStdString(path),
                                                            QString::fromStdString(error)));
            continue;
        }

        const auto factoryVendor = QString::fromStdString(module->getFactory().info().vendor());
        for (const auto& info : module->getFactory().classInfos())
        {
            if (!isInstrument(info))
                continue;

            PluginInfo plugin;
            plugin.name = QString::fromStdString(info.name());
            plugin.vendor = info.vendor().empty() ? factoryVendor
                                                  : QString::fromStdString(info.vendor());
            plugin.path = QString::fromStdString(path);
            plugin.classId = QString::fromStdString(info.ID().toString());
            result.append(plugin);
        }
    }

    std::sort(result.begin(), result.end(), [](const PluginInfo& a, const PluginInfo& b) {
        return a.displayName().compare(b.displayName(), Qt::CaseInsensitive) < 0;
    });
    return result;
}

QList<PluginInfo> loadCached()
{
    QSettings settings;
    QList<PluginInfo> plugins;
    const int count = settings.beginReadArray("plugins");
    for (int i = 0; i < count; ++i)
    {
        settings.setArrayIndex(i);
        PluginInfo plugin;
        plugin.name = settings.value("name").toString();
        plugin.vendor = settings.value("vendor").toString();
        plugin.path = settings.value("path").toString();
        plugin.classId = settings.value("classId").toString();
        plugins.append(plugin);
    }
    settings.endArray();
    return plugins;
}

void saveCache(const QList<PluginInfo>& plugins)
{
    QSettings settings;
    settings.remove("plugins");
    settings.beginWriteArray("plugins", static_cast<int>(plugins.size()));
    for (int i = 0; i < plugins.size(); ++i)
    {
        settings.setArrayIndex(i);
        settings.setValue("name", plugins[i].name);
        settings.setValue("vendor", plugins[i].vendor);
        settings.setValue("path", plugins[i].path);
        settings.setValue("classId", plugins[i].classId);
    }
    settings.endArray();
    settings.setValue("pluginsScanned", true);
}

} // namespace Vst3Scanner

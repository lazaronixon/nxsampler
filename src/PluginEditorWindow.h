#pragma once

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <QWidget>

// Top-level window that embeds a plugin's own editor (for example Kontakt's GUI).
class PluginEditorWindow : public QWidget, public Steinberg::IPlugFrame
{
    Q_OBJECT

public:
    // Returns nullptr and fills `error` when the plugin has no usable editor.
    static PluginEditorWindow* create(Steinberg::Vst::IEditController* controller,
                                      const QString& title, QString* error);
    ~PluginEditorWindow() override;

    // IPlugFrame
    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view,
                                             Steinberg::ViewRect* newSize) override;

    // FUnknown. The window's lifetime is managed by Qt, so reference counting is a no-op.
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    explicit PluginEditorWindow(Steinberg::IPtr<Steinberg::IPlugView> view);
    bool attachView(QString* error);
    void detachView();

    Steinberg::IPtr<Steinberg::IPlugView> view;
    bool resizingFromPlugin = false;
};

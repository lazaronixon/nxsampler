#include "PluginEditorWindow.h"

#include <QCloseEvent>
#include <QResizeEvent>

using namespace Steinberg;

PluginEditorWindow* PluginEditorWindow::create(Vst::IEditController* controller,
                                               const QString& title, QString* error)
{
    if (!controller)
    {
        if (error)
            *error = QStringLiteral("This instrument has no editor.");
        return nullptr;
    }

    IPtr<IPlugView> view = owned(controller->createView(Vst::ViewType::kEditor));
    if (!view || view->isPlatformTypeSupported(kPlatformTypeNSView) != kResultTrue)
    {
        if (error)
            *error = QStringLiteral("This instrument has no editor that can be shown on macOS.");
        return nullptr;
    }

    auto* window = new PluginEditorWindow(view);
    window->setWindowTitle(title);
    if (!window->attachView(error))
    {
        delete window;
        return nullptr;
    }
    return window;
}

PluginEditorWindow::PluginEditorWindow(IPtr<IPlugView> plugView)
: QWidget(nullptr, Qt::Window), view(std::move(plugView))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_NativeWindow);
}

PluginEditorWindow::~PluginEditorWindow()
{
    detachView();
}

bool PluginEditorWindow::attachView(QString* error)
{
    view->setFrame(this);

    ViewRect rect;
    if (view->getSize(&rect) == kResultTrue)
        resize(rect.getWidth(), rect.getHeight());
    if (view->canResize() != kResultTrue)
        setFixedSize(size());

    // On macOS the native handle of a QWidget is its NSView.
    if (view->attached(reinterpret_cast<void*>(winId()), kPlatformTypeNSView) != kResultTrue)
    {
        view->setFrame(nullptr);
        view = nullptr;
        if (error)
            *error = QStringLiteral("The instrument's editor could not be opened.");
        return false;
    }
    return true;
}

void PluginEditorWindow::detachView()
{
    if (!view)
        return;
    view->removed();
    view->setFrame(nullptr);
    view = nullptr;
}

tresult PLUGIN_API PluginEditorWindow::resizeView(IPlugView* plugView, ViewRect* newSize)
{
    if (!plugView || !newSize || plugView != view.get())
        return kInvalidArgument;

    resizingFromPlugin = true;
    const QSize target(newSize->getWidth(), newSize->getHeight());
    if (view->canResize() != kResultTrue)
        setFixedSize(target);
    else
        resize(target);
    resizingFromPlugin = false;

    plugView->onSize(newSize);
    return kResultTrue;
}

tresult PLUGIN_API PluginEditorWindow::queryInterface(const TUID iid, void** obj)
{
    if (FUnknownPrivate::iidEqual(iid, IPlugFrame::iid) ||
        FUnknownPrivate::iidEqual(iid, FUnknown::iid))
    {
        *obj = static_cast<IPlugFrame*>(this);
        return kResultTrue;
    }
    *obj = nullptr;
    return kNoInterface;
}

void PluginEditorWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!view || resizingFromPlugin || view->canResize() != kResultTrue)
        return;

    ViewRect rect(0, 0, event->size().width(), event->size().height());
    view->checkSizeConstraint(&rect);
    if (rect.getWidth() != width() || rect.getHeight() != height())
    {
        resizingFromPlugin = true;
        resize(rect.getWidth(), rect.getHeight());
        resizingFromPlugin = false;
    }
    view->onSize(&rect);
}

void PluginEditorWindow::closeEvent(QCloseEvent* event)
{
    detachView();
    QWidget::closeEvent(event);
}

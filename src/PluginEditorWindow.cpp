#include "PluginEditorWindow.h"

#include <QCloseEvent>
#include <QResizeEvent>

using namespace Steinberg;

namespace {

// The native handle a QWidget's winId() returns on each platform.
#if defined(Q_OS_MACOS)
const FIDString kPlatformType = kPlatformTypeNSView;
#elif defined(Q_OS_WIN)
const FIDString kPlatformType = kPlatformTypeHWND;
#else
const FIDString kPlatformType = kPlatformTypeX11EmbedWindowID;
#endif

} // namespace

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
    if (!view || view->isPlatformTypeSupported(kPlatformType) != kResultTrue)
    {
        if (error)
            *error = QStringLiteral("This instrument has no editor that can be shown on this system.");
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
        resize(fromPlugin(rect));
    if (view->canResize() != kResultTrue)
        setFixedSize(size());

    // winId() is the native window: NSView on macOS, HWND on Windows.
    if (view->attached(reinterpret_cast<void*>(winId()), kPlatformType) != kResultTrue)
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
    const QSize target = fromPlugin(*newSize);
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

    ViewRect rect = toPlugin(event->size());
    view->checkSizeConstraint(&rect);
    const QSize constrained = fromPlugin(rect);
    if (constrained != size())
    {
        resizingFromPlugin = true;
        resize(constrained);
        resizingFromPlugin = false;
    }
    view->onSize(&rect);
}

double PluginEditorWindow::pluginScale() const
{
    // VST3 editors use points on macOS but physical pixels on Windows and Linux.
#if defined(Q_OS_MACOS)
    return 1.0;
#else
    return devicePixelRatioF();
#endif
}

QSize PluginEditorWindow::fromPlugin(const ViewRect& rect) const
{
    const double scale = pluginScale();
    return {qRound(rect.getWidth() / scale), qRound(rect.getHeight() / scale)};
}

ViewRect PluginEditorWindow::toPlugin(const QSize& size) const
{
    const double scale = pluginScale();
    return ViewRect(0, 0, qRound(size.width() * scale), qRound(size.height() * scale));
}

void PluginEditorWindow::closeEvent(QCloseEvent* event)
{
    detachView();
    QWidget::closeEvent(event);
}

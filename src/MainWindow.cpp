#include "MainWindow.h"

#include "ExtractWorker.h"
#include "KeyboardWidget.h"
#include "PluginEditorWindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const int kSampleRates[] = {11025, 22050, 32000, 44100, 48000};

struct Dynamic
{
    const char* symbol;
    int velocity; // typical MIDI velocity for this marking
};

const Dynamic kDynamics[] = {
    {"ppp", 16}, {"pp", 32}, {"p", 48},   {"mp", 64},
    {"mf", 80},  {"f", 96},  {"ff", 112}, {"fff", 127},
};

constexpr int kDefaultVelocity = 96; // f

class WaitCursor
{
public:
    WaitCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~WaitCursor() { QApplication::restoreOverrideCursor(); }
};

} // namespace

MainWindow::MainWindow()
: host(std::make_unique<Vst3Host>())
{
    setWindowTitle(QStringLiteral("NXSampler"));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(buildInstrumentBox());
    layout->addWidget(buildKeyboardBox());
    layout->addWidget(buildSettingsBox());
    layout->addWidget(buildActionRow());
    setCentralWidget(central);

    applyDefaults();

    plugins = Vst3Scanner::loadCached();
    populatePlugins(plugins);
    if (!QSettings().value("pluginsScanned").toBool())
        QTimer::singleShot(0, this, &MainWindow::rescanPlugins);

    updateState();

    // Start the keyboard around middle C.
    QTimer::singleShot(0, this, [this] {
        keyboardScroll->horizontalScrollBar()->setValue(
            keyboard->keyCenterX(60) - keyboardScroll->viewport()->width() / 2);
    });
}

MainWindow::~MainWindow()
{
    if (editor)
        delete editor.data();
    stopMonitor();
    host->unload();
}

void MainWindow::startMonitor()
{
    if (!host->isLoaded() || monitor.isRunning())
        return;

    QString error;
    if (!monitor.isAvailable())
        error = monitor.errorString();
    else if (host->prepare(monitor.sampleRate(), Vst3Host::Mode::Realtime, &error))
        monitor.start(host.get());

    if (!error.isEmpty())
        pluginStatus->setText(tr("Loaded: %1. No live audio: %2").arg(host->plugin().displayName(), error));
}

void MainWindow::stopMonitor()
{
    monitor.stop();
}

QWidget* MainWindow::buildInstrumentBox()
{
    auto* box = new QGroupBox(tr("Instrument (VST3)"));
    auto* layout = new QGridLayout(box);

    pluginCombo = new QComboBox;
    pluginCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    pluginCombo->setMinimumContentsLength(30);
    loadButton = new QPushButton(tr("Load"));
    editorButton = new QPushButton(tr("Open Editor"));
    rescanButton = new QPushButton(tr("Rescan"));
    pluginStatus = new QLabel(tr("No instrument loaded."));

    layout->addWidget(pluginCombo, 0, 0);
    layout->addWidget(loadButton, 0, 1);
    layout->addWidget(editorButton, 0, 2);
    layout->addWidget(rescanButton, 0, 3);
    layout->addWidget(pluginStatus, 1, 0, 1, 4);
    layout->setColumnStretch(0, 1);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadSelectedPlugin);
    connect(editorButton, &QPushButton::clicked, this, &MainWindow::openEditor);
    connect(rescanButton, &QPushButton::clicked, this, &MainWindow::rescanPlugins);
    return box;
}

QWidget* MainWindow::buildKeyboardBox()
{
    auto* box = new QGroupBox(tr("Keys to sample"));
    auto* layout = new QVBoxLayout(box);

    keyboard = new KeyboardWidget;
    keyboardScroll = new QScrollArea;
    keyboardScroll->setWidget(keyboard);
    keyboardScroll->setWidgetResizable(false);
    keyboardScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    keyboardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    keyboardScroll->setFixedHeight(keyboard->sizeHint().height() +
                                   keyboardScroll->horizontalScrollBar()->sizeHint().height() + 4);
    layout->addWidget(keyboardScroll);

    auto* row = new QHBoxLayout;
    auto* selectAll = new QPushButton(tr("Select All"));
    auto* clear = new QPushButton(tr("Clear"));
    selectionLabel = new QLabel;
    auto* hint = new QLabel(tr("Click to toggle · drag to paint · Shift+click for a range"));
    hint->setEnabled(false);
    row->addWidget(selectAll);
    row->addWidget(clear);
    row->addWidget(selectionLabel);
    row->addStretch();
    row->addWidget(hint);
    layout->addLayout(row);

    connect(selectAll, &QPushButton::clicked, keyboard, &KeyboardWidget::selectAll);
    connect(clear, &QPushButton::clicked, keyboard, &KeyboardWidget::clearSelection);
    connect(keyboard, &KeyboardWidget::selectionChanged, this, &MainWindow::updateState);
    return box;
}

QWidget* MainWindow::buildSettingsBox()
{
    auto* box = new QGroupBox(tr("Sample settings"));
    auto* columns = new QHBoxLayout(box);
    auto* left = new QFormLayout;
    auto* right = new QFormLayout;
    columns->addLayout(left);
    columns->addSpacing(24);
    columns->addLayout(right);

    dynamicsCombo = new QComboBox;
    for (const Dynamic& d : kDynamics)
        dynamicsCombo->addItem(QStringLiteral("%1  (velocity %2)").arg(QLatin1String(d.symbol)).arg(d.velocity),
                               d.velocity);
    dynamicsCombo->setToolTip(tr("MIDI velocity each note is played with."));
    left->addRow(tr("Dynamics:"), dynamicsCombo);

    durationSpin = new QDoubleSpinBox;
    durationSpin->setRange(0.05, 60.0);
    durationSpin->setDecimals(2);
    durationSpin->setSingleStep(0.1);
    durationSpin->setSuffix(tr(" s"));
    durationSpin->setToolTip(tr("The note is held for this long; the file is cut at exactly this length."));
    left->addRow(tr("Duration:"), durationSpin);

    auto makeChoice = [](QButtonGroup*& group, const QList<QPair<QString, int>>& options) {
        auto* widget = new QWidget;
        auto* row = new QHBoxLayout(widget);
        row->setContentsMargins(0, 0, 0, 0);
        group = new QButtonGroup(widget);
        for (const auto& [label, id] : options)
        {
            auto* radio = new QRadioButton(label);
            group->addButton(radio, id);
            row->addWidget(radio);
        }
        row->addStretch();
        return widget;
    };
    left->addRow(tr("Channels:"), makeChoice(channelsGroup, {{tr("Mono"), 1}, {tr("Stereo"), 2}}));
    left->addRow(tr("Bit depth:"), makeChoice(bitsGroup, {{tr("8-bit"), 8}, {tr("16-bit"), 16}}));

    sampleRateCombo = new QComboBox;
    for (int rate : kSampleRates)
        sampleRateCombo->addItem(QStringLiteral("%1 Hz").arg(rate), rate);
    right->addRow(tr("Sample rate:"), sampleRateCombo);

    normalizeCheck = new QCheckBox;
    right->addRow(tr("Normalize:"), normalizeCheck);

    nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QStringLiteral("RealStrF"));
    nameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_\\-]{1,40}")), nameEdit));
    right->addRow(tr("Name:"), nameEdit);

    auto* folderRow = new QWidget;
    auto* folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderEdit = new QLineEdit;
    auto* browse = new QPushButton(tr("Browse…"));
    folderLayout->addWidget(folderEdit);
    folderLayout->addWidget(browse);
    right->addRow(tr("Output folder:"), folderRow);

    exampleLabel = new QLabel;
    exampleLabel->setEnabled(false);
    right->addRow(QString(), exampleLabel);

    connect(browse, &QPushButton::clicked, this, &MainWindow::browseFolder);
    connect(nameEdit, &QLineEdit::textChanged, this, &MainWindow::updateState);
    connect(folderEdit, &QLineEdit::textChanged, this, &MainWindow::updateState);
    return box;
}

QWidget* MainWindow::buildActionRow()
{
    auto* widget = new QWidget;
    auto* row = new QHBoxLayout(widget);
    row->setContentsMargins(0, 0, 0, 0);

    progressBar = new QProgressBar;
    progressBar->setRange(0, 1);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    progressLabel = new QLabel;
    extractButton = new QPushButton(tr("Extract"));
    extractButton->setDefault(true);
    extractButton->setMinimumWidth(120);

    row->addWidget(progressBar, 1);
    row->addWidget(progressLabel);
    row->addWidget(extractButton);

    connect(extractButton, &QPushButton::clicked, this, [this] {
        if (extracting)
        {
            if (worker)
                worker->cancel();
            extractButton->setEnabled(false);
            progressLabel->setText(tr("Cancelling…"));
        }
        else
        {
            startExtraction();
        }
    });
    return widget;
}

void MainWindow::populatePlugins(const QList<PluginInfo>& list)
{
    const QString previous = pluginCombo->currentData().toString(); // keep the selection across a rescan
    pluginCombo->clear();
    for (const auto& plugin : list)
        pluginCombo->addItem(plugin.displayName(), plugin.classId);
    const int index = pluginCombo->findData(previous);
    if (index >= 0)
        pluginCombo->setCurrentIndex(index);
    if (list.isEmpty())
        pluginCombo->setPlaceholderText(tr("No VST3 instruments found"));
    updateState();
}

void MainWindow::rescanPlugins()
{
    QStringList errors;
    {
        WaitCursor wait;
        pluginStatus->setText(tr("Scanning VST3 plugins…"));
        QApplication::processEvents();
        plugins = Vst3Scanner::scan(&errors);
    }
    Vst3Scanner::saveCache(plugins);
    populatePlugins(plugins);

    QString status = tr("Found %n instrument(s).", nullptr, static_cast<int>(plugins.size()));
    if (!errors.isEmpty())
        status += tr(" %n plugin bundle(s) could not be read.", nullptr, static_cast<int>(errors.size()));
    if (host->isLoaded())
        status += tr(" Loaded: %1.").arg(host->plugin().displayName());
    pluginStatus->setText(status);
    if (!errors.isEmpty())
        pluginStatus->setToolTip(errors.join('\n'));
}

void MainWindow::loadSelectedPlugin()
{
    const int index = pluginCombo->currentIndex();
    if (index < 0 || index >= plugins.size())
        return;

    if (editor)
        editor->close();

    QString error;
    bool ok = false;
    {
        WaitCursor wait;
        pluginStatus->setText(tr("Loading %1…").arg(plugins[index].displayName()));
        QApplication::processEvents();
        stopMonitor();
        ok = host->load(plugins[index], &error);
    }

    if (!ok)
    {
        pluginStatus->setText(tr("No instrument loaded."));
        QMessageBox::warning(this, tr("Load failed"), error);
    }
    else
    {
        pluginStatus->setText(tr("Loaded: %1. Choose a sound in the editor; you can play it there to listen.")
                                  .arg(host->plugin().displayName()));
        startMonitor();
        openEditor();
    }
    updateState();
}

void MainWindow::openEditor()
{
    if (!host->isLoaded())
        return;

    if (editor)
    {
        editor->show();
        editor->raise();
        editor->activateWindow();
        return;
    }

    QString error;
    editor = PluginEditorWindow::create(host->editController(), host->plugin().displayName(), &error);
    if (!editor)
    {
        QMessageBox::information(this, tr("No editor"), error);
        return;
    }
    editor->show();
}

void MainWindow::browseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Output folder"), folderEdit->text());
    if (!folder.isEmpty())
        folderEdit->setText(folder);
}

ExtractSettings MainWindow::currentSettings() const
{
    ExtractSettings s;
    s.keys = keyboard->selectedKeys();
    s.velocity = dynamicsCombo->currentData().toInt();
    s.durationSec = durationSpin->value();
    s.channels = channelsGroup->checkedId();
    s.bitsPerSample = bitsGroup->checkedId();
    s.sampleRate = sampleRateCombo->currentData().toInt();
    s.normalize = normalizeCheck->isChecked();
    s.name = nameEdit->text().trimmed();
    s.folder = folderEdit->text().trimmed();
    return s;
}

void MainWindow::startExtraction()
{
    ExtractSettings settings = currentSettings();

    QDir dir(settings.folder);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        QMessageBox::warning(this, tr("Output folder"), tr("Could not create %1.").arg(settings.folder));
        return;
    }

    int existing = 0;
    for (int key : settings.keys)
        if (QFileInfo::exists(settings.filePath(key)))
            ++existing;
    if (existing > 0 &&
        QMessageBox::question(this, tr("Overwrite files?"),
                              tr("%n file(s) with these names already exist in the folder. Overwrite them?",
                                 nullptr, existing)) != QMessageBox::Yes)
        return;

    QString error;
    {
        WaitCursor wait;
        stopMonitor(); // the extraction thread takes over the plugin
        if (!host->prepare(settings.sampleRate, Vst3Host::Mode::Offline, &error))
        {
            startMonitor();
            QMessageBox::warning(this, tr("Extraction failed"), error);
            return;
        }
    }

    setExtracting(true);
    progressBar->setRange(0, static_cast<int>(settings.keys.size()));
    progressBar->setValue(0);

    workerThread = new QThread(this);
    worker = new ExtractWorker(host.get(), settings);
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &ExtractWorker::run);
    connect(worker, &ExtractWorker::progress, this, [this](int done, int total, int key) {
        progressBar->setValue(done);
        if (key >= 0)
            progressLabel->setText(tr("Key %1 (%2/%3)").arg(key).arg(done + 1).arg(total));
    });
    connect(worker, &ExtractWorker::finished, this, &MainWindow::onExtractionFinished);
    connect(worker, &ExtractWorker::finished, workerThread, &QThread::quit);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    workerThread->start();
}

void MainWindow::onExtractionFinished(const QStringList& written, const QStringList& errors, bool cancelled)
{
    worker = nullptr;
    workerThread = nullptr;
    setExtracting(false);
    startMonitor();

    const QString folder = folderEdit->text().trimmed();
    progressLabel->setText(cancelled ? tr("Cancelled: %n file(s) written.", nullptr, static_cast<int>(written.size()))
                                     : tr("Done: %n file(s) written.", nullptr, static_cast<int>(written.size())));

    QMessageBox box(this);
    box.setWindowTitle(cancelled ? tr("Extraction cancelled") : tr("Extraction finished"));
    box.setIcon(errors.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    box.setText(tr("%n WAV file(s) written to %1.", nullptr, static_cast<int>(written.size())).arg(folder));
    if (!errors.isEmpty())
    {
        box.setInformativeText(tr("Some keys had problems. See details."));
        box.setDetailedText(errors.join('\n'));
    }
    QPushButton* showButton = box.addButton(tr("Show in Finder"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == showButton)
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MainWindow::setExtracting(bool value)
{
    extracting = value;
    extractButton->setText(value ? tr("Cancel") : tr("Extract"));
    extractButton->setEnabled(true);

    // The plugin must not be touched from the UI while the worker renders with it.
    if (editor)
        editor->setEnabled(!value);
    for (QWidget* w : std::initializer_list<QWidget*>{pluginCombo, loadButton, editorButton, rescanButton,
                                                      keyboard, dynamicsCombo, durationSpin, sampleRateCombo,
                                                      normalizeCheck, nameEdit, folderEdit})
        w->setEnabled(!value);
    for (auto* button : channelsGroup->buttons())
        button->setEnabled(!value);
    for (auto* button : bitsGroup->buttons())
        button->setEnabled(!value);
    if (!value)
        updateState();
}

void MainWindow::updateState()
{
    const int count = static_cast<int>(keyboard->selectedKeys().size());
    selectionLabel->setText(tr("%n key(s) selected", nullptr, count));

    const QString name = nameEdit->text().trimmed();
    const QList<int> keys = keyboard->selectedKeys();
    const int exampleKey = keys.isEmpty() ? 60 : keys.first();
    exampleLabel->setText(name.isEmpty() ? QString()
                                         : tr("Files: %1%2.wav, …").arg(name).arg(exampleKey));

    if (extracting)
        return;

    loadButton->setEnabled(pluginCombo->count() > 0);
    editorButton->setEnabled(host->isLoaded());

    QString missing;
    if (!host->isLoaded())
        missing = tr("Load an instrument.");
    else if (count == 0)
        missing = tr("Select at least one key.");
    else if (name.isEmpty())
        missing = tr("Enter a name.");
    else if (folderEdit->text().trimmed().isEmpty())
        missing = tr("Choose an output folder.");

    extractButton->setEnabled(missing.isEmpty());
    extractButton->setToolTip(missing);
    progressLabel->setText(missing.isEmpty() ? tr("Ready.") : missing);
}

void MainWindow::applyDefaults()
{
    dynamicsCombo->setCurrentIndex(dynamicsCombo->findData(kDefaultVelocity));    durationSpin->setValue(1.0);
    channelsGroup->button(1)->setChecked(true);
    bitsGroup->button(16)->setChecked(true);
    sampleRateCombo->setCurrentIndex(sampleRateCombo->findData(44100));
    normalizeCheck->setChecked(true);
    folderEdit->setText(QStandardPaths::writableLocation(QStandardPaths::MusicLocation) +
                        QStringLiteral("/NXSampler"));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (extracting)
    {
        QMessageBox::information(this, tr("Extraction running"),
                                 tr("Cancel the extraction before closing NXSampler."));
        event->ignore();
        return;
    }
    if (editor)
        editor->close();
    QMainWindow::closeEvent(event);
}

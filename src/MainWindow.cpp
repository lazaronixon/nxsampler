#include "MainWindow.h"

#include "ExtractWorker.h"
#include "KeyboardWidget.h"
#include "PluginEditorWindow.h"

#include <QApplication>
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
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
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

// Recommended settings per Korg Pa3X factory sound category, in the keyboard's order.
struct Preset
{
    const char* name;
    double durationSec;
    bool loop;
    bool crossfade;
    bool trim;
    int keyStep;  // selects every Nth key from firstKey, always including lastKey (1 = every key)
    int firstKey; // each range covers the real instruments in that factory bank
    int lastKey;
};

// The Pa3X Le's 76 keys, E1–G7.
constexpr int kKeyboardLow = 28;
constexpr int kKeyboardHigh = 103;
// Every key of a GM2/Korg drum kit (High Q to Open Surdo), beyond the core GM 35–81.
// In a kit each key is a different drum, so an unsampled key would play the wrong drum.
constexpr int kDrumLow = 27;
constexpr int kDrumHigh = 87;

const Preset kPresets[] = {
    // Durations assume the plugin's modulation effects (rotary, phaser, chorus, LFO) are
    // off and added back on the keyboard, which keeps loops short for its 192 MB memory.
    {"Piano", 3.0, true, true, false, 3, kKeyboardLow, kKeyboardHigh},
    {"E. Piano", 3.0, true, true, false, 3, kKeyboardLow, kKeyboardHigh},
    {"Mallet & Bell", 6.0, false, false, true, 3, 36, 96}, // C2–C7, marimba/vibes; one-shots, bells ring long
    {"Accordion", 3.0, true, true, false, 3, 53, 93},      // F3–A6, the treble keyboard
    {"Organ", 2.0, true, true, false, 4, kKeyboardLow, kKeyboardHigh},
    {"Guitar", 3.0, true, true, false, 3, 40, 88}, // E2–E6
    {"Strings & Vocal", 3.0, true, true, false, 3, kKeyboardLow, 96}, // E1–C7, contrabass to violin
    {"Trumpet & Trbn.", 3.0, true, true, false, 3, 40, 84},          // E2–C6; 3 s: delayed vibrato
    {"Brass", 2.0, true, true, false, 3, 40, 84},                    // E2–C6, sections
    {"Sax", 3.0, true, true, false, 3, 37, 88},                      // C#2–E6; 3 s: delayed vibrato
    {"Woodwind", 3.0, true, true, false, 3, 50, 96},                 // D3–C7, clarinet/oboe/flute; 3 s: vibrato
    {"Synth Pad", 4.0, true, true, false, 4, kKeyboardLow, kKeyboardHigh}, // synthetic: every 4th key is enough
    {"Synth Lead", 2.0, true, true, false, 4, 36, 103}, // C2–G7
    {"Ethnic", 3.0, true, true, false, 3, 40, 93},      // E2–A6, oud to mandolin
    {"Bass", 2.0, true, true, false, 4, 24, 72},        // C1–C5
    // 10 s: only a limit, since Auto Trim cuts each sound to its own length; long SFX fit too.
    {"Drum & SFX", 10.0, false, false, true, 1, kDrumLow, kDrumHigh}, // each kit key is a different drum
};

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
    setWindowTitle(QStringLiteral("NXSampler %1").arg(QApplication::applicationVersion()));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(buildInstrumentBox());
    layout->addWidget(buildKeyboardBox());
    layout->addWidget(buildPresetsBox());
    layout->addWidget(buildSettingsBox());
    auto* loopAndTrim = new QHBoxLayout; // Auto Loop and Auto Trim side by side
    loopAndTrim->addWidget(buildLoopBox(), 1);
    loopAndTrim->addWidget(buildTrimBox(), 1);
    layout->addLayout(loopAndTrim);
    layout->addWidget(buildOutputBox());
    alignFormLabels();
    matchDropdownPadding({nameEdit, folderEdit, durationSpin, crossfadeSpin, trimThresholdSpin, trimFadeSpin});
    layout->addStretch(); // extra window height goes here, so the panels keep their size
    layout->addWidget(buildActionRow());
    setCentralWidget(central);

    // Status bar: instructions and progress on the left, instrument state on the right.
    statusMessage = new QLabel;
    statusMessage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // long text clips, never widens
    pluginStatus = new QLabel(tr("No instrument loaded."));
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(statusMessage, 1);
    statusBar()->addPermanentWidget(pluginStatus);

    applyDefaults();

    // Show the instruments found last time right away, then scan again so newly
    // installed or removed plugins are picked up on every launch.
    plugins = Vst3Scanner::loadCached();
    populatePlugins(plugins);
    QTimer::singleShot(0, this, &MainWindow::scanPlugins);

    updateState();

    // Resizable, but with no maximize or full-screen button. The window is centred on
    // the screen in showEvent(), once the title bar's size is known.
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::WindowFullscreenButtonHint, false);
    adjustSize();

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
        pluginStatus->setText(tr("Loaded: %1 (no live audio: %2)").arg(host->plugin().displayName(), error));
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
    loadButton = new QPushButton(tr("Select"));
    loadButton->setToolTip(tr("Load the chosen instrument."));
    editorButton = new QPushButton(tr("Open"));
    editorButton->setToolTip(tr("Open the instrument's own window to choose and play a sound."));

    layout->addWidget(pluginCombo, 0, 0);
    layout->addWidget(loadButton, 0, 1);
    layout->addWidget(editorButton, 0, 2);
    layout->setColumnStretch(0, 1);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadSelectedPlugin);
    connect(editorButton, &QPushButton::clicked, this, &MainWindow::openEditor);
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

    connect(keyboard, &KeyboardWidget::selectionChanged, this, &MainWindow::updateState);
    return box;
}

QWidget* MainWindow::buildPresetsBox()
{
    auto* box = new QGroupBox(tr("Presets"));
    QGridLayout* grid = newTwoColumnGrid(box);

    presetCombo = new QComboBox;
    for (int i = 0; i < static_cast<int>(std::size(kPresets)); ++i)
        presetCombo->addItem(tr(kPresets[i].name), i);
    presetCombo->setToolTip(tr("Selects the keys to sample and fills in Duration, Auto Loop, "
                               "Crossfade and Auto Trim for this kind of sound."));
    addGridSetting(grid, 0, 0, tr("Category:"), presetCombo, nullptr, 4); // full width

    connect(presetCombo, &QComboBox::currentIndexChanged, this, &MainWindow::applyPreset);
    return box;
}

void MainWindow::applyPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(kPresets)))
        return;
    const Preset& preset = kPresets[index];
    durationSpin->setValue(preset.durationSec);
    loopCheck->setChecked(preset.loop);
    crossfadeCheck->setChecked(preset.crossfade);
    trimCheck->setChecked(preset.trim);
    updateCrossfadeEnabled();
    updateTrimEnabled();

    QList<int> keys;
    for (int key = preset.firstKey; key <= preset.lastKey; key += preset.keyStep)
        keys.append(key);
    if (keys.last() != preset.lastKey)
        keys.append(preset.lastKey); // the top of the range always gets its own sample
    keyboard->setSelectedKeys(keys);
}

QWidget* MainWindow::buildSettingsBox()
{
    auto* box = new QGroupBox(tr("Sample settings"));

    QGridLayout* grid = newTwoColumnGrid(box);
    auto addSetting = [this, grid](int row, int column, const QString& text, QWidget* control,
                                   QLayout* controlLayout = nullptr) {
        addGridSetting(grid, row, column, text, control, controlLayout);
    };

    durationSpin = new QDoubleSpinBox;
    durationSpin->setRange(0.05, 60.0);
    durationSpin->setDecimals(2);
    durationSpin->setSingleStep(0.1);
    durationSpin->setSuffix(tr(" s"));
    durationSpin->setToolTip(tr("The note is held for this long; the file is cut at exactly this length."));
    addSetting(0, 0, tr("Duration:"), durationSpin);

    dynamicsCombo = new QComboBox;
    for (const Dynamic& d : kDynamics)
        dynamicsCombo->addItem(QStringLiteral("%1  (velocity %2)").arg(QLatin1String(d.symbol)).arg(d.velocity),
                               d.velocity);
    dynamicsCombo->setToolTip(tr("MIDI velocity each note is played with."));
    addSetting(1, 0, tr("Dynamics:"), dynamicsCombo);

    sampleRateCombo = new QComboBox;
    for (int rate : kSampleRates)
        sampleRateCombo->addItem(QStringLiteral("%1 Hz").arg(QLocale().toString(rate)), rate);
    addSetting(2, 0, tr("Sample rate:"), sampleRateCombo);

    bitsCombo = new QComboBox;
    bitsCombo->addItem(tr("16-bit"), 16);
    bitsCombo->addItem(tr("8-bit"), 8);
    addSetting(0, 3, tr("Bit depth:"), bitsCombo);

    channelsCombo = new QComboBox;
    channelsCombo->addItem(tr("Mono"), 1);
    channelsCombo->addItem(tr("Stereo"), 2);
    addSetting(1, 3, tr("Channels:"), channelsCombo);

    normalizeCheck = new QCheckBox;
    addSetting(2, 3, tr("Normalize:"), nullptr, checkRow(normalizeCheck));

    return box;
}

QWidget* MainWindow::buildLoopBox()
{
    auto* box = new QGroupBox(tr("Auto Loop"));
    QGridLayout* grid = newOneColumnGrid(box);

    loopCheck = new QCheckBox;
    loopCheck->setToolTip(tr("Find the most seamless loop in each note and store it in the WAV file."));
    addGridSetting(grid, 0, 0, tr("Enabled:"), nullptr, checkRow(loopCheck));

    crossfadeCheck = new QCheckBox;
    crossfadeSpin = new QSpinBox;
    crossfadeSpin->setRange(1, 100);
    crossfadeSpin->setSingleStep(10);
    crossfadeSpin->setSuffix(tr("%"));
    const QString crossfadeTip = tr("Blends the end of the loop into the audio before the loop start "
                                    "for a smoother loop. Length as a percentage of the loop.");
    crossfadeCheck->setToolTip(crossfadeTip);
    crossfadeSpin->setToolTip(crossfadeTip);
    addGridSetting(grid, 1, 0, tr("Crossfade:"), nullptr, checkRow(crossfadeCheck, crossfadeSpin));

    connect(loopCheck, &QCheckBox::toggled, this, &MainWindow::updateCrossfadeEnabled);
    connect(crossfadeCheck, &QCheckBox::toggled, this, &MainWindow::updateCrossfadeEnabled);

    grid->setRowStretch(grid->rowCount(), 1); // keep rows at the top when Auto Trim is taller
    return box;
}

QWidget* MainWindow::buildTrimBox()
{
    auto* box = new QGroupBox(tr("Auto Trim"));
    QGridLayout* grid = newOneColumnGrid(box);

    trimCheck = new QCheckBox;
    trimCheck->setToolTip(tr("Cut the silence at the end of each note, for one-shots like drums "
                             "and effects. When on, no loop is written even if Auto Loop is on."));
    addGridSetting(grid, 0, 0, tr("Enabled:"), nullptr, checkRow(trimCheck));

    trimThresholdSpin = new QSpinBox;
    trimThresholdSpin->setRange(-120, -1);
    trimThresholdSpin->setSuffix(tr(" dB"));
    trimThresholdSpin->setToolTip(tr("Cut where the sound falls this far below its own loudest point."));
    addGridSetting(grid, 1, 0, tr("Threshold:"), trimThresholdSpin);

    trimFadeSpin = new QSpinBox;
    trimFadeSpin->setRange(0, 1000);
    trimFadeSpin->setSingleStep(5);
    trimFadeSpin->setSuffix(tr(" ms"));
    trimFadeSpin->setToolTip(tr("Fade-out added at the cut, so the end of the file doesn't click."));
    addGridSetting(grid, 2, 0, tr("Fade out:"), trimFadeSpin);

    connect(trimCheck, &QCheckBox::toggled, this, &MainWindow::updateTrimEnabled);

    return box;
}

QFormLayout* MainWindow::newForm(QWidget* box)
{
    // One control per row; controls stretch to the full width of the panel.
    auto* form = box ? new QFormLayout(box) : new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop); // macOS centres rows by default
    form->setVerticalSpacing(4);
    forms.append(form);
    return form;
}

QHBoxLayout* MainWindow::checkRow(QCheckBox* check, QWidget* extra)
{
    // Every checkbox row is built the same way so the checkboxes line up.
    auto* row = new QHBoxLayout;
    row->addWidget(check);
    if (extra)
        row->addWidget(extra, 1); // fills the rest of the row
    else
        row->addStretch();
    return row;
}

QGridLayout* MainWindow::newOneColumnGrid(QWidget* box)
{
    // A single label + control column, for the half-width panels.
    auto* grid = new QGridLayout(box);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);
    return grid;
}

QGridLayout* MainWindow::newTwoColumnGrid(QWidget* box)
{
    // Two equal columns on one grid, so rows line up across them.
    // Columns: label, control, gap, label, control.
    auto* grid = new QGridLayout(box);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(4, 1);
    grid->setColumnMinimumWidth(2, 16);
    return grid;
}

void MainWindow::addGridSetting(QGridLayout* grid, int row, int column, const QString& text, QWidget* control,
                                QLayout* controlLayout, int columnSpan)
{
    auto* label = new QLabel(text);
    alignedLabels.append(label);
    grid->addWidget(label, row, column, Qt::AlignRight | Qt::AlignVCenter);
    if (controlLayout)
        grid->addLayout(controlLayout, row, column + 1, 1, columnSpan);
    else
        grid->addWidget(control, row, column + 1, 1, columnSpan);
}

void MainWindow::matchDropdownPadding(std::initializer_list<QWidget*> inputs)
{
#if defined(Q_OS_MACOS)
    // macOS indents dropdown text much more than text-field text. Extra left padding
    // (measured against QComboBox) makes typed values start where dropdown values do.
    // Spin boxes need a little less because their frame sits further left.
    constexpr int kLineEditPadding = 10;
    constexpr int kSpinBoxPadding = 8;
    for (QWidget* input : inputs)
    {
        if (auto* edit = qobject_cast<QLineEdit*>(input))
            edit->setTextMargins(kLineEditPadding, 0, 0, 0);
        else if (auto* spin = qobject_cast<QAbstractSpinBox*>(input))
            if (auto* edit = spin->findChild<QLineEdit*>())
                edit->setTextMargins(kSpinBoxPadding, 0, 0, 0);
    }
#else
    Q_UNUSED(inputs); // other styles already pad text fields and dropdowns alike
#endif
}

void MainWindow::alignFormLabels()
{
    // Same label width in every panel, so the controls line up from panel to panel.
    QList<QWidget*> labels(alignedLabels.begin(), alignedLabels.end());
    for (QFormLayout* form : std::as_const(forms))
        for (int row = 0; row < form->rowCount(); ++row)
            if (QLayoutItem* item = form->itemAt(row, QFormLayout::LabelRole); item && item->widget())
                labels.append(item->widget());

    int widest = 0;
    for (QWidget* label : std::as_const(labels))
        widest = std::max(widest, label->sizeHint().width());
    for (QWidget* label : std::as_const(labels))
        label->setMinimumWidth(widest);
}


QWidget* MainWindow::buildOutputBox()
{
    auto* box = new QGroupBox(tr("Output"));
    auto* form = newForm(box);

    nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QStringLiteral("RealStrF"));
    nameEdit->setMaxLength(13);
    nameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_\\-]{1,13}")), nameEdit));
    form->addRow(tr("Name:"), nameEdit);

    auto* folderRow = new QWidget;
    auto* folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderEdit = new QLineEdit;
    auto* browse = new QPushButton(tr("Browse…"));
    folderLayout->addWidget(folderEdit);
    folderLayout->addWidget(browse);
    form->addRow(tr("Output folder:"), folderRow);

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
    extractButton = new QPushButton(tr("Extract"));
    extractButton->setDefault(true);
    extractButton->setMinimumWidth(120);

    row->addWidget(progressBar, 1);
    row->addWidget(extractButton);

    connect(extractButton, &QPushButton::clicked, this, [this] {
        if (extracting)
        {
            if (worker)
                worker->cancel();
            extractButton->setEnabled(false);
            statusMessage->setText(tr("Cancelling…"));
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
    const QString previous = pluginCombo->currentData().toString(); // keep the selection across a scan
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

void MainWindow::scanPlugins()
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
        pluginStatus->setText(tr("Loaded: %1").arg(host->plugin().displayName()));
        startMonitor();
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
    s.channels = channelsCombo->currentData().toInt();
    s.bitsPerSample = bitsCombo->currentData().toInt();
    s.sampleRate = sampleRateCombo->currentData().toInt();
    s.normalize = normalizeCheck->isChecked();
    s.loop = loopCheck->isChecked();
    s.crossfadePercent = crossfadeCheck->isChecked() ? crossfadeSpin->value() : 0;
    s.trim = trimCheck->isChecked();
    s.trimThresholdDb = trimThresholdSpin->value();
    s.trimFadeMs = trimFadeSpin->value();
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
            statusMessage->setText(tr("Extracting key %1 (%2/%3)…").arg(key).arg(done + 1).arg(total));
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
    statusMessage->setText(cancelled ? tr("Cancelled: %n file(s) written.", nullptr, static_cast<int>(written.size()))
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
    for (QWidget* w : std::initializer_list<QWidget*>{pluginCombo, loadButton, editorButton, presetCombo,
                                                      keyboard, dynamicsCombo, durationSpin, sampleRateCombo,
                                                      bitsCombo, channelsCombo, normalizeCheck, loopCheck,
                                                      trimCheck, nameEdit, folderEdit})
        w->setEnabled(!value);
    updateCrossfadeEnabled();
    updateTrimEnabled();
    if (!value)
        updateState();
}

void MainWindow::updateCrossfadeEnabled()
{
    const bool loopOn = !extracting && loopCheck->isChecked();
    crossfadeCheck->setEnabled(loopOn);
    crossfadeSpin->setEnabled(loopOn && crossfadeCheck->isChecked());
}

void MainWindow::updateTrimEnabled()
{
    const bool trimOn = !extracting && trimCheck->isChecked();
    trimThresholdSpin->setEnabled(trimOn);
    trimFadeSpin->setEnabled(trimOn);
}

void MainWindow::updateState()
{
    const int count = static_cast<int>(keyboard->selectedKeys().size());
    const QString name = nameEdit->text().trimmed();

    if (extracting)
        return;

    loadButton->setEnabled(pluginCombo->count() > 0);
    editorButton->setEnabled(host->isLoaded());

    QString missing;
    if (!host->isLoaded())
        missing = tr("Pick an instrument and click Select.");
    else if (count == 0)
        missing = tr("Click Open to choose a sound, then select keys: click to toggle, "
                     "drag to paint, Shift+click for a range.");
    else if (name.isEmpty())
        missing = tr("Enter a name.");
    else if (folderEdit->text().trimmed().isEmpty())
        missing = tr("Choose an output folder.");

    extractButton->setEnabled(missing.isEmpty());
    extractButton->setToolTip(missing);
    statusMessage->setText(missing.isEmpty() ? tr("Ready. Click Extract.") : missing);
}

void MainWindow::applyDefaults()
{
    dynamicsCombo->setCurrentIndex(dynamicsCombo->findData(kDefaultVelocity));
    durationSpin->setValue(1.0);
    channelsCombo->setCurrentIndex(channelsCombo->findData(1));
    bitsCombo->setCurrentIndex(bitsCombo->findData(16));
    sampleRateCombo->setCurrentIndex(sampleRateCombo->findData(44100));
    normalizeCheck->setChecked(true);
    loopCheck->setChecked(true);
    crossfadeCheck->setChecked(false);
    crossfadeSpin->setValue(30);
    updateCrossfadeEnabled();
    trimCheck->setChecked(false);
    trimThresholdSpin->setValue(-60);
    trimFadeSpin->setValue(10);
    updateTrimEnabled();

    // Start with the first preset (Piano). Selecting index 0 doesn't emit a change when it
    // is already selected, so apply it explicitly.
    presetCombo->setCurrentIndex(0);
    applyPreset(0);
    folderEdit->setText(QStandardPaths::writableLocation(QStandardPaths::MusicLocation) +
                        QStringLiteral("/NXSampler"));
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, &MainWindow::alignStatusBar);

    if (!centred)
    {
        centred = true;
        // Centre the whole window, title bar included, on the screen it opens on.
        if (QScreen* display = screen())
            move(pos() + display->availableGeometry().center() - frameGeometry().center());
    }
}

void MainWindow::alignStatusBar()
{
    // Auto Trim sits in the right half; shift its contents so its checkbox and fields
    // start exactly where Sample settings' right column does (Normalize's checkbox).
    if (QLayout* trimLayout = trimCheck->parentWidget()->layout())
    {
        trimLayout->activate();
        QMargins margins = trimLayout->contentsMargins();
        const int delta = normalizeCheck->mapTo(this, QPoint(0, 0)).x() - trimCheck->mapTo(this, QPoint(0, 0)).x();
        margins.setLeft(std::max(0, margins.left() + delta));
        trimLayout->setContentsMargins(margins);
    }
    // Line the status bar text up with the panels above. The status bar adds its own
    // spacing, which depends on the style, so measure instead of guessing.
    const QMargins content = centralWidget()->layout()->contentsMargins();

    statusMessage->setContentsMargins(0, 0, 0, 0);
    pluginStatus->setContentsMargins(0, 0, 0, 0);
    statusBar()->layout()->activate();

    const int textLeft = statusMessage->mapTo(this, QPoint(0, 0)).x();
    const int textRight = width() - pluginStatus->mapTo(this, QPoint(pluginStatus->width(), 0)).x();
    statusMessage->setContentsMargins(std::max(0, content.left() - textLeft), 0, 0, 0);
    pluginStatus->setContentsMargins(0, 0, std::max(0, content.right() - textRight), 0);
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

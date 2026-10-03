#pragma once

#include "AudioMonitor.h"
#include "Vst3Host.h"
#include "Vst3Scanner.h"

#include <QMainWindow>
#include <QPointer>

#include <memory>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QThread;

class ExtractWorker;
class KeyboardWidget;
class PluginEditorWindow;
struct ExtractSettings;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QWidget* buildInstrumentBox();
    QWidget* buildKeyboardBox();
    QWidget* buildSettingsBox();
    QWidget* buildOutputBox();
    QWidget* buildActionRow();

    void populatePlugins(const QList<PluginInfo>& list);
    void rescanPlugins();
    void loadSelectedPlugin();
    void openEditor();
    void browseFolder();
    void startExtraction();
    void onExtractionFinished(const QStringList& written, const QStringList& errors, bool cancelled);

    ExtractSettings currentSettings() const;
    void setExtracting(bool extracting);
    void updateState();
    void updateCrossfadeEnabled();
    void alignStatusBar();
    void applyDefaults();
    // Live audio, so the instrument can be heard while playing it in its editor.
    void startMonitor();
    void stopMonitor();

    std::unique_ptr<Vst3Host> host;
    AudioMonitor monitor;
    QList<PluginInfo> plugins;
    QPointer<PluginEditorWindow> editor;
    QThread* workerThread = nullptr;
    ExtractWorker* worker = nullptr;
    bool extracting = false;

    // Instrument
    QComboBox* pluginCombo = nullptr;
    QPushButton* loadButton = nullptr;
    QPushButton* editorButton = nullptr;
    QPushButton* rescanButton = nullptr;
    QLabel* pluginStatus = nullptr; // status bar: loaded instrument

    // Keys
    QScrollArea* keyboardScroll = nullptr;
    KeyboardWidget* keyboard = nullptr;
    QLabel* selectionLabel = nullptr;

    // Settings
    QComboBox* dynamicsCombo = nullptr; // item data = MIDI velocity
    QDoubleSpinBox* durationSpin = nullptr;
    QButtonGroup* channelsGroup = nullptr;
    QButtonGroup* bitsGroup = nullptr;
    QComboBox* sampleRateCombo = nullptr;
    QCheckBox* normalizeCheck = nullptr;
    QCheckBox* loopCheck = nullptr;
    QCheckBox* crossfadeCheck = nullptr;
    QSpinBox* crossfadeSpin = nullptr; // % of the loop length
    QLineEdit* nameEdit = nullptr;
    QLineEdit* folderEdit = nullptr;

    // Actions
    QProgressBar* progressBar = nullptr;
    QLabel* statusMessage = nullptr; // status bar: instructions and progress
    QPushButton* extractButton = nullptr;
};

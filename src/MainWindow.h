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

private:
    QWidget* buildInstrumentBox();
    QWidget* buildKeyboardBox();
    QWidget* buildSettingsBox();
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
    QLabel* pluginStatus = nullptr;

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
    QLineEdit* nameEdit = nullptr;
    QLineEdit* folderEdit = nullptr;
    QLabel* exampleLabel = nullptr;

    // Actions
    QProgressBar* progressBar = nullptr;
    QLabel* progressLabel = nullptr;
    QPushButton* extractButton = nullptr;
};

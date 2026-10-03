#include "Vst3Host.h"

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

Vst3Host::Vst3Host()
{
    PluginContextFactory::instance().setPluginContext(&hostContext);
}

Vst3Host::~Vst3Host()
{
    unload();
    PluginContextFactory::instance().setPluginContext(nullptr);
}

bool Vst3Host::load(const PluginInfo& plugin, QString* error)
{
    unload();

    std::string moduleError;
    auto newModule = VST3::Hosting::Module::create(plugin.path.toStdString(), moduleError);
    if (!newModule)
    {
        if (error)
            *error = QStringLiteral("Could not load %1: %2")
                         .arg(plugin.path, QString::fromStdString(moduleError));
        return false;
    }

    const auto uid = VST3::UID::fromString(plugin.classId.toStdString());
    if (!uid)
    {
        if (error)
            *error = QStringLiteral("Invalid plugin class ID %1. Try Rescan.").arg(plugin.classId);
        return false;
    }

    const auto& factory = newModule->getFactory();
    factory.setHostContext(&hostContext);

    for (const auto& info : factory.classInfos())
    {
        if (info.ID() != *uid)
            continue;

        auto newProvider = owned(new PlugProvider(factory, info, true));
        if (!newProvider->initialize())
            break;

        module = newModule;
        provider = newProvider;
        component = provider->getComponentPtr();
        controller = provider->getControllerPtr();
        processor = U::cast<IAudioProcessor>(component);
        loadedPlugin = plugin;

        if (!processor)
        {
            unload();
            if (error)
                *error = QStringLiteral("%1 has no audio processor.").arg(plugin.name);
            return false;
        }
        return true;
    }

    if (error)
        *error = QStringLiteral("Could not create %1. Try Rescan.").arg(plugin.name);
    return false;
}

void Vst3Host::unload()
{
    deactivate();
    processor = nullptr;
    controller = nullptr;
    component = nullptr;
    provider = nullptr; // terminates and disconnects component and controller
    module = nullptr;
    loadedPlugin = {};
    setupSampleRate = 0.0;
}

void Vst3Host::deactivate()
{
    stopProcessing();
    if (active && component)
        component->setActive(false);
    active = false;
    processData.unprepare();
}

bool Vst3Host::prepare(double sampleRate, Mode mode, QString* error)
{
    if (!isLoaded())
    {
        if (error)
            *error = QStringLiteral("No instrument is loaded.");
        return false;
    }

    deactivate();

    // Offline mode lets sample players load from disk instead of dropping notes.
    const int32 processMode = mode == Mode::Offline ? kOffline : kRealtime;

    ProcessSetup setup {};
    setup.processMode = processMode;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate = sampleRate;
    if (processor->canProcessSampleSize(kSample32) != kResultTrue ||
        processor->setupProcessing(setup) != kResultOk)
    {
        if (error)
            *error = QStringLiteral("%1 rejected %2 Hz %3 processing.")
                         .arg(loadedPlugin.name)
                         .arg(sampleRate)
                         .arg(mode == Mode::Offline ? QStringLiteral("offline") : QStringLiteral("realtime"));
        return false;
    }

    // Ask for a stereo main output; other buses keep their current layout.
    const int32 numIn = component->getBusCount(kAudio, kInput);
    const int32 numOut = component->getBusCount(kAudio, kOutput);
    if (numOut < 1)
    {
        if (error)
            *error = QStringLiteral("%1 has no audio output.").arg(loadedPlugin.name);
        return false;
    }
    std::vector<SpeakerArrangement> inArr(static_cast<size_t>(numIn));
    std::vector<SpeakerArrangement> outArr(static_cast<size_t>(numOut));
    for (int32 i = 0; i < numIn; ++i)
        processor->getBusArrangement(kInput, i, inArr[static_cast<size_t>(i)]);
    for (int32 i = 0; i < numOut; ++i)
        processor->getBusArrangement(kOutput, i, outArr[static_cast<size_t>(i)]);
    outArr[0] = SpeakerArr::kStereo;
    processor->setBusArrangements(inArr.data(), numIn, outArr.data(), numOut);

    for (int32 i = 0; i < numIn; ++i)
        component->activateBus(kAudio, kInput, i, false);
    for (int32 i = 0; i < numOut; ++i)
        component->activateBus(kAudio, kOutput, i, i == 0);
    if (component->getBusCount(kEvent, kInput) > 0)
        component->activateBus(kEvent, kInput, 0, true);

    if (component->setActive(true) != kResultOk)
    {
        if (error)
            *error = QStringLiteral("%1 could not be activated.").arg(loadedPlugin.name);
        return false;
    }
    active = true;

    if (!processData.prepare(*component, kBlockSize, kSample32))
    {
        deactivate();
        if (error)
            *error = QStringLiteral("Could not allocate audio buffers.");
        return false;
    }

    processData.processMode = processMode;
    processData.inputEvents = &eventList;
    processData.inputParameterChanges = &inputChanges;
    processData.outputParameterChanges = &outputChanges;
    processData.processContext = &processContext;

    processContext = {};
    processContext.sampleRate = sampleRate;
    processContext.tempo = 120.0;
    processContext.timeSigNumerator = 4;
    processContext.timeSigDenominator = 4;
    processContext.state = ProcessContext::kPlaying | ProcessContext::kTempoValid |
                           ProcessContext::kTimeSigValid | ProcessContext::kContTimeValid;

    latency = static_cast<int>(processor->getLatencySamples());
    setupSampleRate = sampleRate;
    return true;
}

void Vst3Host::startProcessing()
{
    if (active && !processing)
    {
        processor->setProcessing(true);
        processing = true;
    }
}

void Vst3Host::stopProcessing()
{
    if (processing && processor)
        processor->setProcessing(false);
    processing = false;
}

void Vst3Host::process(int64_t frames, const std::vector<Event>& events, Channels* out)
{
    if (!out)
    {
        render(frames, events, nullptr, nullptr);
        return;
    }

    out->resize(2);
    const size_t writePos = (*out)[0].size();
    (*out)[0].resize(writePos + static_cast<size_t>(frames), 0.0f);
    (*out)[1].resize(writePos + static_cast<size_t>(frames), 0.0f);
    render(frames, events, (*out)[0].data() + writePos, (*out)[1].data() + writePos);
}

void Vst3Host::render(int64_t frames, const std::vector<Event>& events, float* left, float* right)
{
    float* const destinations[2] = {left, right};
    if (!processing || processData.numOutputs < 1)
    {
        for (float* dest : destinations)
            if (dest)
                std::fill_n(dest, frames, 0.0f);
        return;
    }

    AudioBusBuffers& mainOut = processData.outputs[0];
    const int32 outChannels = mainOut.numChannels;
    int64_t writePos = 0;

    bool firstBlock = true;
    int64_t remaining = frames;
    while (remaining > 0)
    {
        const int32 blockFrames = static_cast<int32>(std::min<int64_t>(remaining, kBlockSize));

        eventList.clear();
        if (firstBlock)
            for (Event event : events)
                eventList.addEvent(event);
        firstBlock = false;

        inputChanges.clearQueue();
        outputChanges.clearQueue();
        for (int32 ch = 0; ch < outChannels; ++ch)
            std::memset(mainOut.channelBuffers32[ch], 0, sizeof(float) * kBlockSize);
        mainOut.silenceFlags = 0;

        processData.numSamples = blockFrames;
        processor->process(processData);

        processContext.projectTimeSamples += blockFrames;
        processContext.continousTimeSamples += blockFrames;

        for (int32 ch = 0; ch < 2; ++ch)
        {
            float* dest = destinations[ch];
            if (!dest)
                continue;
            // A mono plugin output feeds both channels.
            const int32 source = std::min(ch, outChannels - 1);
            const bool silent = outChannels < 1 || ((mainOut.silenceFlags >> source) & 1);
            if (silent)
                std::fill_n(dest + writePos, blockFrames, 0.0f);
            else
                std::copy_n(mainOut.channelBuffers32[source], blockFrames, dest + writePos);
        }

        writePos += blockFrames;
        remaining -= blockFrames;
    }
}

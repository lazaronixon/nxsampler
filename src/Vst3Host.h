#pragma once

#include "AudioOps.h"
#include "Vst3Scanner.h"

#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <QString>

#include <vector>

// Owns one loaded VST3 instrument and renders audio from it offline.
//
// Threading: load(), unload() and prepare() run on the UI thread, never while audio is
// being rendered. startProcessing(), process()/render() and stopProcessing() run on
// either the extraction thread or the live audio thread, never both at once.
class Vst3Host
{
public:
    static constexpr int kBlockSize = 512;

    Vst3Host();
    ~Vst3Host();

    Vst3Host(const Vst3Host&) = delete;
    Vst3Host& operator=(const Vst3Host&) = delete;

    bool load(const PluginInfo& plugin, QString* error);
    void unload();
    bool isLoaded() const { return component != nullptr; }
    const PluginInfo& plugin() const { return loadedPlugin; }

    Steinberg::Vst::IEditController* editController() const { return controller.get(); }

    enum class Mode
    {
        Offline,  // extraction: as fast as possible, sample players load fully from disk
        Realtime, // live monitoring through the speakers
    };

    // Configures processing at the given rate and mode and activates the plugin.
    bool prepare(double sampleRate, Mode mode, QString* error);
    double sampleRate() const { return setupSampleRate; }
    int latencySamples() const { return latency; }

    void startProcessing();
    void stopProcessing();

    // Renders `frames` frames. `events` all start at sample offset 0 of the first block.
    // When `out` is not null, two channels (L, R) are appended to it.
    void process(int64_t frames, const std::vector<Steinberg::Vst::Event>& events, Channels* out);

    // Same, writing into caller-owned buffers (null to discard). Does not allocate,
    // so it is safe on the live audio thread.
    void render(int64_t frames, const std::vector<Steinberg::Vst::Event>& events, float* left,
                float* right);

private:
    void deactivate();

    Steinberg::Vst::HostApplication hostContext;
    VST3::Hosting::Module::Ptr module;
    Steinberg::IPtr<Steinberg::Vst::PlugProvider> provider;
    Steinberg::IPtr<Steinberg::Vst::IComponent> component;
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> processor;
    Steinberg::IPtr<Steinberg::Vst::IEditController> controller;
    PluginInfo loadedPlugin;

    Steinberg::Vst::HostProcessData processData;
    Steinberg::Vst::ProcessContext processContext {};
    Steinberg::Vst::EventList eventList {64};
    Steinberg::Vst::ParameterChanges inputChanges;
    Steinberg::Vst::ParameterChanges outputChanges;

    double setupSampleRate = 0.0;
    int latency = 0;
    bool active = false;
    bool processing = false;
};

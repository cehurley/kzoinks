#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "IModule.h"
#include "SynthParameters.h"

// Singleton registry. Each module self-registers via REGISTER_MODULE() at startup.
class ModuleRegistry
{
public:
    using Factory = std::function<std::unique_ptr<IModule>(SynthParameters&)>;

    static ModuleRegistry& getInstance();

    void registerFactory(Factory f);
    std::vector<std::unique_ptr<IModule>> createAll(SynthParameters& params) const;

    // Names of every registered module type, for populating "pick a module" UI.
    // Constructs a throwaway instance of each to read getName() — fine since this
    // only runs at startup / on user action, never on the audio thread.
    juce::StringArray getAvailableNames(SynthParameters& params) const;

    // Builds a fresh instance of the type with this getName(), or nullptr if no
    // registered type matches.
    std::unique_ptr<IModule> createByName(const juce::String& name, SynthParameters& params) const;

private:
    std::vector<Factory> factories;
};

// Drop this at the bottom of any module's .cpp to auto-register it.
#define REGISTER_MODULE(ClassName)                                             \
    static const bool _synthReg_##ClassName = [] {                            \
        ModuleRegistry::getInstance().registerFactory(                         \
            [](SynthParameters& p) -> std::unique_ptr<IModule>                \
            { return std::make_unique<ClassName>(p); });                       \
        return true;                                                           \
    }()

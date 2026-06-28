#include "ModuleRegistry.h"

ModuleRegistry& ModuleRegistry::getInstance()
{
    static ModuleRegistry instance;
    return instance;
}

void ModuleRegistry::registerFactory(ModuleRegistry::Factory f)
{
    factories.push_back(std::move(f));
}

std::vector<std::unique_ptr<IModule>> ModuleRegistry::createAll(SynthParameters& params) const
{
    std::vector<std::unique_ptr<IModule>> result;
    for (auto& f : factories)
        result.push_back(f(params));
    return result;
}

juce::StringArray ModuleRegistry::getAvailableNames(SynthParameters& params) const
{
    juce::StringArray names;
    for (auto& f : factories)
        names.add(f(params)->getName());
    return names;
}

std::unique_ptr<IModule> ModuleRegistry::createByName(const juce::String& name, SynthParameters& params) const
{
    for (auto& f : factories)
    {
        auto inst = f(params);
        if (inst->getName() == name)
            return inst;
    }
    return nullptr;
}

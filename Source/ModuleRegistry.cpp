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

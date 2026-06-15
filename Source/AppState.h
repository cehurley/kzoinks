#pragma once

namespace juce { class PropertiesFile; }

// Returns the application's persistent properties file.
// Defined in Main.cpp where SynthApp owns ApplicationProperties.
// Returns nullptr only during early startup / late shutdown.
juce::PropertiesFile* getAppProperties();

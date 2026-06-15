#include <JuceHeader.h>
#include "MainComponent.h"
#include "AppState.h"

class SynthApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise(const juce::String&) override
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName        = getApplicationName();
        opts.filenameSuffix         = ".xml";
        opts.osxLibrarySubFolder    = "Application Support";
        appProperties.setStorageParameters(opts);

        mainWindow.reset(new MainWindow(getApplicationName()));

        // Register the "Setups" menu in the macOS system menu bar.
        // MainComponent must already exist (created inside MainWindow ctor).
        auto* mc = dynamic_cast<MainComponent*>(mainWindow->getContentComponent());
        if (mc)
        {
            juce::MenuBarModel::setMacMainMenu(mc->createMenuModel());

            // Auto-load the last used setup
            if (auto* props = getAppProperties())
            {
                auto last = props->getValue("setup.current");
                if (last.isNotEmpty())
                    mc->loadSetup(last);
            }
        }
    }

    void shutdown() override
    {
        juce::MenuBarModel::setMacMainMenu(nullptr);
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

    juce::ApplicationProperties appProperties;

    // ---- MainWindow --------------------------------------------------------
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);

            // Restore saved position; fall back to centred on first run
            bool restored = false;
            if (auto* props = getAppProperties())
            {
                auto state = props->getValue("mainWindow");
                if (state.isNotEmpty())
                {
                    restoreWindowStateFromString(state);
                    restored = true;
                }
            }
            if (!restored)
                centreWithSize(getWidth(), getHeight());

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            if (auto* props = getAppProperties())
            {
                props->setValue("mainWindow", getWindowStateAsString());
                props->saveIfNeeded();
            }
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

// Free function declared in AppState.h — resolves here where SynthApp is complete.
juce::PropertiesFile* getAppProperties()
{
    if (auto* app = dynamic_cast<SynthApp*>(juce::JUCEApplication::getInstance()))
        return app->appProperties.getUserSettings();
    return nullptr;
}

START_JUCE_APPLICATION(SynthApp)

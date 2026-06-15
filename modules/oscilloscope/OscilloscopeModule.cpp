#include "OscilloscopeModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

OscilloscopeModule::OscilloscopeModule(SynthParameters&) {}

void OscilloscopeModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    fifo.reset();
    ringBuffer.fill(0.0f);
}

void OscilloscopeModule::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&,
                                      int startSample, int numSamples)
{
    const int numCh = juce::jmin(audio.getNumChannels(), 2);
    if (numCh == 0) return;

    int s1, size1, s2, size2;
    fifo.prepareToWrite(numSamples, s1, size1, s2, size2);

    for (int i = 0; i < size1 + size2; ++i)
    {
        const int src = startSample + i;
        const int dst = (i < size1) ? s1 + i : s2 + (i - size1);
        float mix = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mix += audio.getSample(ch, src);
        ringBuffer[(size_t)dst] = mix / (float)numCh;
    }

    fifo.finishedWrite(size1 + size2);
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class OscilloscopeDisplay : public juce::Component, private juce::Timer
{
public:
    explicit OscilloscopeDisplay(OscilloscopeModule& m) : module(m)
    {
        displayBuf.assign(OscilloscopeModule::fifoSize, 0.0f);
        startTimerHz(30);
    }

    ~OscilloscopeDisplay() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.fillAll(juce::Colour(0xff020c06));

        // Title bar
        auto titleBar = bounds.removeFromTop(22);
        g.setColour(juce::Colour(0xff081a0e));
        g.fillRect(titleBar);
        g.setColour(juce::Colours::lightgreen.withAlpha(0.75f));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("OSCILLOSCOPE", titleBar, juce::Justification::centred);

        const float w  = (float)bounds.getWidth();
        const float h  = (float)bounds.getHeight();
        const float cx = (float)bounds.getX();
        const float cy = (float)bounds.getCentreY();

        // Subtle grid
        g.setColour(juce::Colour(0xff081a0e));
        g.drawHorizontalLine((int)cy,                    cx, cx + w);
        g.drawHorizontalLine((int)(cy - h * 0.25f),     cx, cx + w);
        g.drawHorizontalLine((int)(cy + h * 0.25f),     cx, cx + w);
        g.drawVerticalLine  (bounds.getX() + bounds.getWidth()  / 2,
                             (float)bounds.getY(), (float)bounds.getBottom());

        // Find most recent rising zero-crossing that leaves displaySamples to draw.
        // Search backward from near the end of the buffer.
        const int N       = OscilloscopeModule::fifoSize;
        const int dispN   = OscilloscopeModule::displaySamples;
        const int searchTo   = N - dispN - 2;
        const int searchFrom = juce::jmax(0, searchTo - dispN * 2);
        int triggerPos = searchFrom;

        for (int i = searchTo; i > searchFrom; --i)
        {
            if (displayBuf[(size_t)i] < 0.0f && displayBuf[(size_t)(i + 1)] >= 0.0f)
            {
                triggerPos = i + 1;
                break;
            }
        }

        // Draw waveform
        juce::Path path;
        for (int i = 0; i < dispN; ++i)
        {
            float x = cx + (float)i / (float)(dispN - 1) * w;
            float y = cy - displayBuf[(size_t)(triggerPos + i)] * h * 0.44f;
            y = juce::jlimit((float)bounds.getY() + 1.0f, (float)bounds.getBottom() - 1.0f, y);

            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }

        g.setColour(juce::Colour(0xff00ff88).withAlpha(0.88f));
        g.strokePath(path, juce::PathStrokeType(1.5f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        const int avail = module.fifo.getNumReady();
        if (avail <= 0) return;

        int s1, size1, s2, size2;
        module.fifo.prepareToRead(avail, s1, size1, s2, size2);

        const int total  = size1 + size2;
        const int bufSz  = OscilloscopeModule::fifoSize;

        if (total >= bufSz)
        {
            // Entire buffer replaced — copy last bufSz samples
            const int skip = total - bufSz;
            for (int i = 0; i < bufSz; ++i)
            {
                const int si = skip + i;
                displayBuf[(size_t)i] = (si < size1)
                    ? module.ringBuffer[(size_t)(s1 + si)]
                    : module.ringBuffer[(size_t)(s2 + si - size1)];
            }
        }
        else
        {
            // Slide window left and append new samples at the tail
            std::memmove(displayBuf.data(),
                         displayBuf.data() + total,
                         (size_t)(bufSz - total) * sizeof(float));
            const int tail = bufSz - total;
            for (int i = 0; i < size1; ++i)
                displayBuf[(size_t)(tail + i)]         = module.ringBuffer[(size_t)(s1 + i)];
            for (int i = 0; i < size2; ++i)
                displayBuf[(size_t)(tail + size1 + i)] = module.ringBuffer[(size_t)(s2 + i)];
        }

        module.fifo.finishedRead(size1 + size2);
        repaint();
    }

    OscilloscopeModule& module;
    std::vector<float>  displayBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeDisplay)
};

} // namespace

std::unique_ptr<juce::Component> OscilloscopeModule::createEditor()
{
    return std::make_unique<OscilloscopeDisplay>(*this);
}

REGISTER_MODULE(OscilloscopeModule);

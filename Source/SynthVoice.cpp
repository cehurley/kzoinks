#include "SynthVoice.h"

SynthVoice::SynthVoice(const SynthParameters& p) : params(p)
{
}

void SynthVoice::setCurrentSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
    ampEnv.setSampleRate(newSampleRate);
    filterEnv.setSampleRate(newSampleRate);
    // ~3ms time constant — fast enough to feel responsive, slow enough to prevent SVF blow-up
    smoothCoeff = 1.0f - std::exp(-1.0f / (0.003f * (float)newSampleRate));
}

void SynthVoice::noteStarted()
{
    svfLow = svfBand = dirtLP = 0.0f;
    smoothCutoff = params.filterCutoff.load();
    smoothRes    = params.filterRes.load();
    smoothDirt   = params.dirt.load();

    auto note    = getCurrentlyPlayingNote();
    noteVelocity = note.noteOnVelocity.asUnsignedFloat();
    notePressure = note.pressure.asUnsignedFloat();
    noteTimbre   = note.timbre.asUnsignedFloat();

    ampEnv.setParameters({ params.ampAttack.load(), params.ampDecay.load(),
                           params.ampSustain.load(), params.ampRelease.load() });
    filterEnv.setParameters({ params.filtAttack.load(), params.filtDecay.load(),
                              params.filtSustain.load(), params.filtRelease.load() });
    ampEnv.noteOn();
    filterEnv.noteOn();
}

void SynthVoice::noteStopped(bool allowTailOff)
{
    if (allowTailOff) {
        ampEnv.noteOff();
        filterEnv.noteOff();
    } else {
        ampEnv.reset();
        filterEnv.reset();
        clearCurrentNote();
    }
}

void SynthVoice::notePressureChanged()
{
    notePressure = getCurrentlyPlayingNote().pressure.asUnsignedFloat();
}

void SynthVoice::notePitchbendChanged() {}  // handled via getFrequencyInHertz()

void SynthVoice::noteTimbreChanged()
{
    noteTimbre = getCurrentlyPlayingNote().timbre.asUnsignedFloat();
}

void SynthVoice::noteKeyStateChanged() {}

// ---- oscillators ----

float SynthVoice::polyBlep(double t, double dt) const noexcept
{
    if (t < dt) {
        t /= dt;
        return (float)(t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return (float)(t * t + t + t + 1.0);
    }
    return 0.0f;
}

float SynthVoice::sawSample(double& phase, double freq) noexcept
{
    double dt = freq / sampleRate;
    float  s  = (float)(2.0 * phase - 1.0);
    s -= polyBlep(phase, dt);
    phase += dt;
    if (phase >= 1.0) phase -= 1.0;
    return s;
}

float SynthVoice::squareSample(double& phase, double freq) noexcept
{
    double dt = freq / sampleRate;
    float  s  = phase < 0.5 ? 1.0f : -1.0f;
    s += polyBlep(phase, dt);
    s -= polyBlep(std::fmod(phase + 0.5, 1.0), dt);
    phase += dt;
    if (phase >= 1.0) phase -= 1.0;
    return s;
}

float SynthVoice::sineSample(double& phase, double freq) noexcept
{
    double dt = freq / sampleRate;
    float  s  = (float)std::sin(juce::MathConstants<double>::twoPi * phase);
    phase += dt;
    if (phase >= 1.0) phase -= 1.0;
    return s;
}

// ---- filter ----

float SynthVoice::processSVF(float input, float cutoffHz, float q) noexcept
{
    float fc = juce::jlimit(20.0f, (float)(sampleRate / 3.0), cutoffHz);
    float f  = juce::jmin(2.0f * std::sin(juce::MathConstants<float>::pi
                           * fc / (float)sampleRate), 0.95f);

    // Jury stability condition on the SVF state matrix: f² + 2·f·q < 4
    // →  q_max = (4 - f²) / (2f).  Back off 2% so we approach self-oscillation
    // but never cross the instability boundary regardless of cutoff position.
    const float qMax = (4.0f - f * f) / (2.0f * f) * 0.98f;
    q = juce::jlimit(0.1f, qMax, q);

    svfLow        += f * svfBand;
    float svfHigh  = input - svfLow - q * svfBand;
    svfBand        = f * svfHigh + svfBand;

    // Belt-and-suspenders: if state somehow went non-finite, reset rather than
    // letting silence or garbage propagate through subsequent modules.
    if (!std::isfinite(svfLow) || !std::isfinite(svfBand))
        svfLow = svfBand = 0.0f;

    return svfLow;
}

// ---- render ----

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                 int startSample, int numSamples)
{
    if (!isActive()) return;

    // Update envelopes from shared params each block
    ampEnv.setParameters({ params.ampAttack.load(), params.ampDecay.load(),
                           params.ampSustain.load(), params.ampRelease.load() });
    filterEnv.setParameters({ params.filtAttack.load(), params.filtDecay.load(),
                              params.filtSustain.load(), params.filtRelease.load() });

    auto   note   = getCurrentlyPlayingNote();
    double freq   = note.getFrequencyInHertz();
    double freq2  = freq * std::pow(2.0, (double)params.detune.load() / 1200.0);

    float  baseCutoff   = params.filterCutoff.load();
    float  targetRes    = params.filterRes.load();
    float  filterEnvAmt = params.filterEnvAmt.load();
    float  targetDirt   = params.dirt.load();
    int    wave         = params.waveform.load();

    for (int i = 0; i < numSamples; ++i)
    {
        float ampGain    = ampEnv.getNextSample();
        float filterGain = filterEnv.getNextSample();

        float osc1, osc2;
        switch (wave)
        {
            case 1:  osc1 = squareSample(phase1, freq);  osc2 = squareSample(phase2, freq2); break;
            case 2:  osc1 = sineSample  (phase1, freq);  osc2 = sineSample  (phase2, freq2); break;
            default: osc1 = sawSample   (phase1, freq);  osc2 = sawSample   (phase2, freq2); break;
        }

        float osc = 0.5f * osc1 + 0.5f * osc2;

        // Slew parameters toward targets — prevents abrupt SVF coefficient jumps
        float targetCutoff = baseCutoff
                           + filterGain * filterEnvAmt * 6000.0f
                           + noteTimbre * noteTimbre * 3000.0f
                           + notePressure * 1500.0f;
        smoothCutoff += smoothCoeff * (targetCutoff - smoothCutoff);
        smoothRes    += smoothCoeff * (targetRes    - smoothRes);
        smoothDirt   += smoothCoeff * (targetDirt   - smoothDirt);

        // Dirt: one-pole LP seats the low end, soft-clip drives into the filter
        if (smoothDirt > 0.001f)
        {
            float drive  = 1.0f + smoothDirt * 9.0f;
            float lpCoeff = 0.15f;
            dirtLP += lpCoeff * (osc - dirtLP);
            float driven = osc + smoothDirt * (dirtLP * (drive - 1.0f));
            osc = std::tanh(driven * drive) / drive;
        }

        float filtered = processSVF(osc, smoothCutoff, smoothRes);
        float sample   = filtered * ampGain * noteVelocity * 0.4f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample(ch, startSample + i, sample);

        if (!ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

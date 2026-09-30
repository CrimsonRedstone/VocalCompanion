#include "DSP/Aeterna.h"
#include "Chain.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check (bool condition, const char* message) { if (! condition) throw std::runtime_error (message); }
void checkFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            check (std::isfinite (b.getSample (c, i)) && std::abs (b.getSample (c, i)) < 5, "Aeterna unstable output");
}
}
void runAeternaRegression()
{
    // Release compilation exposed a pre-existing WIDTH reset array overrun.
    vc::ImagerModule width;
    for (double rate : {44100.,48000.,96000.})
    {
        width.prepare (rate,257,2);
        for (int i = 0; i < 20; ++i) width.reset();
        juce::AudioBuffer<float> silence (2,257); silence.clear(); width.process (silence);
        checkFinite (silence);
    }
    check (std::abs (vc::AeternaModule::harmonyInterval (60, 0, 0, 2) - 4) < .01f, "C major third");
    check (std::abs (vc::AeternaModule::harmonyInterval (62, 0, 0, 2) - 3) < .01f, "D diatonic third");
    check (std::abs (vc::AeternaModule::harmonyInterval (60, 0, 1, 2) - 3) < .01f, "C minor third");
    check (std::abs (vc::AeternaModule::harmonyInterval (69, 9, 1, 4) - 7) < .01f, "A minor fifth");
    vc::Chain chain;
    chain.add (vc::ModuleType::Aeterna);
    const float settings[] {127,78,93,8,2,1};
    for (int i = 0; i < 6; ++i) chain.get (0)->setParam (i, settings[i]);
    vc::Chain restored; restored.fromValueTree (chain.toValueTree());
    check (restored.get (0)->getType() == vc::ModuleType::Aeterna, "Aeterna state type lost");
    for (int i = 0; i < 6; ++i) check (restored.get (0)->getParam (i) == settings[i], "Aeterna control state lost");
    for (double rate : {44100.,48000.,96000.})
    {
        vc::AeternaModule module;
        module.setParam (0, 0);
        module.prepare (rate, 128, 2);
        for (int count : {1,63,128,511,1024})
        {
            juce::AudioBuffer<float> b (2, count);
            for (int i = 0; i < count; ++i) { b.setSample (0,i,.1f * std::sin ((float) i)); b.setSample (1,i,.2f * std::cos ((float) i)); }
            juce::AudioBuffer<float> dry; dry.makeCopyOf (b); module.process (b);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < count; ++i) check (b.getSample (c,i) == dry.getSample (c,i), "Devotion zero is not exact dry");
        }
        module.setParam (0,127); module.setParam (1,100); module.setParam (2,100);
        juce::AudioBuffer<float> mono (1,257);
        for (int block = 0; block < 25; ++block)
        {
            for (int i=0;i<257;++i) mono.setSample (0,i,.25f*std::sin (vc::kTwoPi*220.f*(float)(block*257+i)/(float)rate));
            module.process (mono); checkFinite (mono);
        }
        check (std::abs (module.getMeter (2) - 57) < .2f, "Aeterna pitch detector failed 220 Hz");
        module.reset(); mono.clear(); module.process (mono);
        check (mono.getMagnitude (0,257) < 1.e-6f, "Aeterna reset retains audio");
        std::cout << "PASS: Aeterna dry, variable blocks, mono, pitch and reset at " << rate << " Hz\n";
    }
    vc::AeternaModule module;
    module.setParam (0,127); module.setParam (1,100); module.setParam (2,100);
    module.prepare (48000,256,2);
    juce::AudioBuffer<float> b (2,256);
    double earlyEnergy = 0, lateEnergy = 0, inputEnergy = 0, processedEnergy = 0;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    for (int block = 0; block < 2250; ++block) // 12 seconds, 2s voiced, 10s held tail
    {
        if (block == 375)
        {
            std::cout << "Aeterna benchmark: 2 s continuous full-Devotion input in "
                      << (juce::Time::getMillisecondCounterHiRes()-start)/1000 << " s (this build)\n";
            module.setParam (5,1);
        }
        for (int i = 0; i < 256; ++i)
        {
            float x = 0;
            if (block < 375)
            {
                const float phase = vc::kTwoPi * 220.f * (float) (block*256+i) / 48000.f;
                x = .2f * (std::sin (phase) + .4f*std::sin (phase*2) + .2f*std::sin (phase*3));
                inputEnergy += x*x;
            }
            b.setSample (0,i,x); b.setSample (1,i,x*.9f);
        }
        module.process (b); checkFinite (b);
        for (int i=0;i<256;++i)
        {
            const double energy = std::pow (b.getSample (0,i),2);
            if (block < 375) processedEnergy += energy;
            if (block >= 1500 && block < 1688) earlyEnergy += energy;
            if (block >= 2062) lateEnergy += energy;
        }
    }
    check (processedEnergy > inputEnergy * .1 && processedEnergy < inputEnergy * 8, "Aeterna level outside expected range");
    check (lateEnergy > 1.e-7 && lateEnergy > earlyEnergy * .1 && lateEnergy < earlyEnergy * 8, "Aeterna Hold does not sustain stably");
    check (std::isinf (module.tailLengthSeconds()), "Hold tail not reported infinite");
    module.setParam (5,0); check (module.tailLengthSeconds() >= 20, "Long tail not reported");
    std::cout << "PASS: Aeterna full choir/shimmer stability and Hold, late/early energy=" << lateEnergy/earlyEnergy
              << ", 12 s audio processed in " << (juce::Time::getMillisecondCounterHiRes()-start)/1000 << " s (this build)\n";
}

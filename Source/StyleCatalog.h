#pragma once
#include "Presets.h"

namespace vc
{

struct StyleEntry
{
    const char* name;
    const char* type;   // Lead Vocal, Backing Vocal, Chopped, Pitched, Robotic
    const char* color;  // Ambient, Bright, Chaos, Clean, Cute
    const char* info;
    const char* xml;
};

inline const char* kStyleTypes[]  = { "Lead Vocal", "Backing Vocal", "Chopped", "Pitched", "Robotic" };
inline const char* kStyleColors[] = { "Ambient", "Bright", "Chaos", "Clean", "Cute" };
inline constexpr int kNumStyleTypes  = 5;
inline constexpr int kNumStyleColors = 5;

inline const StyleEntry kStyles[] = {
    { "Cute", "Lead Vocal", "Cute",
      "Raised formants, small-tract pop lead. HPF 150 Hz + 4 kHz presence.",
      R"(<STATE in='1' out='0' name='Cute'><CHAIN>
  <MODULE type='gain' gain='2' mode='1' drive='0.2'/>
  <MODULE type='pitch' formant='3' retune='30' human='0.15'/>
  <MODULE type='dyneq' g1='-4' g4='3' air='4' dyn='0.45'/>
  <MODULE type='deesser' harsh='-22' sib='-18' sense='0.7'/>
  <MODULE type='fetcomp' thresh='-18' ratio='6' mix='0.85'/>
</CHAIN></STATE>)" },
    { "Shimmer", "Lead Vocal", "Bright",
      "Air injector + high shelf sparkle with light chorus.",
      R"(<STATE in='0' out='0' name='Shimmer'><CHAIN>
  <MODULE type='gain' mode='1' drive='0.15'/>
  <MODULE type='dyneq' air='6' g4='2' dyn='0.25'/>
  <MODULE type='chorus' rate='0.5' depth='2.2' mix='0.28'/>
  <MODULE type='imager' lo='18' hi='45' xover='250' trans='80'/>
  <MODULE type='reverb' size='0.55' mix='0.2' duck='0.8'/>
</CHAIN></STATE>)" },
    { "Whisper", "Backing Vocal", "Ambient",
      "Breathy, intimate, ducked hall. Low drive, heavy air.",
      R"(<STATE in='-4' out='0' name='Whisper'><CHAIN>
  <MODULE type='gain' gain='-2' mode='0' drive='0'/>
  <MODULE type='optocomp' thresh='-22' ratio='4' attack='10' release='220'/>
  <MODULE type='dyneq' g1='-3' air='5' dyn='0.2'/>
  <MODULE type='reverb' size='0.7' mix='0.32' duck='0.55'/>
</CHAIN></STATE>)" },
    { "Power", "Lead Vocal", "Bright",
      "Belted pop lead. FET smash + limiter + low-mid weight.",
      R"(<STATE in='3' out='-1' name='Power'><CHAIN>
  <MODULE type='gain' gain='4' mode='2' drive='0.4'/>
  <MODULE type='fetcomp' input='6' thresh='-12' ratio='12' attack='0.1' release='90'/>
  <MODULE type='dyneq' g2='2' g4='2' air='3' dyn='0.5'/>
  <MODULE type='deesser' range='10'/>
  <MODULE type='limiter' ceil='-0.3' rel='60' makeup='1'/>
</CHAIN></STATE>)" },
    { "Rock / Growl", "Lead Vocal", "Chaos",
      "Dirty tube, mid grit, hard FET. Throat bite.",
      R"(<STATE in='4' out='-1' name='Rock / Growl'><CHAIN>
  <MODULE type='gain' gain='6' mode='2' drive='0.75'/>
  <MODULE type='fetcomp' input='8' thresh='-10' ratio='20' attack='0.05' release='80'/>
  <MODULE type='dyneq' g3='3' g4='-1' air='1' dyn='0.3'/>
  <MODULE type='pitch' formant='-2' retune='80'/>
</CHAIN></STATE>)" },
    { "Mellow / Soft", "Backing Vocal", "Clean",
      "Dark, warm opto. Low-pass air cut, no sizzle.",
      R"(<STATE in='0' out='0' name='Mellow / Soft'><CHAIN>
  <MODULE type='gain' mode='0' drive='0'/>
  <MODULE type='optocomp' thresh='-20' ratio='3' attack='25' release='400' makeup='3'/>
  <MODULE type='dyneq' g2='2' air='-3' dyn='0.2'/>
  <MODULE type='reverb' size='0.35' mix='0.12' duck='0.7'/>
</CHAIN></STATE>)" },
    { "Robot", "Robotic", "Chaos",
      "Hard pitch snap, no air, band-limited.",
      R"(<STATE in='0' out='0' name='Robot'><CHAIN>
  <MODULE type='pitch' scale='3' retune='0' human='0' formant='0'/>
  <MODULE type='dyneq' g1='-6' g2='-2' g3='4' air='-4' dyn='0.1'/>
  <MODULE type='phaser' rate='0.15' mix='0.35'/>
  <MODULE type='fetcomp' thresh='-16' ratio='8'/>
</CHAIN></STATE>)" },
    { "Radio / Telephone", "Chopped", "Chaos",
      "Bandpass 400 Hz–3 kHz, smashed, dirty.",
      R"(<STATE in='6' out='-2' name='Radio / Telephone'><CHAIN>
  <MODULE type='gain' gain='5' mode='2' drive='0.55'/>
  <MODULE type='dyneq' g1='-12' g2='-4' g3='5' g4='-2' air='-8' dyn='0.2'/>
  <MODULE type='fetcomp' thresh='-8' ratio='20' attack='0.02' release='60'/>
  <MODULE type='deesser' range='6'/>
</CHAIN></STATE>)" },
    { "Guaranteed", "Lead Vocal", "Clean",
      "Commercial punchy lead. Tight FET, 4 kHz presence.",
      R"(<STATE in='1' out='0' name='Guaranteed'><CHAIN>
  <MODULE type='gain' gain='2' mode='1' drive='0.25'/>
  <MODULE type='deesser' harsh='-20' sib='-16' sense='0.75'/>
  <MODULE type='fetcomp' thresh='-16' ratio='8' attack='0.3' release='160'/>
  <MODULE type='dyneq' g4='3' air='3' dyn='0.5'/>
  <MODULE type='limiter' ceil='-0.4' rel='70'/>
</CHAIN></STATE>)" },
    { "Here and Now", "Lead Vocal", "Bright",
      "In-your-face pop. Saturation, fast limiter, 1/16 delay.",
      R"(<STATE in='2' out='-1' name='Here and Now'><CHAIN>
  <MODULE type='gain' gain='3' mode='2' drive='0.35'/>
  <MODULE type='pitch' formant='0.5' retune='40'/>
  <MODULE type='fetcomp' thresh='-12' ratio='10' attack='0.08'/>
  <MODULE type='delay' sync='1' div='1' mix='0.12' ping='1'/>
  <MODULE type='limiter' ceil='-0.3'/>
</CHAIN></STATE>)" },
    { "Hometown", "Lead Vocal", "Clean",
      "Warm acoustic. Opto, 200 Hz body, wood room.",
      R"(<STATE in='0' out='0' name='Hometown'><CHAIN>
  <MODULE type='gain' mode='0'/>
  <MODULE type='optocomp' thresh='-18' ratio='3' attack='18' release='320' makeup='2'/>
  <MODULE type='dyneq' g2='2' air='1' dyn='0.25'/>
  <MODULE type='reverb' size='0.32' mix='0.14' duck='0.65'/>
</CHAIN></STATE>)" },
    { "Intense Connection", "Lead Vocal", "Bright",
      "Wall-of-sound chorus. Heavy exciter, 2-voice chorus.",
      R"(<STATE in='2' out='-1' name='Intense Connection'><CHAIN>
  <MODULE type='gain' gain='2' mode='1' drive='0.4'/>
  <MODULE type='pitch' formant='1' retune='25'/>
  <MODULE type='fetcomp' thresh='-14' ratio='8'/>
  <MODULE type='dyneq' air='5' g4='3' dyn='0.4'/>
  <MODULE type='chorus' voices='4' mix='0.32' depth='2.4'/>
  <MODULE type='limiter' ceil='-0.3'/>
</CHAIN></STATE>)" },
    { "Interference", "Chopped", "Chaos",
      "Broken transmission. Narrow band, phaser, bit of chaos.",
      R"(<STATE in='3' out='-1' name='Interference'><CHAIN>
  <MODULE type='gain' mode='2' drive='0.6'/>
  <MODULE type='dyneq' g1='-10' g2='-2' g3='4' air='-6' dyn='0.15'/>
  <MODULE type='phaser' rate='2.2' depth='0.9' mix='0.55'/>
  <MODULE type='tremolo' rate='8' depth='0.35'/>
  <MODULE type='delay' mix='0.2' fb='0.45'/>
</CHAIN></STATE>)" },
    { "Ambient Dream", "Backing Vocal", "Ambient",
      "Washed pad vocal. LPF, huge ducking reverb + 1/4 delay.",
      R"(<STATE in='-2' out='0' name='Ambient Dream'><CHAIN>
  <MODULE type='gain' gain='-1' mode='0'/>
  <MODULE type='pitch' formant='-1' retune='60'/>
  <MODULE type='dyneq' air='-2' g4='-2' dyn='0.15'/>
  <MODULE type='delay' sync='1' mix='0.28' fb='0.4'/>
  <MODULE type='reverb' size='0.85' mix='0.55' duck='0.4'/>
</CHAIN></STATE>)" },
    { "Crystal Lead", "Lead Vocal", "Bright",
      "Glass-top pop. Air, stereoize, short plate.",
      R"(<STATE in='1' out='0' name='Crystal Lead'><CHAIN>
  <MODULE type='gain' mode='1' drive='0.18'/>
  <MODULE type='deesser' sib='-20' sense='0.8'/>
  <MODULE type='optocomp' thresh='-18' ratio='3'/>
  <MODULE type='dyneq' air='5.5' g4='2' dyn='0.3'/>
  <MODULE type='imager' lo='15' hi='40' xover='300' trans='80'/>
  <MODULE type='reverb' size='0.28' mix='0.12' duck='0.85'/>
</CHAIN></STATE>)" },

    { "Mouth", "Lead Vocal", "Cute",
      "Throat-forward GEN mouth. Formant jump + mid bite.",
      R"(<STATE in='1' out='0' name='Mouth'><CHAIN>
  <MODULE type='gain' gain='2' mode='1'/>
  <MODULE type='pitch' formant='5' retune='50' key='0'/>
  <MODULE type='paraeq' g2='3' hp='120'/>
  <MODULE type='fetcomp' thresh='-16' ratio='8'/>
</CHAIN></STATE>)" },
    { "Character", "Lead Vocal", "Cute",
      "Raised-formant tract size, light air, tight de-ess.",
      R"(<STATE in='0' out='0' name='Character'><CHAIN>
  <MODULE type='gain' mode='1'/>
  <MODULE type='pitch' formant='4' retune='35' key='0'/>
  <MODULE type='air' amt='0.4' duck='0.6'/>
  <MODULE type='deesser'/>
  <MODULE type='optocomp'/>
</CHAIN></STATE>)" },
    { "Air / BRE", "Lead Vocal", "Bright",
      "Ducked breath layer sitting above 8 kHz.",
      R"(<STATE in='0' out='0' name='Air / BRE'><CHAIN>
  <MODULE type='gain'/>
  <MODULE type='air' amt='0.7' duck='0.4'/>
  <MODULE type='deesser'/>
  <MODULE type='optocomp'/>
  <MODULE type='dyneq' air='5'/>
</CHAIN></STATE>)" },
    { "Vocoder", "Robotic", "Chaos",
      "Hard quantize + ring carrier for vocoder colour.",
      R"(<STATE in='1' out='0' name='Vocoder'><CHAIN>
  <MODULE type='pitch' retune='5' scale='3' key='0'/>
  <MODULE type='ringmod' freq='90' mix='0.28'/>
  <MODULE type='paraeq' hp='200' lp='6000'/>
  <MODULE type='fetcomp'/>
</CHAIN></STATE>)" },
    { "Chaos", "Chopped", "Chaos",
      "Bitcrush stutter + downsample grit, safety limiter.",
      R"(<STATE in='0' out='-1' name='Chaos'><CHAIN>
  <MODULE type='gain' mode='2' drive='0.4'/>
  <MODULE type='bitcrush' bits='6' rate='0.45' mix='0.55'/>
  <MODULE type='ringmod' freq='140' mix='0.3'/>
  <MODULE type='delay' mix='0.2'/>
  <MODULE type='limiter' ceil='-1.5'/>
</CHAIN></STATE>)" },
    { "Glitch Stutter", "Chopped", "Chaos",
      "Downsampled chops, square tremolo, ping delay.",
      R"(<STATE in='1' out='-1' name='Glitch Stutter'><CHAIN>
  <MODULE type='bitcrush' bits='5' mix='0.5'/>
  <MODULE type='tremolo' shape='1' depth='0.7' rate='12'/>
  <MODULE type='delay' ping='1' mix='0.2'/>
  <MODULE type='paraeq' hp='180'/>
  <MODULE type='limiter' ceil='-1'/>
</CHAIN></STATE>)" },
    { "Distant Dream", "Backing Vocal", "Ambient",
      "Far-away plate, dark LPF, slow chorus.",
      R"(<STATE in='-1' out='0' name='Distant Dream'><CHAIN>
  <MODULE type='gain' mode='0'/>
  <MODULE type='paraeq' lp='5500' g1='-3'/>
  <MODULE type='chorus' rate='0.15' mix='0.4'/>
  <MODULE type='delay' mix='0.22'/>
  <MODULE type='reverb' size='0.9' mix='0.48' duck='0.3'/>
</CHAIN></STATE>)" },
    { "Backing Stack", "Backing Vocal", "Clean",
      "Micro-detune chorus + gentle width for stacked harmonies.",
      R"(<STATE in='0' out='0' name='Backing Stack'><CHAIN>
  <MODULE type='gain'/>
  <MODULE type='optocomp'/>
  <MODULE type='chorus' mix='0.4' voices='4' depth='3.2'/>
  <MODULE type='imager' lo='8' hi='55' xover='250' trans='70'/>
  <MODULE type='reverb' mix='0.16'/>
</CHAIN></STATE>)" },
    { "Anime Chorus", "Lead Vocal", "Cute",
      "Cute-lead polish: raised formant, air, chorus, width.",
      R"(<STATE in='1' out='0' name='Anime Chorus'><CHAIN>
  <MODULE type='gain' mode='1'/>
  <MODULE type='pitch' formant='2.5' retune='28' key='0'/>
  <MODULE type='air' amt='0.4'/>
  <MODULE type='deesser'/>
  <MODULE type='fetcomp'/>
  <MODULE type='chorus' mix='0.22'/>
  <MODULE type='imager' lo='18' hi='42' xover='280' trans='80'/>
  <MODULE type='limiter' ceil='-0.3'/>
</CHAIN></STATE>)" },
    { "Club Smash", "Lead Vocal", "Bright",
      "In-your-face club vocal. Heavy limiter, presence, width.",
      R"(<STATE in='3' out='-1' name='Club Smash'><CHAIN>
  <MODULE type='gain' gain='5' mode='2' drive='0.35'/>
  <MODULE type='fetcomp' thresh='-12' ratio='12'/>
  <MODULE type='dyneq' g4='4' air='3'/>
  <MODULE type='imager' lo='12' hi='55' xover='250' trans='75'/>
  <MODULE type='mseq' shi='2'/>
  <MODULE type='limiter' ceil='-0.2' makeup='2'/>
</CHAIN></STATE>)" },
    { "Chipmunk", "Pitched", "Cute",
      "Tiny tract, fast retune, bright air.",
      R"(<STATE in='1' out='0' name='Chipmunk'><CHAIN>
  <MODULE type='pitch' formant='9' retune='8' key='0'/>
  <MODULE type='air' amt='0.5'/>
  <MODULE type='dyneq' air='4'/>
  <MODULE type='delay' mix='0.12'/>
  <MODULE type='limiter' ceil='-0.5'/>
</CHAIN></STATE>)" },
    { "Demon", "Pitched", "Chaos",
      "Huge tract, low formant, dark warmth, growl harmonics.",
      R"(<STATE in='2' out='0' name='Demon'><CHAIN>
  <MODULE type='pitch' formant='-8' retune='60' key='0'/>
  <MODULE type='exciter' harm='0.2' warm='0.6'/>
  <MODULE type='paraeq' hp='60' g1='3' lp='7000'/>
  <MODULE type='fetcomp'/>
</CHAIN></STATE>)" },
    { "Octave Chop", "Pitched", "Chaos",
      "Formant jump + tremolo slice. Playful pitched FX.",
      R"(<STATE in='2' out='0' name='Octave Chop'><CHAIN>
  <MODULE type='pitch' formant='7' retune='10' human='0.05' scale='2'/>
  <MODULE type='tremolo' rate='12' depth='0.55' shape='1'/>
  <MODULE type='delay' sync='1' div='0' mix='0.18'/>
  <MODULE type='dyneq' g1='-4' air='3'/>
</CHAIN></STATE>)" },
    { "Robotic Tuning", "Robotic", "Chaos",
      "Hard snap, no flex, no humanize. Instant scale lock.",
      R"(<STATE in='0' out='0' name='Robotic Tuning'><CHAIN>
  <MODULE type='gain' mode='1' drive='0.15'/>
  <MODULE type='deesser' sense='0.65'/>
  <MODULE type='autotune' retune='0' flex='0' human='0' throat='0' key='0' scale='1'/>
  <MODULE type='fetcomp' thresh='-16' ratio='8'/>
  <MODULE type='dyneq' g1='-4' g3='3' air='-2' dyn='0.15'/>
</CHAIN></STATE>)" },
    { "Lead Polish", "Lead Vocal", "Clean",
      "Natural correction: 22 ms retune, 20 c flex, 45 percent humanize.",
      R"(<STATE in='1' out='0' name='Lead Polish'><CHAIN>
  <MODULE type='gain' gain='2' mode='1' drive='0.2'/>
  <MODULE type='deesser' harsh='-20' sib='-16' sense='0.7'/>
  <MODULE type='autotune' retune='22' flex='20' human='45' throat='0' key='0' scale='1'/>
  <MODULE type='fetcomp' thresh='-18' ratio='6' mix='0.85'/>
  <MODULE type='dyneq' g4='2' air='3' dyn='0.4'/>
  <MODULE type='limiter' ceil='-0.3'/>
</CHAIN></STATE>)" },
};

inline constexpr int kNumStyles = 30;

inline juce::ValueTree getStyleTree (int index)
{
    if (index < 0 || index >= kNumStyles) return {};
    if (auto xml = juce::XmlDocument::parse (kStyles[index].xml))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

inline int findStyleIndex (const juce::String& name)
{
    for (int i = 0; i < kNumStyles; ++i)
        if (name == kStyles[i].name) return i;
    return -1;
}

} // namespace vc

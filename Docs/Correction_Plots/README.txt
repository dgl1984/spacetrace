SpaceTrace dataset-correction response plots

These PNGs are documentation only. SpaceTrace does not read them at runtime. They
are generated from the actual shipped correction WAVs and retained Tone Trace models
with scripts/plot_correction.py. Every PNG has a matching .txt description derived
from the same numeric curve.

The standard plots use 20 Hz-20 kHz on a logarithmic frequency axis and a common
-18 dB to +26 dB vertical range so the five included heads can be compared without
decorative styling or changing scales from one head to another.

For each head there are two useful views when the source model is retained:
- *_correction_response.png: magnitude response of the actual correction WAV shipped.
- *_ToneTrace_model.png: the original Tone Trace model curve.

The files in this directory describe the global Mono Point Source correction.
StereoPair/ contains separate left/right source-leg response plots for the updated
Stereo Pair corrections on heads 1, 3 and 4, all using -18 to +30 dB so the full
curves fit without clipping. Each is generated from the actual
combined WAV: original correction convolved with the same common residual for
both legs. Each PNG has a matching accessible text description. These curves
exclude the separate source gains, ear gains, head trim and pair trim, all of
which retain their previous values. The original models and exact composition
inputs are retained in Docs/Correction_Models/StereoPair/.

The combined WAV plots include their final 40 ms half-cosine tail fade to zero.
This is a time-domain taper; no normalization or gain calibration was added.

The paired text description reports the strongest boost, deepest cut, and broad-band
means. It intentionally does not claim anything about localization quality or
subjective preference.

FULL2DEG KU100 correction provenance
-----------------------------------
The user-approved FULL2DEG Tone Trace model is retained as
../Correction_Models/FULL2DEG_KU100_ToneTrace.ttm. The final correction WAV uses
the same curve with one constant -0.0576 dB head-aware normalization offset so the
FULL2DEG Raw and Dataset Corrected modes have essentially the same average level.
Head-to-head loudness matching is separate in levelTrimDb.

FABIAN HATO 0 correction provenance
----------------------------------
The user-approved FABIAN Tone Trace model is retained as
../Correction_Models/FABIAN_HATO0_ToneTrace.ttm. FABIAN likewise uses one constant
normalization offset to preserve the approved EQ shape while keeping Raw and Dataset
Corrected sensibly level matched. Its much larger head-to-head loudness trim remains
separate in the manifest.

Licensing / provenance boundary
-------------------------------
These files document head-specific response/correction material; they are not covered
by one blanket SpaceTrace licence. Use Licenses/THIRD_PARTY_NOTICES.txt and the
matching Heads/<head>/LICENSE.txt as the authority. In particular, SpaceTrace treats
the FULL2DEG-specific model/correction/plots/descriptions as CC BY-SA 3.0 adapted
material, and the FABIAN-specific response-derived material under the FABIAN CC BY
4.0 attribution boundary. The IRCAM, MIT KEMAR and SADIE material retains the
notices/attribution conditions identified in their head packages.

SpaceTrace Stereo Pair Tone Trace models

These 48 kHz Custom Max models describe the original side-reference corrections.
For heads 1, 3 and 4, the final shipping responses also include the common residual
described below. The original models alone do not represent those combined responses.

For each head, the 90-degree model is the input-Left source leg at centered Stereo Pair geometry and the 270-degree model is the input-Right source leg. The correction is common to both ears of its source leg.

MIT KEMAR Normal and FABIAN HATO 0 include small listening-driven common refinements after pink-noise audition revealed residual sibilance-prone peaks. The refinements are deliberately broad and are applied equally to the 90/270 model pair for that head:

- MIT KEMAR: -1.5 dB centered 7.5 kHz (0.35-octave FWHM) and -2.2 dB centered 12.15 kHz (0.42-octave FWHM).
- FABIAN HATO 0: -0.8 dB centered 6.45 kHz (0.35-octave FWHM) and -1.8 dB centered 10.7 kHz (0.42-octave FWHM).

These common refinements do not alter the within-leg left/right HRTF magnitude ratio or ITD. The final correction WAVs are regenerated with the released Tone Trace 1.0.5 minimum-phase IR renderer, then passed through scripts/prepare_stereo_pair.py so constant normalization, source balance and pair trim are recalculated rather than hidden in the EQ curve.

Additional common residuals for heads 1, 3 and 4 (2026-10-05)
---------------------------------------------------------
IRCAM LISTEN 1050, SADIE II D1 and KU100 FULL2DEG each have one additional Tone
Trace residual captured with Dataset Corrected Stereo Pair active, otherwise
default plugin settings, and centered host pan. The same residual is convolved
offline with both existing source-leg correction WAVs. This preserves the existing
correction and adds the residual without another runtime convolution stage.

Components/<head>/ retains base_left.wav, base_right.wav, residual.wav and the
original residual.ttm. These are the exact inputs, including their original gain.
The head provenance.json records hashes and the recipe. Use compose_corrections.py
on the preserved base inputs, never on an already combined response. No new
normalization or source/ear balance or pair/head trim adjustment is applied.
The result is 17279 samples at 48 kHz, approximately 360 ms. The host tail report
is 400 ms to include the correction and shipping HRTF tail. This longer filter
can require more processing despite keeping the same number of convolutions.

The final 40 ms of each combined response now has a half-cosine fade to exact
zero. The fade begins at unity with a smooth slope and leaves the preceding
response intact. There is no gain makeup. Measured magnitude changes are below
0.001 dB over 20 Hz-20 kHz; each head's provenance records the actual maximum.
Reproduction requires --tail-fade-ms 40 after selecting the preserved base and
residual inputs. The raw component WAVs and Tone Trace models remain unchanged.

These residuals stay active at all Stereo Pair positions in Dataset Corrected
mode. Heads 2 and 5, Mono Point Source, Raw and Custom Correction are unchanged.
The original 90/270 models and the residual models remain separate measurement
records; there is no invented merged measurement model. The final actual WAV
responses are plotted in Docs/Correction_Plots/StereoPair with text descriptions.

The corresponding Heads/<head>/LICENSE.txt and Licenses/THIRD_PARTY_NOTICES.txt
terms also apply to these head-derived components and plots. FULL2DEG-specific
material retains the CC BY-SA 3.0 attribution and share-alike boundary.

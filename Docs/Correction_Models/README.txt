SpaceTrace retained Tone Trace correction models
================================================

These .ttm files are retained so the shipped fixed dataset corrections are auditable
and reproducible. SpaceTrace itself does not read .ttm files at runtime.

Each model belongs to the head named in its filename and was produced from that
head's response using the documented pink-noise workflow. Do not transplant a model
to another head merely because both measurements used a similar dummy-head model.

Licensing / provenance
----------------------
The models are head-specific response-derived material. They are not placed under one
blanket SpaceTrace licence. See Licenses/THIRD_PARTY_NOTICES.txt and each matching
Heads/<head>/LICENSE.txt. SpaceTrace conservatively distributes the FULL2DEG model
under CC BY-SA 3.0 and the FABIAN response-derived material with the FABIAN CC BY 4.0
attribution/change boundary. IRCAM LISTEN, MIT KEMAR and SADIE II retain their own
notices/attribution requirements or requests as documented in the release.

Stereo Pair 90°/270° Tone Trace models are retained under StereoPair/. Those models are the reproducibility source for the optional pre-sum Stereo Pair correction IRs; KEMAR and FABIAN include documented small listening-driven common refinements.

# Aeterna animation

Selected unchanged from the supplied `Aeterna_Singing_Velocity(1).zip`.
Only the four final phase atlases are required by the plugin. Duplicate strips,
preview GIFs, HTML viewers, older anchors and generation tools are omitted.

Each RGBA atlas is 2048 x 1536, a 16 x 8 grid of 128 x 192 frames.
Velocity is round(clamp(Devotion, 0, 127)); x=(velocity%16)*128,
y=(velocity/16)*192. Phase chooses atlas 0..3. Fixed pivot: (64,184).
Zero: resting A pose; 64: prayer; 127: raised Y pose. Supplied breathing is
four frames, not lip sync. Keep nearest-neighbor rendering and never blend
neighboring pose images. UI smooths the numeric velocity only.

Artwork remains the supplied user's artwork; no new third-party art was added.

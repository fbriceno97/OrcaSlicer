# Cut-plane section frames

Cut planes belong to a model object and use that object's local coordinates. They
do not destructively split its volumes. Both 3MF writers store the stack as
versioned object metadata, including disabled planes and independent build
directions. A project without this metadata retains an empty, disabled stack.

The partition uses ordered peel semantics: each plane takes material not claimed
by a later plane. A stack of N planes therefore produces at most N + 1 sections.
Directed model parts are excluded from this partition and receive independent
section descriptors. Negative volumes and parameter modifiers remain available
to every section.

A whole-object build direction selects the root's world-space build normal. When
there is no splitting plane, the object receives one root descriptor. Invalid
planes reject derivation rather than silently falling back to directed parts.

`Print::apply` derives descriptors for each distinct instance transform. Section
identity includes the model object, instance identity, transform, and section
index. Copies with identical transforms remain separate when they carry sections;
their eventual machine-space placements differ. An unchanged descriptor reuses
its PrintObject. Descriptor changes invalidate its slicing state. Disabling the
printer's non-planar option restores ordinary instance grouping.

For a section, `PrintObject::trafo_sliced()` is the object-to-slice affine map
`s = A*x + t`. The build normal is mapped from object space by the inverse
transpose of the instance transform, then rotated onto slicing +Z. The map also
centers XY and subtracts the descriptor's build-axis base. Section height ranges
already include instance shrinkage compensation; layer scheduling does not apply
that compensation a second time. A sunk root building vertically keeps its base
at zero so buried material is not lifted above the bed by rebasing.

`PrintInstance::get_slice_to_world_transform()` reverses this slicing frame and
restores the copy's XY placement. Scaling, mirroring and shrinkage remain in the
geometry; the section-to-world map is rigid. Writer input also contains the
current G-code origin, which must be subtracted before applying this map. Plate
and nozzle offsets and Z calibration are physical corrections applied afterward.
During a section export, custom-G-code position variables use machine
coordinates, including plate subtraction, both inside mapped sections and
between them. Within a mapped section they use the last emitted endpoint. Raw custom G-code does
not pass through the writer again. Returning a plate-global position here would
shift a macro move by the plate offset a second time.
The initial toolpath preview uses the same per-copy inverse slicing map to place
section perimeter, infill and support vertices back in world geometry. Machine
calibration is excluded from this model-space view.
Section preview keeps local layer order in separate slider ranges rather than
sorting vertices by world Z. It waits for both model and support rows at each
height before compacting empty planning rows, so an empty model row cannot
remove a support-only slider height. A tilted layer has varying world Z, and an upper
section's rebased heights cannot be compared with its physical coordinates.

The preliminary planar path-conflict checker groups sections of the same
physical ModelInstance under one identity. Their rebased Z schedules cannot be
compared as world heights, and the partition deliberately shares cut faces.
Different physical copies retain different identities. Full section clearance
still runs against emitted machine coordinates before G-code becomes exportable.

A model-space bound `n*x <= d` becomes `n_s*s <= d + n_s*t`, where
`n_s = inverse(A).transpose()*n`. At each slice height this is an XY half-plane.
Horizontal bounds either keep or discard the entire slice. Bed bounds include
half the bead height projected onto the bed normal. Clipping happens on region
polygons, preserving their material and modifier assignments. Parts belonging to
another section retain empty slice slots so modifier parent indices remain valid.
Section processing avoids region bounding-box shortcuts whose coordinates belong
to the ordinary slicing frame.
Bounds are reapplied after conical overhang processing and contour compensation,
including uncompensated first-layer outlines, so these operations cannot grow
material back across a cut after the initial mesh clipping.

Normal and organic support installation clips generated layer polygons and
contact, overhang and enforcer footprints against the section's bounds over the
whole support layer-height span before installing support layers. Floors and ceilings both limit this region; only
floors provide load-bearing heights. Normal and classic-tree overhang detection
count floor contact from the actual previous slice height through the current
layer as supporting material. This includes shallow cut faces and Z anti-aliasing
sampling offsets rather than assuming mid-layer sampling. Classic
tree also excludes floor-supported first-layer strips from sharp-tail seeds.
Classic-tree descent terminates only when an entire circular or polygon foot
reaches a load-bearing floor; disk tests include the radius in every XY direction.
Ceilings and vertical or downward-facing floors cannot terminate descent.
Ceilings never suppress overhangs this way. The physical world-Z build plate is carried
into the section frame even for a tilted root. Ordinary objects leave this
clipping inactive. A final containment pass applies to all support generators,
including classic tree: it clips expanded paths at full bead width and height,
retains flow metadata and island ordering, opens loops at boundaries, and rebuilds
support footprints from the resulting paths. It rejects contoured or sloped support
paths rather than flattening their Z data. Support-load projection groups plane sections by stable model-instance identity,
visits children before receivers and lifts each footprint onto its authored floor
before changing frames. Independent cutting and build normals therefore retain
the correct contact position. A receiver carries only load not already caught by
its material; a shelf that catches load prevents it reappearing below. Projection
rejects cross-instance handoffs and unrepresentable receiver planes. Normal
support generation consumes fresh inherited load in child-first order,
with separate model-contact gaps and flush support-column contacts. Extra empty
planning layers let contacts reach the receiving ceiling; they are omitted from
export and progress counts. Support enable/gap changes invalidate that schedule.
Slicing-cache equivalence includes section and instance identity, and transient
load is rebuilt on every family generation. Classic-tree generation seeds fresh inherited footprints on virtual overhang
rows after island pruning. Those rows account for the tree's contact gap, keeping
model contacts separate from flush support-column contacts. Overhang category
pointers are recorded after all seed polygons have been appended. Organic
inheritance maps contacts using the generator's rounded gap-layer count and
uniform support grid. Receivers with variable layer-height grids are rejected;
a shorter final planning row does not change the contact grid. Support below
floating bases is rejected by the family generator.

Print validation, processing and export enforce the same section compatibility
rules. Cut-plane sections use an upright root at the physical bed, one printing
filament, By object sequencing, and no prime tower, raft, spiral vase or draft
shield. Child build directions follow their supporting cut-plane normals.
Independent part directions, tilted/floating roots and floating child bases are
rejected with a setting-specific error. The printer's hotend/gantry model also
limits the allowed build direction. Projects without active derived sections
retain their ordinary processing path. A sequential export scope binds the per-copy frame, writes unique section/build-axis metadata,
resets cooling in physical coordinates and closes the frame before global motion.
Exception cleanup restores the ordinary writer frame. Entering a section marks
the logical position as uncommanded until the first absolute XYZ move. The writer
retains the last emitted endpoint independently of planned layer Z, including
global transitions after a complete position has been established. Global
XY-only commands preserve the emitted Z; Z-only commands preserve emitted XY.
Arcs update the XY endpoint and positive-extrusion height. Resetting the writer
configuration clears this transient position.
Custom-position variables, frame exit and in-place lift checks use that endpoint,
so planning a layer cannot teleport macro coordinates or trigger a lift through a
pose the printer has not reached. The `max_layer_z` macro override uses the highest generated positive-extrusion
machine endpoint, including Z calibration, seeded from processed startup material.
Descending deposition includes its starting height; planned positions, travel and
retracting wipes do not raise this maximum. Caller override data stays unchanged.
The `layer_z` override intersects the local layer plane with a vertical line
through the last emitted machine XY, including plate/nozzle offsets and Z
calibration. It does not transform an uncommanded local endpoint, which would
also move XY on tilted sections. Non-upward planes are rejected. Macros that
move XY before using this height still require separate integration. Within an
active section, parsed linear/arc XYZ endpoints synchronize the emitted pose,
including relative moves and temporary inch units. Macros must restore absolute
XYZ mode, millimeters and the configured extrusion mode. Conditional mode changes,
arc-plane/arc-center mode changes, unmodeled XYZ commands, homing and
machine-origin/workspace changes reject the
template rather than leaving a guessed position. Explicit position-variable
assignments retain their existing authoritative behavior. Raw XYZ motion receives
the writer's conservative physical feed cap, including explicit feed commands
and temporary inch units. E-only `G92 E0` precision resets are accepted in
relative extrusion mode and reset the writer's shared E coordinate without
changing physical pose or used-filament accounting. Raw extrusion, XYZ coordinate resets, motion-limit changes
and conditional XYZ moves are rejected inside sections. Firmware conditionals
must be balanced within the macro. Startup/global macros retain their existing
behavior and remain covered by chronological hotend validation.
Whole-layer bed settings are
limited to an upright root whose slice origin is at the physical bed. Per-path
first-layer settings use the section point's physical bed height; elevated child
sections do not restart the bed speed ramp. The exporter resets local layer
height caches at section boundaries and uses actual section-row thicknesses.
Elevated children retain normal temperatures and skip the physical next-object
adhesion macro; upright bed roots retain its normal behavior. Their cooling
schedule also bypasses initial bed fan overrides, fan-off periods and fan ramps,
while keeping each local layer's filament-variant configuration. Wave fan
controls remain available on elevated sections. Legacy last-Z-only macro fallbacks
are skipped inside sections because full endpoint parsing already tracks relative
motion and unit changes. Rafts and draft shields across sections are rejected
by the compatibility check. Brims and finite-height skirts belong only to upright
bed roots. Volume-group brim generation, bed hulls and skirt grouping exclude
rebased children, and section emission never repeats bed adhesion at an elevated
local layer zero. Combined/per-object grouping still distinguishes physical
copies; the automatic brim calculation retains the full model-volume height.
Skirts stop when their bed root's local schedule ends.
Timelapse parking is omitted in active sections, including farthest-point camera
placement, with a validation warning. Ordinary prints retain their timelapse
behavior. Unsupported directed-part descriptors are rejected rather than falling
back to ordinary planar output.

Sequential clearance groups sections by their physical model instance. It uses
the complete compensated model footprint and maximum world Z once per copy,
rather than reporting overlapping sections as separate objects or treating each
short local section as a short completed part. Different copies retain horizontal
head and vertical rod/lid clearance checks. Section print ordering remains
parent-first within each copy.

Sequential staging tracks the calibrated height of actual deposited moves,
including positive-extrusion wipes, instead of a section's rebased local maximum.
The horizontal transfer uses at least 1 mm above this height (or the profile's
larger Z hop), and rejects a staging height beyond the printer's vertical limit.
The final collision check remains mandatory for ascent/descent and other motion;
a rejected file is removed before it becomes exportable.

Generated section motion uses a conservative scalar feed cap: the smaller positive Z travel
and machine Z speed limits. Zero Z travel speed uses the machine limit; if both
are unset, generated section motion is rejected. The cap applies to mapped XY, XYZ and Z
travel, extrusion feed commands and the cached speed used for role-based wipes,
because each XYZ motion can contain physical Z motion. Global spiral lifts
apply the same speed cap with and without arc fitting. In-place lifts, including
spirals, are suppressed when the logical pose differs from the emitted pose. Section-containing exports
keep these physical limits active through generated global transitions, including
when no geometric frame is bound. Export cleanup clears the mode; ordinary prints
retain their existing feed behavior. This deliberately conservative cap does not replace
collision checks. Print and travel acceleration are bounded by the configured
machine Z acceleration, rounded downward to the writer's integer command format.
Zero/inherited acceleration selects that bound inside a section. Sequential
section entry establishes bounded modal acceleration before any section move;
missing or sub-unit limits are rejected. Direction-aware feed optimization is
not used: all section XYZ motion keeps the conservative scalar cap. Raw section
macro feed commands receive the same cap.

G-code processing separately retains section IDs and normalized build directions.
Reloading moves also reloads their direction palette. Annotated section travel
retains its physical Z even during a custom start stage,
so preview preparation-stage flattening cannot hide a clearance move.
Export and edited-file collision processing also explicitly preserve preparation
Z for unannotated motion. Processed-result assignment preserves Z calibration;
fresh and reset results use zero calibration. Removing section comments cannot restore display-only
Z flattening in the data used for clearance checks.
With non-planar mode enabled, export checks annotated output before renaming its temporary file, and checks
edited G-code before file export or raw-G-code host upload. These checks describe
a configured hotend model; generic defaults do not establish physical printer
clearance.

With non-planar validation enabled, unannotated XYZ motion also receives the
chronological upright-nozzle check. Upright checks use the same spatial index
against sampled material within and across section boundaries; moving a section
marker does not switch to a rounded XY grid. Removing section comments during
post-processing cannot turn off clearance validation. Ordinary printing with the
experimental option disabled retains its existing export path.
Wipe motion is included in the nozzle sweep, and positive-extrusion wipes add
material for subsequent moves to avoid.

Annotated section extrusion and travel must stay within physical Z travel limits
and the plate-local XY printable area. Concave-bed checks also reject a segment
that crosses outside the bed between two valid endpoints.
The check restores the calibration removed from preview positions before testing
the bed and printable-height bounds. Untagged firmware-managed start/end motion
is outside this section-specific limit check.

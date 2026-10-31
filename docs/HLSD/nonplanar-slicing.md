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
While a section transform is active, custom-G-code position variables use the
emitted machine coordinates, including plate subtraction. Raw custom G-code does
not pass through the writer again. Returning a plate-global position here would
shift a macro move by the plate offset a second time.
The initial toolpath preview uses the same per-copy inverse slicing map to place
section perimeter, infill and support vertices back in world geometry. Machine
calibration is excluded from this model-space view.
Section preview keeps local layer order in separate slider ranges rather than
sorting vertices by world Z. A tilted layer has varying world Z, and an upper
section's rebased heights cannot be compared with its physical coordinates.

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
floors provide load-bearing heights. The physical world-Z build plate is carried
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
load is rebuilt on every family generation. Tree-support inheritance and support
below floating bases are rejected by the family generator. The section export
guard remains active.

Derived sections are rejected by print validation, processing, and export while
safe section transitions and support propagation remain incomplete. A sequential
export scope binds the per-copy frame, writes unique section/build-axis metadata,
resets cooling in physical coordinates and closes the frame before global motion.
Exception cleanup restores the ordinary writer frame. Entering a section marks
the logical position as uncommanded until the first absolute XYZ move. The writer
retains the last emitted section endpoint independently of planned layer Z.
Custom-position variables, frame exit and in-place lift checks use that endpoint,
so planning a layer cannot teleport macro coordinates or trigger a lift through a
pose the printer has not reached. The `max_layer_z` macro override uses the highest generated positive-extrusion
machine endpoint, including Z calibration, seeded from processed startup material.
Descending deposition includes its starting height; planned positions, travel and
retracting wipes do not raise this maximum. Caller override data stays unchanged.
Other layer-height placeholders and raw macro motion still require separate
section integration. This scope remains behind
the export guard; frame binding alone does not establish complete motion safety
or correct section-layer settings. This includes explicitly directed
parts with zero rotation: their rebased layer schedules still require section
emission. The guard prevents a project from silently producing ordinary planar
G-code for these descriptors.

Sequential staging tracks the calibrated height of actual deposited moves,
including positive-extrusion wipes, instead of a section's rebased local maximum.
The horizontal transfer uses at least 1 mm above this height (or the profile's
larger Z hop), and rejects a staging height beyond the printer's vertical limit.
The final collision check remains necessary for ascent/descent and other motion;
these staging components do not remove the section export guard.

Generated section motion uses a conservative scalar feed cap: the smaller positive Z travel
and machine Z speed limits. Zero Z travel speed uses the machine limit; if both
are unset, generated section motion is rejected. The cap applies to mapped XY, XYZ and Z
travel, extrusion feed commands and the cached speed used for role-based wipes,
because each XYZ motion can contain physical Z motion. Section-containing exports
keep these physical limits active through generated global transitions, including
when no geometric frame is bound. Export cleanup clears the mode; ordinary prints
retain their existing feed behavior. This deliberately conservative cap does not replace
collision checks. Print and travel acceleration are bounded by the configured
machine Z acceleration, rounded downward to the writer's integer command format.
Zero/inherited acceleration selects that bound inside a section. Sequential
section entry establishes bounded modal acceleration before any section move;
missing or sub-unit limits are rejected. Direction-aware feed optimization,
transition route planning and raw custom macros still require integration before
section export can be enabled.

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

Annotated section extrusion and travel must stay within physical Z travel limits.
The check restores the calibration removed from preview positions before testing
the bed and printable-height bounds. Untagged firmware-managed start/end motion
is outside this section-specific limit check.

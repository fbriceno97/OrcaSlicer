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
clipping inactive. Parent support-demand propagation and the separate classic
tree-support path still require integration before section export can be enabled.

Derived sections are rejected by print validation, processing, and export while
machine-space section emission is unavailable. This includes explicitly directed
parts with zero rotation: their rebased layer schedules still require section
emission. The guard prevents a project from silently producing ordinary planar
G-code for these descriptors.

G-code processing separately retains section IDs and normalized build directions.
Reloading moves also reloads their direction palette. Annotated section travel
retains its physical Z even during a custom start stage,
so preview preparation-stage flattening cannot hide a clearance move.
With non-planar mode enabled, export checks annotated output before renaming its temporary file, and checks
edited G-code before file export or raw-G-code host upload. These checks describe
a configured hotend model; generic defaults do not establish physical printer
clearance.

With non-planar validation enabled, unannotated XYZ motion also receives the
chronological upright-nozzle check. Removing section comments during
post-processing cannot turn off clearance validation. Ordinary printing with the
experimental option disabled retains its existing export path.

Annotated section extrusion and travel must stay within physical Z travel limits.
The check restores the calibration removed from preview positions before testing
the bed and printable-height bounds. Untagged firmware-managed start/end motion
is outside this section-specific limit check.

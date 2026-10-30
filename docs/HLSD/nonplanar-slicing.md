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

A model-space bound `n*x <= d` becomes `n_s*s <= d + n_s*t`, where
`n_s = inverse(A).transpose()*n`. At each slice height this is an XY half-plane.
Horizontal bounds either keep or discard the entire slice. Bed bounds include
half the bead height projected onto the bed normal. Clipping happens on region
polygons, preserving their material and modifier assignments. Parts belonging to
another section retain empty slice slots so modifier parent indices remain valid.
Section processing avoids region bounding-box shortcuts whose coordinates belong
to the ordinary slicing frame.

Derived sections are rejected by print validation, processing, and export while
machine-space section emission is unavailable. This includes explicitly directed
parts with zero rotation: their rebased layer schedules still require section
emission. The guard prevents a project from silently producing ordinary planar
G-code for these descriptors.

G-code processing separately retains section IDs and normalized build directions.
Reloading moves also reloads their direction palette. With non-planar mode enabled,
export checks annotated output before renaming its temporary file, and checks
edited G-code before file export or raw-G-code host upload. These checks describe
a configured hotend model; generic defaults do not establish physical printer
clearance.

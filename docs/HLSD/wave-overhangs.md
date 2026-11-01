# Wave overhangs

Wave overhangs replace the extra overhang perimeters inside exposed infill with
bounded Anderson wave traces. Generated coverage is removed from ordinary infill
to avoid printing that area twice. The normal wall envelope remains intact.
The feature is off by default, and its spacing, speed, fan and floor settings
have no effect on ordinary toolpaths while it is disabled.

Perimeter generation retains actual wave coverage on each `LayerRegion` in its
section's slicing frame. A shared perimeter pass clips that coverage back onto
each compatible source region. Regions with different wave enable, spacing,
speed or fan settings use separate perimeter passes, so one part cannot lend
its wave settings to a neighboring part.

`wave_overhang_floor_layers` defaults to zero. A positive value projects a source
region's wave footprint into the requested following layer rows, within the
same section. Infill preparation intersects these masks with existing sparse
surfaces and converts only those intersections to solid infill. External shells,
bridges, holes and the perimeter envelope retain their normal processing.
Reinforcement can cross material-region boundaries in the same section, but does
not project across a cut-plane boundary into another section. Coverage persists
independently of the floor setting so changing the layer count invalidates infill
preparation without rebuilding perimeters. Turning waves off regenerates
perimeters and clears their retained footprints.

Wave G-code keeps the existing extrusion role for preview and cooling while
marking trace boundaries. Wave speed is an upper bound; filament, overhang,
cooling and nonplanar physical motion limits still apply. A configured wave fan
override respects disabled cooling and first-layer restrictions, then restores
ordinary cooling. Nonplanar exports retain chronological modeled hotend collision
and printable-volume checks. Software fixtures do not establish physical nozzle
clearance or print quality.

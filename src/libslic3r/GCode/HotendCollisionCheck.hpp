#ifndef slic3r_HotendCollisionCheck_hpp_
#define slic3r_HotendCollisionCheck_hpp_

#include "GCodeProcessor.hpp"
#include "../PrintConfig.hpp"
#include "../HotendEnvelope.hpp"
#include "TrunnionAC.hpp"

#include <optional>
#include <functional>

namespace Slic3r {

// UNLAYERED_MOD -- post-export hotend collision check for multiplanar prints.
//
// The hotend is modelled as a four-sided pyramid (HotendEnvelope, PrintConfig.hpp) with its apex at
// the nozzle tip and its axis along the tool axis, each face at its own angle because a real
// toolhead is not round. Material deposited in an EARLIER "episode" -- a maximal run of moves
// sharing one tool axis, i.e. one chunk or cut-plane segment under By object / By group sequencing
// -- that ends up inside that pyramid while a later episode prints is a collision.
//
// Frame: RepRapFirmware compensates the rotary axes, so the processed move positions are the
// nozzle tip in the BED frame and deposited material stays where it was laid; the tool axis per
// move is GCodeProcessorResult::tool_axes[move.tool_axis_id], which the exporter tags at every
// orientation change. The pyramid's faces turn with the head: tool_head_frame() gives the head's
// real orientation for that axis on a 5-axis machine. On a virtual 5-axis (3-axis) printer the
// nozzle never tilts, so `upright` holds the pyramid vertical with its faces along machine X/Y
// whatever axis the chunk was tagged with. Section markers partition episodes;
// unannotated motion is still checked when the caller enables validation.
// Upright nozzles also check earlier deposition within the same episode in motion order.
struct HotendCollisionParams
{
    HotendEnvelope envelope;          // height <= 0 disables the check
    bool           upright = true;    // ordinary three-axis nozzle stays vertical
    // A/C table-table machine: the head never tilts, the table does, so the head's faces in the bed
    // frame come from the (A, C) the exporter picks for each episode (TrunnionAC::solve_sequence),
    // not from tool_head_frame()'s B/C model.
    std::optional<TrunnionAC::Machine> ac;
    double         margin  = 0.5;     // lateral tolerance on every face (line width / 2 + nozzle flat), mm
    double         eps     = 0.3;     // ignore material within this height above the tip: the layer being printed
    std::function<void()> throw_on_cancel;
};

// Returns a ConflictResult describing a detected hit (with `message` set), or nullopt when the
// envelope stays clear of all earlier material. _objName1/_objName2 name the printing chunk and the
// hit material's chunk, _height is the machine Z of the offending tip position.
std::optional<ConflictResult> check_hotend_collisions(const GCodeProcessorResult &result, const HotendCollisionParams &params);

} // namespace Slic3r

#endif // slic3r_HotendCollisionCheck_hpp_

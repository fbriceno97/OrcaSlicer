#!/usr/bin/env python3
"""Compare actual bucket traversal bodies on unsplit synthetic adjacency graphs.

Usage: python3 scripts/check_paint_bucket_parity.py --reference ../bambu
Requires a C++17 compiler. This isolated test does not validate mesh adjacency
construction, subdivision, GUI rendering, undo integration, or project I/O;
those require the native TriangleSelector suite and GUI tests.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def bucket_body(root):
    source = (root / 'src/libslic3r/TriangleSelector.cpp').read_text()
    start = source.index('void TriangleSelector::bucket_fill_select_triangles(')
    end = source.index('// Selects either the whole triangle', start)
    return source[start:end]


ADAPTER = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <queue>
#include <utility>
#include <vector>
constexpr double EPSILON = 1e-4;
namespace Geometry { double deg2rad(double v) { return v * 3.14159265358979323846 / 180.; } }
struct Vec3f {
    float x=0, y=0, z=1;
    float dot(const Vec3f &n) const { return x*n.x+y*n.y+z*n.z; }
};
struct Vec3i {
    std::array<int,3> a;
    Vec3i(int x=-1,int y=-1,int z=-1):a{x,y,z} {}
    int operator()(int i) const { return a[i]; }
    auto begin() const { return a.begin(); } auto end() const { return a.end(); }
};
using Vec3i32 = Vec3i;
using EnforcerBlockerType = int;
struct ClippingPlane { bool active=false; bool is_active() const { return active; } };
struct Triangle {
    int source_triangle=0, state=0;
    std::array<int,3> verts_idxs{0,1,2};
    bool selected=false;
    bool is_split() const { return false; }
    bool is_selected_by_seed_fill() const { return selected; }
    void select_by_seed_fill() { selected=true; }
    int get_state() const { return state; }
};
class TriangleSelector {
public:
    std::vector<Triangle> m_triangles;
    std::vector<Vec3f> m_face_normals;
    std::vector<Vec3i> adjacency;
    std::vector<bool> clipped;
    int select_unsplit_triangle(const Vec3f&,int f) const { return f; }
    bool verify_triangle_neighbors(const Triangle&, const Vec3i&) const { return true; }
    void seed_fill_unselect_all_triangles() { for(auto &t:m_triangles)t.selected=false; }
    bool is_facet_clipped(int f,const ClippingPlane& c) const { return c.active && clipped[f]; }
    // Original unsplit facets: direct and propagated neighbors coincide.
    auto precompute_all_neighbors() const { return std::make_pair(adjacency,adjacency); }
    void append_touching_subtriangles(int f,int,int,std::vector<int>&out) const {
        if(f>=0) out.push_back(f);
    }
    void bucket_fill_select_triangles(const Vec3f&,int,const ClippingPlane&,float,bool,bool);
};
'''

CASES = r'''
int main() {
    int cases=0;
    for(int count : {3, 100, 10000})
    for(float step : {0.f, 5.f, 20.f, 30.f, 40.f, 90.f})
    for(float threshold : {-1.f, 0.f, 30.f})
    for(bool colors : {false,true})
    for(bool clipping : {false,true}) {
        TriangleSelector s;
        for(int i=0;i<count;++i) {
            Triangle t; t.source_triangle=i; t.state=colors && i>=count/2 ? 1 : 0;
            s.m_triangles.push_back(t);
            const double angle=Geometry::deg2rad(i*step);
            s.m_face_normals.push_back({float(std::sin(angle)),0,float(std::cos(angle))});
            s.adjacency.push_back(Vec3i(i==0?-1:i-1,i==count-1?-1:i+1,-1));
            s.clipped.push_back(clipping && i>=count/2);
        }
        s.bucket_fill_select_triangles({},0,{clipping},threshold,true,true);
        std::vector<int> selected;
        for(int i=0;i<count;++i) if(s.m_triangles[i].selected) selected.push_back(i);
        const int expected=(threshold>=0 && step>threshold) ? 1 : ((colors||clipping)?count/2:count);
        assert(int(selected.size())==expected);
        // Include every selected ID so equal counts cannot hide a different set.
        std::cout<<cases++<<":";
        for(int id:selected)std::cout<<id<<",";
        std::cout<<"\n";
    }
    std::cerr<<cases<<" unsplit traversal cases passed\n";
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--compiler', default='c++')
    args = parser.parse_args()
    target = Path(__file__).resolve().parents[1]
    outputs = []
    with tempfile.TemporaryDirectory() as directory:
        for label, root in [('orca', target), ('bambu', args.reference)]:
            cpp = Path(directory) / (label + '.cpp')
            binary = Path(directory) / label
            cpp.write_text(ADAPTER + bucket_body(root) + CASES)
            subprocess.run([args.compiler, '-std=c++17', '-O2', str(cpp), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
            print(label + ': ' + result.stderr.strip())
            outputs.append(result.stdout)
    if outputs[0] != outputs[1]:
        raise SystemExit('FAIL: selected triangle sets differ')
    print('PASS: identical selected triangle IDs in all 216 cases; unsplit traversal only')


if __name__ == '__main__':
    main()

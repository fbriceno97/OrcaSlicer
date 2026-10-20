#!/usr/bin/env python3
"""Compare painting/subdivision source against Bambu Studio v02.08.04.57.

This isolated check uses real Eigen and unmodified selector painting methods.
A small dependency facade supplies mesh storage, neighbor/normal construction,
logging and a sequential TBB replacement. It does not replace the native suite:
GUI, project I/O, networking, production adjacency construction and concurrency
are outside this check. Generated source stays in a temporary directory.

Usage: python3 scripts/check_paint_selector_parity.py --reference ../bambu \
  --eigen /path/to/eigen3
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile

ADAPTER = r'''
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <utility>
#include <vector>
namespace cereal { class access; }
namespace boost { namespace container { template<class T,int N> using small_vector=std::vector<T>; } }
namespace tbb {
template<class T>struct blocked_range {T b,e;blocked_range(T x,T y):b(x),e(y){} T begin()const{return b;}T end()const{return e;}};
template<class R,class F>void parallel_for(R r,F f){f(r);}
}
#define BOOST_LOG_TRIVIAL(x) std::cerr
namespace Slic3r {
using Vec3f=Eigen::Vector3f;using Vec3d=Eigen::Vector3d;using Vec3i=Eigen::Vector3i;using Vec3i32=Vec3i;
using Vec2i=Eigen::Vector2i;using Vec2i32=Vec2i;using Matrix3f=Eigen::Matrix3f;
using Transform3d=Eigen::Transform<double,3,Eigen::Affine>;using Transform3f=Eigen::Transform<float,3,Eigen::Affine>;
using stl_vertex=Vec3f;using stl_triangle_vertex_indices=Vec3i;
template<class T>auto begin(const Eigen::MatrixBase<T>&v){return v.derived().data();}
template<class T>auto end(const Eigen::MatrixBase<T>&v){return v.derived().data()+v.size();}
size_t next_highest_power_of_2(size_t v){size_t n=1;while(n<v)n*=2;return n;}
constexpr double EPSILON=1e-4;constexpr double PI=3.14159265358979323846;
template<class T>auto sqr(T x){return x*x;}
int next_idx_modulo(int i,int n){return (i+1)%n;}
int prev_idx_modulo(int i,int n){return (i+n-1)%n;}
template<class T>bool is_approx(T a,T b){return std::abs(a-b)<1e-6;}
struct indexed_triangle_set {std::vector<Vec3f> vertices;std::vector<Vec3i> indices;};
struct TriangleMesh {indexed_triangle_set its;TriangleMesh(){}TriangleMesh(const indexed_triangle_set&s):its(s){} };
std::vector<Vec3i> its_face_neighbors(const indexed_triangle_set&its){
 std::vector<Vec3i> ns(its.indices.size(),Vec3i(-1,-1,-1));
 std::map<std::pair<int,int>,std::pair<int,int>> edges;
 for(int f=0;f<int(its.indices.size());++f)for(int s=0;s<3;++s){int a=its.indices[f][s],b=its.indices[f][(s+1)%3];auto it=edges.find({b,a});if(it!=edges.end()){ns[f][s]=it->second.first;ns[it->second.first][it->second.second]=f;}else edges[{a,b}]={f,s};}return ns;
}
std::vector<Vec3f> its_face_normals(const indexed_triangle_set&its){std::vector<Vec3f> n;for(auto f:its.indices)n.push_back((its.vertices[f[1]]-its.vertices[f[0]]).cross(its.vertices[f[2]]-its.vertices[f[0]]).normalized());return n;}
namespace Geometry {
double deg2rad(double d){return d*PI/180.;}
struct Transformation {Transform3d t;Transformation(const Transform3d&x):t(x){}Vec3d get_scaling_factor()const{return Vec3d(t.linear().col(0).norm(),t.linear().col(1).norm(),t.linear().col(2).norm());}};
}
}
'''

PROGRAM = r'''using namespace Slic3r;

indexed_triangle_set its_make_sphere(double radius, double fa)
{
    int   sectorCount = int(ceil(2. * M_PI / fa));

    int   stackCount  = int(ceil(M_PI / fa));

    float sectorStep  = float(2. * M_PI / sectorCount);

    float stackStep   = float(M_PI / stackCount);


    indexed_triangle_set mesh;

    auto& vertices = mesh.vertices;

    vertices.reserve((stackCount - 1) * sectorCount + 2);

    for (int i = 0;
 i <= stackCount;
 ++ i) {
        // from pi/2 to -pi/2
        double stackAngle = 0.5 * M_PI - stackStep * i;

        double xy = radius * cos(stackAngle);

        double z  = radius * sin(stackAngle);

        if (i == 0 || i == stackCount)
            vertices.emplace_back(Vec3f(float(xy), 0.f, float(z)));

        else
            for (int j = 0;
 j < sectorCount;
 ++ j) {
                // from 0 to 2pi
                double sectorAngle = sectorStep * j;

                vertices.emplace_back(Vec3d(xy * std::cos(sectorAngle), xy * std::sin(sectorAngle), z).cast<float>());

            }
    }

    auto& facets = mesh.indices;

    facets.reserve(2 * (stackCount - 1) * sectorCount);

    for (int i = 0;
 i < stackCount;
 ++ i) {
        // Beginning of current stack.
        int k1 = (i == 0) ? 0 : (1 + (i - 1) * sectorCount);

        int k1_first = k1;

        // Beginning of next stack.
        int k2 = (i == 0) ? 1 : (k1 + sectorCount);

        int k2_first = k2;

        for (int j = 0;
 j < sectorCount;
 ++ j) {
            // 2 triangles per sector excluding first and last stacks
            int k1_next = k1;

            int k2_next = k2;

            if (i != 0) {
                k1_next = (j + 1 == sectorCount) ? k1_first : (k1 + 1);

                facets.emplace_back(k1, k2, k1_next);

            }
            if (i + 1 != stackCount) {
                k2_next = (j + 1 == sectorCount) ? k2_first : (k2 + 1);

                facets.emplace_back(k1_next, k2, k2_next);

            }
            k1 = k1_next;

            k2 = k2_next;

        }
    }

    return mesh;

}


void dump(const TriangleSelector &s){
 std::vector<std::string> faces;

 for(int state=0;
state<3;
++state){auto mesh=s.get_facets(EnforcerBlockerType(state));
for(auto f:mesh.indices){std::vector<std::string> vertices;
for(int i=0;
i<3;
++i){auto v=mesh.vertices[f[i]];
std::ostringstream ss;
ss<<std::hexfloat<<v.x()<<","<<v.y()<<","<<v.z();
vertices.push_back(ss.str());
}std::sort(vertices.begin(),vertices.end());
faces.push_back(std::to_string(state)+":"+vertices[0]+"|"+vertices[1]+"|"+vertices[2]);
}}
 std::sort(faces.begin(),faces.end());
for(auto &f:faces)std::cout<<f<<"\n";

}
using namespace Slic3r;

static TriangleMesh fold(float degrees) {
 float a=degrees*float(PI)/180.f;
indexed_triangle_set its;

 its.vertices={Vec3f(0,0,0),Vec3f(1,0,0),Vec3f(0,1,0),Vec3f(1,1,0),Vec3f(1+std::cos(a),0,std::sin(a))};

 its.indices={Vec3i(0,1,2),Vec3i(1,3,2),Vec3i(1,4,3)};
return TriangleMesh(its);

}
static TriangleMesh cube(){indexed_triangle_set its;
its.vertices={Vec3f(0,0,0),Vec3f(.1,0,0),Vec3f(.1,.1,0),Vec3f(0,.1,0),Vec3f(0,0,.1),Vec3f(.1,0,.1),Vec3f(.1,.1,.1),Vec3f(0,.1,.1)};
its.indices={Vec3i(0,2,1),Vec3i(0,3,2),Vec3i(4,5,6),Vec3i(4,6,7),Vec3i(0,1,5),Vec3i(0,5,4),Vec3i(1,2,6),Vec3i(1,6,5),Vec3i(2,3,7),Vec3i(2,7,6),Vec3i(3,0,4),Vec3i(3,4,7)};
return TriangleMesh(its);
}
int main(){
 for(float bend:{0.f,20.f,40.f,90.f})for(float angle:{0.f,30.f}){auto mesh=fold(bend);
TriangleSelector s(mesh);
s.bucket_fill_select_triangles(Vec3f(.2,.2,0),0,{},angle,true,true);
s.seed_fill_apply_on_triangles(EnforcerBlockerType::ENFORCER);
assert(s.num_facets(EnforcerBlockerType::ENFORCER)==(bend<=angle?3:2));
}
 for(auto shape:{TriangleSelector::CursorType::CIRCLE,TriangleSelector::CursorType::SPHERE}){auto mesh=fold(0);
TriangleSelector s(mesh,.2 COLOR_BRUSH_CAP);
auto c=TriangleSelector::SinglePointCursor::cursor_factory(Vec3f(.2,.2,0),Vec3f(.2,.2,10),.12,shape,Transform3d::Identity(),{});
s.select_patch(0,std::move(c),EnforcerBlockerType::ENFORCER,Transform3d::Identity(),true);
assert(s.has_facets(EnforcerBlockerType::ENFORCER));
int n=s.num_facets(EnforcerBlockerType::ENFORCER);
auto before=s.serialize();
s.bucket_fill_select_triangles(Vec3f(.2,.2,0),0,{},-1,true,true);
s.seed_fill_apply_on_triangles(EnforcerBlockerType::BLOCKER);
assert(s.num_facets(EnforcerBlockerType::BLOCKER)==n);
assert(!s.has_facets(EnforcerBlockerType::ENFORCER));
s.deserialize(before);
assert(s.num_facets(EnforcerBlockerType::ENFORCER)==n);
dump(s);
std::cout<<"split brush "<<int(shape)<<" passed "<<n<<" leaves\n";
}
 {auto mesh=cube();
auto t=Transform3d::Identity();
t.scale(10.);
TriangleSelector s(mesh,.2 COLOR_BRUSH_CAP);
auto c=TriangleSelector::SinglePointCursor::cursor_factory(.4,Vec3f(0,0,1),.2,t,{});
s.select_patch(0,std::move(c),EnforcerBlockerType::ENFORCER,t,true);
assert(s.has_facets(EnforcerBlockerType::ENFORCER));
auto p=s.get_facets(EnforcerBlockerType::ENFORCER);
for(auto v:p.vertices){assert(v.z()*10>.38);
assert(v.z()*10<.62);
}dump(s);
std::cout<<"scaled height band passed "<<p.indices.size()<<" leaves\n";
}

 {TriangleMesh mesh(its_make_sphere(5.,2*PI/64));
TriangleSelector s(mesh);
auto f=mesh.its.indices[0];
Vec3f hit=(mesh.its.vertices[f[0]]+mesh.its.vertices[f[1]]+mesh.its.vertices[f[2]])/3.f;
s.bucket_fill_select_triangles(hit,0,{},30.f,true,true);
s.seed_fill_apply_on_triangles(EnforcerBlockerType::ENFORCER);
assert(s.num_facets(EnforcerBlockerType::ENFORCER)==int(mesh.its.indices.size()));
dump(s);
std::cout<<"dense sphere passed "<<s.num_facets(EnforcerBlockerType::ENFORCER)<<" faces\n";
}
 for(auto shape:{TriangleSelector::CursorType::CIRCLE,TriangleSelector::CursorType::SPHERE}) {auto mesh=fold(0);
for(auto &v:mesh.its.vertices)v*=20.f;
TriangleSelector s(mesh,.2 COLOR_BRUSH_CAP);
auto cursor=TriangleSelector::SinglePointCursor::cursor_factory(Vec3f(4,4,0),Vec3f(4,4,10),2.f,shape,Transform3d::Identity(),{});
s.select_patch(0,std::move(cursor),EnforcerBlockerType::ENFORCER,Transform3d::Identity(),true);
assert(s.has_facets(EnforcerBlockerType::ENFORCER));
dump(s);
std::cout<<"large brush passed "<<s.num_facets(EnforcerBlockerType::ENFORCER)<<" faces\n";
}
 std::cout<<"all extracted selector integration checks passed\n";

}
'''

def without_includes(text):
    return re.sub(r'^#include .*\n', '', text, flags=re.M)


def source_for(root, reference):
    header = without_includes((root / 'src/libslic3r/TriangleSelector.hpp').read_text())
    source = without_includes((root / 'src/libslic3r/TriangleSelector.cpp').read_text())
    adapter = ADAPTER
    if reference:
        # Bambu declares filament states in Model.hpp. Use that exact declaration.
        model = (root / 'src/libslic3r/Model.hpp').read_text()
        begin = model.index('enum class EnforcerBlockerType')
        end = model.index('};', begin) + 2
        adapter += '\nnamespace Slic3r {\n' + model[begin:end] + '\n}\n'
        # Unused mesh-refinement APIs depend on app utilities. The painting,
        # subdivision, neighbor, cursor and serialization methods remain intact.
        begin = source.index('TriangleSelector::FacetSubdivisionResult TriangleSelector::set_facets_with_subdivision(')
        end = source.index('void TriangleSelector::split_triangle(', begin)
        source = source[:begin] + source[end:]
        cap = '#define COLOR_BRUSH_CAP\n'
    else:
        # Mesh-to-mesh paint transfer is unrelated to interactive painting.
        source = source[:source.index('static bool segments_intersect_proj')] + '\n}\n'
        cap = '#define COLOR_BRUSH_CAP , 0.2f\n'
    return adapter + header + source + cap + PROGRAM


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--eigen', type=Path, required=True)
    parser.add_argument('--compiler', default='c++')
    args = parser.parse_args()
    if not (args.eigen / 'Eigen/Core').exists():
        raise SystemExit('--eigen must contain Eigen/Core')
    reference_sha = subprocess.check_output(
        ['git', '-C', str(args.reference), 'rev-parse', 'HEAD'], text=True).strip()
    if reference_sha != 'f977235e6d736c4c0b650520ac5a5b72cbfe9244':
        raise SystemExit('Reference must be the exact v02.08.04.57 commit')
    target = Path(__file__).resolve().parents[1]
    results = []
    with tempfile.TemporaryDirectory() as directory:
        for label, root, reference in [('orca', target, False), ('bambu', args.reference, True)]:
            cpp = Path(directory) / (label + '.cpp')
            binary = Path(directory) / label
            cpp.write_text(source_for(root, reference))
            subprocess.run([args.compiler, '-std=c++17', '-O0', '-I' + str(args.eigen), str(cpp), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, check=True)
            results.append(result.stdout)
            for line in result.stdout.decode().splitlines():
                if 'passed' in line:
                    print(label + ': ' + line)
    if results[0] != results[1]:
        raise SystemExit('FAIL: painted triangle geometry or states differ')
    print('PASS: identical painted triangle geometry and states')
    print('SHA256: ' + hashlib.sha256(results[0]).hexdigest())


if __name__ == '__main__':
    main()

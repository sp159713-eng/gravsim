import glob
import os
import subprocess
import sys

ROOT = os.getcwd()
UCRT = r"C:\msys64\ucrt64\bin"
ENV = dict(os.environ, PATH=UCRT + os.pathsep + os.environ.get("PATH", ""))
SOURCES = ["main.cpp", "particles.cpp", "camera.cpp", "selftest.cpp", "app.cpp", "app_vk.cpp", "app_buffers.cpp", "app_grid.cpp", "app_overlay.cpp", "app_sky.cpp", "app_planets.cpp", "app_input.cpp", "app_galaxy.cpp", "app_universe.cpp", "universe.cpp", "milkyway.cpp", "app_blackhole.cpp", "starsystem.cpp", "app_visit.cpp", "app_galvisit.cpp", "galaxygen.cpp", "app_bhsizes.cpp", "app_bhtour.cpp", "app_tour.cpp", "app_oort.cpp", "app_micro.cpp", "app_microlabels.cpp", "app_gw.cpp", "ephem.cpp", "realstars.cpp", "nebulae.cpp", "app_nebula.cpp", "app_lifecycle.cpp", "lifecycle_stages.cpp", "app_lifetable.cpp", "app_orbits.cpp", "app_bloom.cpp", "app_andromeda.cpp", "app_dark.cpp", "app_starsizes.cpp", "app_sandbox.cpp", "app_sandbox2.cpp", "app_sandbox3.cpp", "app_sandbox4.cpp", "app_sandbox5.cpp", "app_throw.cpp", "app_throw2.cpp", "app_throw3.cpp", "massctl.cpp", "app_earthmoon.cpp", "app_earthmoon_ui.cpp", "app_timedil.cpp", "app_timedil_ui.cpp", "app_lagrange.cpp", "app_lagrange_phys.cpp", "app_lagrange_ui.cpp", "app_lagrange_inset.cpp", "app_lightride.cpp", "app_lightride_draw.cpp", "app_futuresky.cpp", "app_futuresky_ui.cpp", "futuresky_keys.cpp", "nightsky_core.cpp", "nightsky_stars_a.cpp", "nightsky_stars_b.cpp", "nightsky_lines.cpp", "app_nightsky.cpp", "app_nightsky_ui.cpp", "impact_phys.cpp", "app_impact.cpp", "app_impact_ui.cpp", "eclipse_phys.cpp", "app_eclipse.cpp", "app_eclipse_ui.cpp", "capture.cpp", "capture_io.cpp", "app_help_page.cpp", "quiz_state.cpp", "quiz_ui.cpp", "quiz_jump.cpp", "quiz_bank_a.cpp", "quiz_bank_b.cpp", "quiz_bank_c.cpp", "bhdisc_vk.cpp", "bhdisc_sim.cpp", "bhdisc_ui.cpp"]
SHADERS = ["nbody.comp", "point.vert", "point.frag", "grid.vert", "grid.frag", "overlay.vert", "sky.vert", "sky.frag", "planet.vert", "planet.frag", "galaxy.vert", "galaxy.frag", "universe.vert", "universe.frag", "cmb.vert", "cmb.frag", "blackhole.vert", "blackhole.frag", "micro.vert", "micro.frag", "nebula.vert", "nebula.frag", "lifecycle.frag", "bloom_bright.frag", "bloom_blur.frag", "bloom_comp.frag", "dark.frag", "starsizes.frag", "earthmoon.frag", "futuresky.frag", "nightsky.frag", "impact.frag", "eclipse.frag", "bhdisc_step.comp", "bhdisc.vert", "bhdisc.frag"]


def run(cmd, timeout=180):
    result = subprocess.run(cmd, cwd=ROOT, env=ENV, capture_output=True, text=True, timeout=timeout)
    if result.returncode != 0:
        print("FAILED:", " ".join(cmd))
        print((result.stdout + result.stderr)[-3000:])
        sys.exit(1)
    return result.stdout


os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
OUT = "gravsim.exe"
args = []
for a in sys.argv[1:]:
    if a.startswith("--out="):
        OUT = a[6:]
    else:
        args.append(a)
only = [f.replace("\\", "/") for f in args]
if only:
    for rel in only:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            print("FAILED: missing", rel)
            sys.exit(1)
        if rel.endswith((".comp", ".vert", ".frag")):
            run([os.path.join(UCRT, "glslc.exe"), path, "-o", path + ".spv"])
        elif rel.endswith(".cpp"):
            obj = os.path.join(ROOT, "build", os.path.basename(rel) + ".o")
            run([os.path.join(UCRT, "g++.exe"), "-std=c++17", "-O2", "-Wall", "-Isrc", "-c", path, "-o", obj])
        elif rel.endswith((".h", ".hpp")):
            run([os.path.join(UCRT, "g++.exe"), "-std=c++17", "-Wall", "-Isrc", "-fsyntax-only", "-x", "c++", path])
    print("CHECK PASS (files:", " ".join(only), ")")
    sys.exit(0)
for shader in glob.glob(os.path.join(ROOT, "shaders", "*.*")):
    if shader.endswith((".comp", ".vert", ".frag")):
        run([os.path.join(UCRT, "glslc.exe"), shader, "-o", shader + ".spv"])

objects = []
for source in glob.glob(os.path.join(ROOT, "src", "*.cpp")):
    obj = os.path.join(ROOT, "build", os.path.basename(source) + ".o")
    run([os.path.join(UCRT, "g++.exe"), "-std=c++17", "-O2", "-Wall", "-Isrc", "-c", source, "-o", obj])
    objects.append(obj)

have = {os.path.basename(p) for p in glob.glob(os.path.join(ROOT, "src", "*.cpp"))}
have_shaders = {os.path.basename(p) for p in glob.glob(os.path.join(ROOT, "shaders", "*"))}
if set(SOURCES) <= have and set(SHADERS) <= have_shaders:
    exe = os.path.join(ROOT, OUT)
    run([os.path.join(UCRT, "g++.exe")] + objects + ["-o", exe, "-static-libgcc", "-static-libstdc++", "-lglfw3", "-lvulkan-1", "-lgdi32"])
    print(run([exe, "--selftest"], timeout=120)[-1500:])
    print("CHECK PASS (full build + selftest)")
else:
    print("CHECK PASS (partial: compiled", sorted(have), sorted(have_shaders), ")")

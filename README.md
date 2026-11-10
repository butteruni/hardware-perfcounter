# HardwarePerfCounter

Adreno GPU counter library based on [google/hardware-perfcounter](https://github.com/google/hardware-perfcounter),
with A7xx/A8xx definitions from Mesa. Android applications can use its C API to
read system-wide counters through KGSL.

## Build

```sh
git clone --branch adreno-a7xx-a8xx https://github.com/butteruni/hardware-perfcounter.git
cd hardware-perfcounter
cmake -G Ninja -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DHPC_ENABLE_GPU_MALI=OFF -DHPC_BUILD_EXAMPLES=OFF
cmake --build build-android
```

Use a separate build directory with `-DANDROID_ABI=armeabi-v7a` for 32-bit Android.
Normal builds use checked-in definitions and require no source submodules.

## Use in an application

```cmake
set(HPC_ENABLE_GPU_MALI OFF CACHE BOOL "" FORCE)
set(HPC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/hardware-perfcounter)
target_link_libraries(your_collector PRIVATE hpc::gpu::adreno::kgsl)
```

Include `hpc/gpu/adreno/kgsl.h` for explicit device open, slot query,
GET/READ/PUT and close operations. For managed reservations and delta sampling,
link `hpc::gpu::adreno::a7xx` or `hpc::gpu::adreno::a8xx`, create a context with
`hpc_gpu_adreno_a7xx_create_context` or `hpc_gpu_adreno_a8xx_create_context`, then
use the start/query/stop/destroy functions in `raw.h`.

Some vendor kernels require this before sampling:

```sh
adb shell "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"
adb shell cat /sys/class/kgsl/kgsl-3d0/perfcounter
```

Check every return value, release successful reservations, and discard windows
reported as discontinuities. Device permissions and available counter slots
remain kernel-dependent. Counters are system-wide, not per-process.

## Sync and update

To sync the original project:

```sh
git remote add upstream https://github.com/google/hardware-perfcounter.git
git fetch upstream
git merge upstream/main
```

Add the remote only once. The Google repository is currently archived.

To update Mesa definitions, use a local Mesa checkout and an explicit revision:

```sh
git -C /path/to/mesa fetch origin
python scripts/update_mesa.py --source /path/to/mesa --revision <commit>
python scripts/generate_modern_adreno.py --check
python tests/run_host_tests.py
cmake --build build-android
```

The import updates source hashes and regenerates event definitions and chip-ID
mappings. Review the diff and run device checks before using a new revision.
KGSL group IDs are maintained separately in `third_party/adreno/kgsl_groups.json`.

In a consuming project, update and commit the submodule pointer:

```sh
git -C third_party/hardware-perfcounter fetch origin
git -C third_party/hardware-perfcounter checkout <tested-commit>
git add third_party/hardware-perfcounter
```

See [source notices](third_party/adreno/README.md) and [LICENSE](LICENSE).

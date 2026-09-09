#!/usr/bin/env bash
#
# probe_device_caps.sh
#
# PURPOSE: Report the optional Vulkan capabilities the virtual geometry
#          pipeline depends on, and derive the architectural decisions that
#          they force.
#
# USAGE:   tools/probe_device_caps.sh [--gpu N] [--write [PATH]] [--no-color]
#
# This script reads 'vulkaninfo' and requires no build, so the answer can be
# obtained -- and committed -- before any pipeline code is written. The engine
# performs the equivalent query at startup and exposes the result through
# 'eng::rhi::DeviceCapabilities'; keep the two in sync.

set -uo pipefail

WRITE_PATH=""
DO_WRITE=0
USE_COLOR=1
GPU_INDEX=""

while [ $# -gt 0 ]; do
    case "$1" in
        --gpu)
            GPU_INDEX="${2-}"
            if ! printf '%s' "$GPU_INDEX" | grep -qE '^[0-9]+$'; then
                echo "--gpu expects a device index" >&2
                exit 2
            fi
            shift
            ;;
        --write)
            DO_WRITE=1
            case "${2-}" in
                ""|--*) WRITE_PATH="doc/device_profile.md" ;;
                *) WRITE_PATH="$2"; shift ;;
            esac
            ;;
        --no-color) USE_COLOR=0 ;;
        -h|--help)
            sed -n '3,15p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 2
            ;;
    esac
    shift
done

if [ ! -t 1 ]; then USE_COLOR=0; fi
if [ "$USE_COLOR" = "1" ]; then
    C_OK=$'\033[32m'; C_NO=$'\033[31m'; C_WARN=$'\033[33m'
    C_DIM=$'\033[2m';  C_B=$'\033[1m';  C_0=$'\033[0m'
else
    C_OK=""; C_NO=""; C_WARN=""; C_DIM=""; C_B=""; C_0=""
fi

# ---------------------------------------------------------------- collect ---

if ! command -v vulkaninfo >/dev/null 2>&1; then
    echo "${C_NO}vulkaninfo not found.${C_0}" >&2
    echo "  macOS:  brew install --cask vulkan-sdk" >&2
    echo "  Linux:  apt install vulkan-tools  (or the LunarG SDK)" >&2
    exit 127
fi

RAW="$(vulkaninfo 2>/dev/null)"
if [ -z "$RAW" ]; then
    echo "${C_NO}vulkaninfo produced no output -- no usable Vulkan driver.${C_0}" >&2
    exit 1
fi

# vulkaninfo emits one "GPUn:" block per physical device. Systems with a
# software rasterizer (llvmpipe / lavapipe) or a hybrid iGPU+dGPU pair report
# several, and the first one is frequently not the device to plan against.
GPU_IDS=$(printf '%s\n' "$RAW" | grep -oE '^GPU[0-9]+:' \
    | sed -E 's/^GPU([0-9]+):/\1/' | sort -un)
GPU_COUNT=$(printf '%s\n' "$GPU_IDS" | grep -c . )

# Return every line belonging to the specified GPU index.
gpu_block()
{
    printf '%s\n' "$RAW" | awk -v want="$1" '
        /^GPU[0-9]+:/ {
            idx = $0
            sub(/^GPU/, "", idx)
            sub(/:$/, "", idx)
            inblock = (idx == want)
            next
        }
        inblock { print }
    '
}

# Return the first "field = value" line of the specified block.
block_value()
{
    printf '%s\n' "$2" | grep -E "^[[:space:]]+$1[[:space:]]+=" | head -1 \
        | sed -E 's/.*=[[:space:]]*//'
}

# Choose the device to report on: an explicit --gpu, else the first discrete
# GPU, else the first device that is not a pure software implementation.
SELECTED=""
FALLBACK=""
GPU_SUMMARY=""

for id in $GPU_IDS; do
    block=$(gpu_block "$id")
    name=$(block_value deviceName "$block")
    type=$(block_value deviceType "$block")
    GPU_SUMMARY="${GPU_SUMMARY}${id}|${type}|${name}"$'\n'

    case "$type" in
        *DISCRETE_GPU*)   [ -z "$SELECTED" ] && SELECTED="$id" ;;
        *CPU*|*OTHER*)    : ;;
        *)                [ -z "$FALLBACK" ] && FALLBACK="$id" ;;
    esac
done

if [ -n "$GPU_INDEX" ]; then
    if ! printf '%s\n' "$GPU_IDS" | grep -qx "$GPU_INDEX"; then
        echo "${C_NO}No such device index: GPU${GPU_INDEX}${C_0}" >&2
        exit 2
    fi
    SELECTED="$GPU_INDEX"
fi

[ -z "$SELECTED" ] && SELECTED="$FALLBACK"
[ -z "$SELECTED" ] && SELECTED=$(printf '%s\n' "$GPU_IDS" | head -1)
[ -z "$SELECTED" ] && SELECTED=0

# Every query below reads this device only.
RAW=$(gpu_block "$SELECTED")
if [ -z "$RAW" ]; then
    echo "${C_NO}Could not isolate GPU${SELECTED} in the vulkaninfo output.${C_0}" >&2
    exit 1
fi

# Return the first reported value of a boolean feature, or "absent" when the
# feature's structure was never emitted (its extension is unsupported).
feature()
{
    local value
    value=$(printf '%s\n' "$RAW" \
        | grep -E "^[[:space:]]+$1[[:space:]]+=" \
        | head -1 \
        | sed -E 's/.*=[[:space:]]*//' \
        | tr -d '[:space:]')
    printf '%s' "${value:-absent}"
}

# Return the first reported value of a numeric property, or "?" when absent.
property()
{
    local value
    value=$(printf '%s\n' "$RAW" \
        | grep -E "^[[:space:]]+$1[[:space:]]+=" \
        | head -1 \
        | sed -E 's/.*=[[:space:]]*//' \
        | tr -d '[:space:]')
    printf '%s' "${value:-?}"
}

# Return "true" when the named device extension is present.
extension()
{
    if printf '%s\n' "$RAW" | grep -qE "^[[:space:]]+$1[[:space:]]*:"; then
        printf 'true'
    else
        printf 'false'
    fi
}

DEVICE_NAME=$(printf '%s\n' "$RAW" | grep -E '^[[:space:]]+deviceName[[:space:]]+=' \
    | head -1 | sed -E 's/.*=[[:space:]]*//')
DRIVER_NAME=$(printf '%s\n' "$RAW" | grep -E '^[[:space:]]+driverName[[:space:]]+=' \
    | head -1 | sed -E 's/.*=[[:space:]]*//')
DRIVER_INFO=$(printf '%s\n' "$RAW" | grep -E '^[[:space:]]+driverInfo[[:space:]]+=' \
    | head -1 | sed -E 's/.*=[[:space:]]*//')
API_VERSION=$(printf '%s\n' "$RAW" | grep -E '^[[:space:]]+apiVersion[[:space:]]+=' \
    | head -1 | sed -E 's/.*=[[:space:]]*//')

BDA=$(feature bufferDeviceAddress)
DESC_IDX=$(feature descriptorIndexing)
RT_ARRAY=$(feature runtimeDescriptorArray)
NU_IMG=$(feature shaderSampledImageArrayNonUniformIndexing)
PART_BOUND=$(feature descriptorBindingPartiallyBound)
MULTI_DRAW=$(feature multiDrawIndirect)
DRAW_COUNT=$(feature drawIndirectCount)
INT64=$(feature shaderInt64)
BUF_ATOM64=$(feature shaderBufferInt64Atomics)
IMG_ATOM64=$(feature shaderImageInt64Atomics)
DRAW_PARAMS=$(feature shaderDrawParameters)
MESH_FEAT=$(feature meshShader)
TASK_FEAT=$(feature taskShader)

EXT_MESH=$(extension VK_EXT_mesh_shader)
EXT_IMG_ATOM64=$(extension VK_EXT_shader_image_atomic_int64)
EXT_DRAW_COUNT=$(extension VK_KHR_draw_indirect_count)

SUBGROUP=$(property subgroupSize)
MIN_SUBGROUP=$(property minSubgroupSize)
MAX_SUBGROUP=$(property maxSubgroupSize)
MAX_CS_INVOC=$(property maxComputeWorkGroupInvocations)
MAX_BINDLESS=$(property maxPerStageDescriptorUpdateAfterBindSampledImages)

# ------------------------------------------------------------------ report --

# Print one capability row: name, state, and the consequence of losing it.
row()
{
    local name="$1" state="$2" consequence="$3" mark color
    case "$state" in
        true)   mark="yes"; color="$C_OK" ;;
        false)  mark="NO";  color="$C_NO" ;;
        absent) mark="--";  color="$C_NO"; state="not exposed" ;;
        *)      mark="?";   color="$C_WARN" ;;
    esac
    printf '  %-44s %s%-4s%s %s%s%s\n' \
        "$name" "$color" "$mark" "$C_0" "$C_DIM" "$consequence" "$C_0"
}

echo
echo "${C_B}Device${C_0}"
printf '  %-44s %s\n' "deviceName"  "${DEVICE_NAME:-unknown}"
printf '  %-44s %s\n' "driver"      "${DRIVER_NAME:-unknown} ${DRIVER_INFO:-}"
printf '  %-44s %s\n' "apiVersion"  "${API_VERSION:-unknown}"
printf '  %-44s %s\n' "reporting on" "GPU${SELECTED} of ${GPU_COUNT}"
if [ "$GPU_COUNT" -gt 1 ]; then
    printf '%s' "$GPU_SUMMARY" | while IFS='|' read -r id type name; do
        [ -z "$id" ] && continue
        if [ "$id" = "$SELECTED" ]; then mark="*"; else mark=" "; fi
        printf '  %s GPU%-41s %s %s%s%s\n' \
            "$mark" "$id" "$name" "$C_DIM" "$type" "$C_0"
    done
    echo "  ${C_DIM}select another with --gpu N${C_0}"
fi

echo
echo "${C_B}Bindless and GPU-driven submission${C_0}   ${C_DIM}(M0 / M1)${C_0}"
row "bufferDeviceAddress"                        "$BDA"         "raw GPU pointers into cluster data"
row "descriptorIndexing"                         "$DESC_IDX"    "shader-computed texture indices"
row "runtimeDescriptorArray"                     "$RT_ARRAY"    "unbounded descriptor arrays"
row "shaderSampledImageArrayNonUniformIndexing"  "$NU_IMG"      "per-pixel material lookup in resolve"
row "descriptorBindingPartiallyBound"            "$PART_BOUND"  "sparse bindless table"
row "multiDrawIndirect"                          "$MULTI_DRAW"  "many draws from one buffer"
row "drawIndirectCount"                          "$DRAW_COUNT"  "GPU decides the draw count"
row "VK_KHR_draw_indirect_count"                 "$EXT_DRAW_COUNT" "same, as an extension"
row "shaderDrawParameters"                       "$DRAW_PARAMS" "gl_DrawID for per-cluster lookup"

echo
echo "${C_B}64-bit integer support${C_0}                ${C_DIM}(M3 software rasterizer)${C_0}"
row "shaderInt64"                                "$INT64"       "uint64 arithmetic in shaders"
row "shaderBufferInt64Atomics"                   "$BUF_ATOM64"  "64-bit atomics on buffers"
row "VK_EXT_shader_image_atomic_int64"           "$EXT_IMG_ATOM64" "extension gating the next line"
row "shaderImageInt64Atomics"                    "$IMG_ATOM64"  "atomicMax(depth<<32 | triID)"

echo
echo "${C_B}Optional geometry pipeline${C_0}            ${C_DIM}(M1 stretch)${C_0}"
row "VK_EXT_mesh_shader"                         "$EXT_MESH"    "task/mesh stages"
row "meshShader"                                 "$MESH_FEAT"   "mesh stage feature bit"
row "taskShader"                                 "$TASK_FEAT"   "task stage feature bit"

echo
echo "${C_B}Properties${C_0}"
printf '  %-44s %s\n' "subgroupSize"                    "$SUBGROUP"
printf '  %-44s %s\n' "min / max subgroupSize"          "${MIN_SUBGROUP} / ${MAX_SUBGROUP}"
printf '  %-44s %s\n' "maxComputeWorkGroupInvocations"  "$MAX_CS_INVOC"
printf '  %-44s %s\n' "bindless sampled images / stage" "$MAX_BINDLESS"

# ----------------------------------------------------------------- verdict --

# M1 submission path.
if [ "$EXT_MESH" = "true" ] && [ "$MESH_FEAT" = "true" ]; then
    M1_PATH="mesh shaders available -- indirect draw stays primary, mesh path is the optional experiment"
    M1_LEVEL="ok"
elif [ "$DRAW_COUNT" = "true" ] || [ "$EXT_DRAW_COUNT" = "true" ]; then
    M1_PATH="indirect draw with a GPU-written count buffer"
    M1_LEVEL="ok"
elif [ "$MULTI_DRAW" = "true" ]; then
    M1_PATH="indirect draw at a fixed maximum count -- culled clusters must be zeroed out by the compute pass, not removed"
    M1_LEVEL="warn"
else
    M1_PATH="no GPU-driven submission at all -- the architecture does not apply to this device"
    M1_LEVEL="bad"
fi

# M3 rasterizer variant.
if [ "$IMG_ATOM64" = "true" ]; then
    M3_PATH="64-bit variant -- atomicMax on a uint64 image, (depth:32 | clusterTriangleID:32)"
    M3_LEVEL="ok"
elif [ "$BUF_ATOM64" = "true" ]; then
    M3_PATH="buffer-backed variant -- 64-bit atomics on a storage buffer, resolved to an image in a second pass"
    M3_LEVEL="warn"
else
    M3_PATH="32-bit variant -- atomicMax packing (depth:24 | index:8) per tile, plus a resolve pass"
    M3_LEVEL="warn"
fi

# Bindless prerequisites.
if [ "$BDA" = "true" ] && [ "$DESC_IDX" = "true" ] && [ "$RT_ARRAY" = "true" ]; then
    M0_PATH="bindless prerequisites met"
    M0_LEVEL="ok"
else
    M0_PATH="bindless prerequisites NOT met -- M0 cannot deliver its deliverable on this device"
    M0_LEVEL="bad"
fi

verdict()
{
    local level="$1" label="$2" text="$3" color
    case "$level" in
        ok)   color="$C_OK" ;;
        warn) color="$C_WARN" ;;
        *)    color="$C_NO" ;;
    esac
    printf '  %s%-4s%s %s\n' "$color" "$label" "$C_0" "$text"
}

echo
echo "${C_B}Decisions this forces${C_0}"
verdict "$M0_LEVEL" "M0" "$M0_PATH"
verdict "$M1_LEVEL" "M1" "$M1_PATH"
verdict "$M3_LEVEL" "M3" "$M3_PATH"
echo

# ------------------------------------------------------------------- write --

if [ "$DO_WRITE" = "1" ]; then
    mkdir -p "$(dirname "$WRITE_PATH")"
    {
        echo "# Device profile"
        echo
        echo "Generated by \`tools/probe_device_caps.sh\` on $(date -u '+%Y-%m-%d')."
        echo "Re-run and commit whenever the reference device changes."
        echo
        echo "| | |"
        echo "|---|---|"
        echo "| Device | ${DEVICE_NAME:-unknown} (GPU${SELECTED} of ${GPU_COUNT}) |"
        echo "| Driver | ${DRIVER_NAME:-unknown} ${DRIVER_INFO:-} |"
        echo "| API version | ${API_VERSION:-unknown} |"
        echo "| Subgroup size | ${SUBGROUP} (min ${MIN_SUBGROUP}, max ${MAX_SUBGROUP}) |"
        echo "| Max compute invocations | ${MAX_CS_INVOC} |"
        echo "| Bindless sampled images / stage | ${MAX_BINDLESS} |"
        echo
        echo "## Capabilities"
        echo
        echo "| Capability | State |"
        echo "|---|---|"
        echo "| bufferDeviceAddress | ${BDA} |"
        echo "| descriptorIndexing | ${DESC_IDX} |"
        echo "| runtimeDescriptorArray | ${RT_ARRAY} |"
        echo "| shaderSampledImageArrayNonUniformIndexing | ${NU_IMG} |"
        echo "| descriptorBindingPartiallyBound | ${PART_BOUND} |"
        echo "| multiDrawIndirect | ${MULTI_DRAW} |"
        echo "| drawIndirectCount | ${DRAW_COUNT} |"
        echo "| VK_KHR_draw_indirect_count | ${EXT_DRAW_COUNT} |"
        echo "| shaderDrawParameters | ${DRAW_PARAMS} |"
        echo "| shaderInt64 | ${INT64} |"
        echo "| shaderBufferInt64Atomics | ${BUF_ATOM64} |"
        echo "| VK_EXT_shader_image_atomic_int64 | ${EXT_IMG_ATOM64} |"
        echo "| shaderImageInt64Atomics | ${IMG_ATOM64} |"
        echo "| VK_EXT_mesh_shader | ${EXT_MESH} |"
        echo "| meshShader | ${MESH_FEAT} |"
        echo "| taskShader | ${TASK_FEAT} |"
        echo
        echo "## Decisions"
        echo
        echo "- **M0** -- ${M0_PATH}"
        echo "- **M1** -- ${M1_PATH}"
        echo "- **M3** -- ${M3_PATH}"
    } > "$WRITE_PATH"
    echo "Written to ${WRITE_PATH}"
    echo
fi

case "$M0_LEVEL" in bad) exit 1 ;; esac
exit 0

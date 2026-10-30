#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
kicad_state_dir=/tmp/niulai-kicad-state
mkdir -p "$kicad_state_dir/config" "$kicad_state_dir/cache" "$kicad_state_dir/data"

python3 "$script_dir/generate_carrier_schematic.py"

XDG_CONFIG_HOME="$kicad_state_dir/config" \
XDG_CACHE_HOME="$kicad_state_dir/cache" \
XDG_DATA_HOME="$kicad_state_dir/data" \
kicad-cli sch upgrade --force "$script_dir/niulai_carrier_v0.2.kicad_sch"

XDG_CONFIG_HOME="$kicad_state_dir/config" \
XDG_CACHE_HOME="$kicad_state_dir/cache" \
XDG_DATA_HOME="$kicad_state_dir/data" \
kicad-cli sch erc --severity-all --exit-code-violations --format report \
  --output "$script_dir/niulai_carrier_v0.2_erc.rpt" \
  "$script_dir/niulai_carrier_v0.2.kicad_sch"

XDG_CONFIG_HOME="$kicad_state_dir/config" \
XDG_CACHE_HOME="$kicad_state_dir/cache" \
XDG_DATA_HOME="$kicad_state_dir/data" \
kicad-cli sch export pdf --black-and-white \
  --output "$script_dir/niulai_carrier_v0.2.pdf" \
  "$script_dir/niulai_carrier_v0.2.kicad_sch"

mkdir -p "$script_dir/svg"
XDG_CONFIG_HOME="$kicad_state_dir/config" \
XDG_CACHE_HOME="$kicad_state_dir/cache" \
XDG_DATA_HOME="$kicad_state_dir/data" \
kicad-cli sch export svg --black-and-white \
  --output "$script_dir/svg" \
  "$script_dir/niulai_carrier_v0.2.kicad_sch"

# KiCad emits presentation-only trailing spaces in SVG style lines. Normalize
# the generated text so repository whitespace checks remain deterministic.
sed -i 's/[[:space:]]*$//' "$script_dir/svg/niulai_carrier_v0.2.svg"

XDG_CONFIG_HOME="$kicad_state_dir/config" \
XDG_CACHE_HOME="$kicad_state_dir/cache" \
XDG_DATA_HOME="$kicad_state_dir/data" \
kicad-cli sch export netlist --format kicadxml \
  --output "$script_dir/niulai_carrier_v0.2.net" \
  "$script_dir/niulai_carrier_v0.2.kicad_sch"
sed -i \
  -e 's#<source>[^<]*</source>#<source>niulai_carrier_v0.2.kicad_sch</source>#' \
  -e 's#<date>[^<]*</date>#<date>generated-by-build</date>#' \
  "$script_dir/niulai_carrier_v0.2.net"

python3 "$script_dir/verify_carrier_schematic.py"

zip -j -FS "$script_dir/niulai_carrier_v0.2_easyeda_import.zip" \
  "$script_dir/niulai_carrier_v0.2.kicad_sch" \
  "$script_dir/niulai_carrier_v0.2.kicad_pro" \
  "$script_dir/niulai_carrier_v0.2.kicad_sym" \
  "$script_dir/sym-lib-table" \
  "$script_dir/fp-lib-table" \
  "$script_dir/niulai_carrier_v0.2_bom.csv" \
  "$script_dir/niulai_carrier_v0.2_netlist.csv" \
  "$script_dir/../MODULE_PINOUT_CONTRACT_v0.3.csv" \
  "$script_dir/niulai_carrier_v0.2.net" \
  "$script_dir/niulai_carrier_v0.2_erc.rpt" \
  "$script_dir/niulai_carrier_v0.2.pdf" \
  "$script_dir/svg/niulai_carrier_v0.2.svg" \
  "$script_dir/README.md"

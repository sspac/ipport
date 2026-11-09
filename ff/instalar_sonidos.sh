#!/usr/bin/env bash
# desde ~/ipport:  bash ff/instalar_sonidos.sh
# Copia assets/fiendfolio (la misma estructura que resources/ del mod) a los assets del port y agrega las filas a index.txt.
# NO toca sounds.xml: ese parche se hace en memoria al arrancar (ffsound.c).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
A=port/app/src/main/assets
mkdir -p $A/fiendfolio
cp -r "$HERE/fiendfolio/." $A/fiendfolio/
rm -rf $A/port_patch/ff-sounds
sed -i '/ff-sounds\//d' $A/port_patch/index.txt
(cd $A && find fiendfolio/sounds -name '*.wav' | sort) | while read -r f; do
  grep -qF "$f" $A/port_patch/index.txt || printf 'file\t%s\t%s\n' "$f" "$f" >> $A/port_patch/index.txt
done
echo "filas de fiendfolio en index.txt: $(grep -c 'fiendfolio/' $A/port_patch/index.txt)"

#!/usr/bin/env bash
# Fetches the pinned third-party sources into third_party/ (JUCE, clap-juce-extensions and only
# the reverb folders of the Airwindows repository, which is otherwise several GB).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p third_party

JUCE_REF=3fb5cbc0340758596dac39f7c8de47d562b4896e        # JUCE 9.0.4
CLAPJUCE_REF=7adee3a1bd4684d4caa5601100e364abccff4b4d
AIRWINDOWS_REF=b79c7a3f9053f408f38f740bef56ca3291a09ad4

fetch () { # name url ref
    if [ ! -d "third_party/$1/.git" ]; then git init -q "third_party/$1"; git -C "third_party/$1" remote add origin "$2"; fi
    git -C "third_party/$1" fetch -q --depth 1 origin "$3"
    git -C "third_party/$1" checkout -q FETCH_HEAD
}

fetch JUCE https://github.com/juce-framework/JUCE.git "$JUCE_REF"
fetch clap-juce-extensions https://github.com/free-audio/clap-juce-extensions.git "$CLAPJUCE_REF"
git -C third_party/clap-juce-extensions submodule update -q --init --recursive --depth 1

if [ ! -d third_party/airwindows/.git ]; then
    git init -q third_party/airwindows
    git -C third_party/airwindows remote add origin https://github.com/airwindows/airwindows.git
    git -C third_party/airwindows config core.sparseCheckout true
fi
cat > third_party/airwindows/.git/info/sparse-checkout <<'SPARSE'
/LICENSE*
/Airwindopedia.txt
/plugins/LinuxVST/src/k*
/plugins/LinuxVST/src/*Verb*
/plugins/LinuxVST/src/*verb*
/plugins/LinuxVST/src/Galactic*
/plugins/LinuxVST/src/Chamber*
/plugins/LinuxVST/src/Infinity*
/plugins/LinuxVST/src/*Coat*
/plugins/LinuxVST/src/Ultralight*
/plugins/LinuxVST/src/NonlinearSpace*
/plugins/LinuxVST/src/BrightAmbience*
/plugins/LinuxVST/src/Reverb*
/plugins/LinuxVST/src/MV*
/plugins/LinuxVST/src/StarChild*
/plugins/LinuxVST/src/Melt*
/plugins/LinuxVST/src/MatrixVerb*
SPARSE
git -C third_party/airwindows fetch -q --depth 1 --filter=blob:none origin "$AIRWINDOWS_REF"
git -C third_party/airwindows checkout -q FETCH_HEAD
echo "third_party ready"

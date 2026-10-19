# Third-party notices

Airverbs' own code is public domain (The Unlicense, see `LICENSE`). It builds on the projects
below, which keep their own licences. None of them are vendored in this repository:
`tools/fetch_deps.sh` downloads pinned versions at build time. The generated files in
`source/airwindows/` (engine wrappers and metadata, including descriptions taken from the
Airwindopedia) are derived from Airwindows and are covered by its MIT licence below.

## Airwindows (MIT)
All 53 reverb algorithms are the original Airwindows LinuxVST sources by Chris Johnson,
compiled unmodified (see `source/airwindows/shim/audioeffectx.h`). Descriptions shown in the
GUI come from the Airwindopedia.

    MIT License
    Copyright (c) 2018 Chris Johnson

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
    and associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Support Chris: https://www.patreon.com/airwindows

## JUCE 9
Dual-licensed: AGPLv3 or a commercial JUCE licence (https://juce.com/legal/juce-8-licence/).
The Unlicense is compatible with the AGPLv3, so the Airverbs source can stay public domain.
Compiled Airverbs binaries contain JUCE, so anyone distributing them must follow JUCE's terms:
under the AGPLv3 that means making the complete corresponding source available. This public
repository does exactly that for the binaries its own GitHub Releases publish.

## clap-juce-extensions (MIT) and the CLAP SDK (MIT)
https://github.com/free-audio/clap-juce-extensions

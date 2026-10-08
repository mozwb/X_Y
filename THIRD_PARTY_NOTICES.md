# Third-party software

This repository includes or builds the following third-party components. Keep
their notices and license texts when redistributing source or binaries.

| Component | Source | License and distribution notes |
|---|---|---|
| FFmpeg | `vendor/FFmpeg` (Git submodule) | The configured build is LGPL 2.1-or-later (`CONFIG_GPL=0`, `CONFIG_NONFREE=0`, `CONFIG_VERSION3=0`). FFmpeg also contains optional GPL components which are not enabled by this build. Static linking has additional LGPL relinking/source requirements; see `vendor/FFmpeg/COPYING.LGPLv2.1` and `vendor/FFmpeg/LICENSE.md`. |
| FreeType | `vendor/freetype` | FreeType License (FTL), not its optional GPLv2 license. See `vendor/freetype/LICENSE.TXT` and `vendor/freetype/docs/FTL.TXT`. |
| glad | `vendor/glad` | MIT for glad; generated Khronos material carries additional Khronos notices. See `vendor/glad/LICENSE` and `vendor/glad/include/KHR/khrplatform.h`. |
| GLM | `vendor/glm` | Dual-licensed under the Happy Bunny License or MIT. X_Y selects the MIT option; see `vendor/glm/copying.txt`. |
| Dear ImGui | `vendor/imgui` (Git submodule) | MIT. See `vendor/imgui/LICENSE.txt`. |
| stb | `vendor/stb` (Git submodule) | Individual stb files are dual-licensed under MIT or public domain. X_Y selects MIT; see `vendor/stb/LICENSE`. |

The FFmpeg and codec copyright licenses do not grant patent rights. Before
commercial distribution, review the patent status of the codecs enabled in the
FFmpeg build (for example H.264, HEVC, and AAC) in the jurisdictions where the
software will be used or sold.

The root `LICENSE` applies to original X_Y project code only. Third-party
components remain under their respective licenses listed above; this project
license does not replace or override those terms.

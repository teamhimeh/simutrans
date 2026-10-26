# PNG heightmap export checks

## Automated checks

Build using the existing platform configuration, then run:

```sh
python tests/test_heightmap_export.py --cxx g++
python tests/test_keybindings_unicode.py --cxx g++
```

The heightmap check requires Python 3, a C++11 compiler and libpng development
files. It compiles the production exporter, image codecs and loader with an
in-memory world adapter. It verifies linear encoding including the world loader's
height offset, PNG dimensions/orientation, border-loss counts, Unicode paths,
existing-file preservation, rename and final-close failures, and filename length
boundaries. It does not exercise world generation or slope correction.

Run relevant existing scenarios following `tests/README.txt`:

```sh
WORKDIR="$SIMUTRANS_TEST_BASE" SIM_BINARY="$SIM_BINARY" tests/run-automated-tests.sh test_slope_set_and_restore
WORKDIR="$SIMUTRANS_TEST_BASE" SIM_BINARY="$SIM_BINARY" tests/run-automated-tests.sh test_slope_set_near_map_border
WORKDIR="$SIMUTRANS_TEST_BASE" SIM_BINARY="$SIM_BINARY" tests/run-automated-tests.sh test_slope_max_height_diff
WORKDIR="$SIMUTRANS_TEST_BASE" SIM_BINARY="$SIM_BINARY" tests/run-automated-tests.sh test_gui_accessibility_windows_and_components
```

The full suite runs in CI. Keep tests already registered in `failing_tests`
separate from regressions introduced by a change.

## Game checks

Build Windows previews with `USE_FREETYPE = 1`, as in `.github/build64.sh`,
and `STATIC = 1` for an executable that does not require extra runtime DLLs.
Before delivery, launch against a copy of current distribution data without a
`font/` directory. Confirm startup and Japanese text using Windows system fonts.
A successful launch with the legacy test base's bitmap fonts does not verify this.

1. Use a disposable save. Open the map and choose **Heightmap PNG**. Confirm the
   size, height limits, sea level and edge-height warning are readable, including in Japanese.
2. Save using a new name. Confirm a W by H, 8-bit grayscale PNG is created in
   `maps/`, the dialog closes and the success message includes height limits and sea level.
3. Try the same name again. Confirm the image is unchanged and the dialog stays
   open. Select a short existing name such as `a.png` and check the input updates.
4. Save a Japanese filename. Check it in the heightmap load dialog. Try an invalid
   directory and confirm failure is reported without leaving a partial PNG.
5. In a new game, load the PNG with **linear** conversion and the same height
   limits and sea level. Disable cities, industries, attractions, trees, rivers and
   lake generation when comparing terrain, so generation does not obscure the result.
6. Compare heights after world initialization, including its slope and coast
   corrections. Include a non-square map, negative heights, a steep slope and a
   coastline. Do not treat successful PNG decoding alone as terrain verification.
7. Repeat after rotating the map and with a large map. Check width/height and
   orientation. Compare right/bottom boundary vertices and the top-left vertex
   separately: the existing importer duplicates the former and may change the
   latter. Exact terrain identity is not promised by this image format.

Export reads only terrain vertices. It does not store buildings, ways, lake levels,
or the import settings inside the PNG. Keep the full savegame for complete backup.

# Homework 1 — n26140765 林郁家

Contents in `submission/`: modified/new files in `SourceCode/`, `Demo.mp4`,
`Report.pdf`, and editable `Report.docx`.
This follows the slide requirement to submit modified source only. Overlay
`SourceCode/` onto the course's `sample_code/` directory; retain the original
thirdparty libraries, shaders, teapot model, and UV texture.

## Linux

Install dependencies if needed:

```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglm-dev
```

Then, from the merged `sample_code/` directory:

The script configures a Release build, compiles it, and launches the program
with the four required file arguments. It requires an active graphical session
and an OpenGL 3.3-capable driver. Press Escape to exit.

Manual equivalent:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
cd build/bin/Release
./Homework01 './resources/model/Utah_teapot_(solid)_texture.obj' \
  './resources/texture/uv.png' './Shader/BasicVertexShader.vs.glsl' \
  './Shader/BasicFragmentShader.fs.glsl'
```

For Windows, follow the original slides' GLFW/CMake setup, compile Release,
and launch `Homework01.exe` from its output directory with the same four
relative resource/shader paths above. Windows execution was not tested.

## Controls and bonus features

- Camera Position and Look At: change view and target.
- Background color: edit clear color.
- Render mode: filled triangles or wireframe.
- Field of view: 20–90 degrees.
- Auto orbit camera: continuous orbit around the target.
- Model: original teapot or new UV-mapped cube.
- Texture: original UV image or new blue/gold checkerboard.
- Reset view: restore camera, target, field of view, background and fill mode.

Camera/target/background/wireframe controls already existed in the supplied
sample. The completed wrappers make rendering work. FOV, automatic orbit,
model selection, texture selection and reset behavior were added in this work.
Bonus points are subject to the instructor's decision.

## Demo

`submission/Demo.mp4` is converted from the supplied `submission/Demo.webm`
screen recording (about 67 seconds). It uses H.264 video and yuv420p pixel
format for compatibility. The original 993 x 615 image is preserved with a
one-pixel border on the right and bottom, producing 994 x 616 output.
The source WEBM remains available and is excluded from the submission ZIP.

To reproduce raw capture, create an empty directory and set `HW1_DEMO_DIR`
to its absolute path when running `bash run.sh`. The program writes 720 PPM
frames and exits. Leave this variable unset for normal interactive use.

## Asset provenance and AI use

The teapot and UV image are unchanged course assets. The cube and checker BMP
were generated specifically for this homework, with no downloaded assets.
Codex assisted in interpreting the slides, writing and repairing C++ code,
generating the two new assets, testing, recording the demo, and drafting the
report. Review the implementation and AI disclosure before submission.

[![](http://i.imgur.com/pE9Bswx.png)]()

NanoCL provides a thin wrapper on top of OpenGl functions for GPGPU programming. It can be used to allocate and manage GPU memory and run GLSL kernels on standard `float[]` arrays.

This library is the upstream source for [turbo.js](https://github.com/turbo/js), but is vastly more capable. 

Though NanoCL is much more trivial than OpenCL, it can provide certain advantages:

- virtually no compile-time overhead
- completely dependency-free
- compatible with almost all GPUs and GLSL-compatible software renderers
- transparent source code
- allows for easier tracing using performance analysis (perf, Amp, ...) because no superfluous levels of indirections are used

NanoCL was designed for Windows. It does not require any OpenGL wrapper libraries like GLU or GLFW, just the OpenGL headers. It was tested using TDM-GCC, ICC and MSVC.

Documentation is in progress.

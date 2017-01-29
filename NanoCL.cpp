// (c) 2014-2017, github.com/turbo
// MIT licensed

#include <windows.h>

#include <string>
#include <vector>

#include <GL/gl.h>

#ifndef __MINGW32__
#pragma comment(lib, "opengl32.lib") // MSVC, ICL
#pragma comment(lib, "gdi32.lib")    // ICL
#pragma comment(lib, "user32.lib")   // ICL
#endif

#define NanoCL_MAX_LOG_LENGTH 10000

#define NanoCL_V                                                               \
  "varying vec2 pos;void "                                                     \
  "main(void){pos=vec2(gl_MultiTexCoord0);gl_Position=gl_Vertex;}"

#define NanoCL_K                                                               \
  "varying vec2 pos;vec4 read(sampler2D m){return texture2D(m,pos);}"          \
  "void commit(vec4 d){gl_FragColor=d;}"

#define defPROC(a, b, ...)                                                     \
  typedef a(__stdcall *MCL##b)(__VA_ARGS__);                                   \
  MCL##b b = nullptr

#define loadPROC(a)                                                            \
  a = MCL##a((wglGetProcAddress(#a)));                                         \
  if (a == nullptr)                                                            \
    ExitProcess(printf("ERROR: Couldn't load GL(Ext) function %s.\n", #a));

#define kernel(k) #k

namespace NanoCL {
defPROC(const char *, glGetStringi, int, int);
defPROC(void, glActiveTexture, int);
defPROC(void, glAttachShader, unsigned, unsigned);
defPROC(void, glCompileShader, unsigned);
defPROC(void, glDeleteShader, unsigned);
defPROC(void, glGetInfoLogARB, unsigned, int, int *, char *);
defPROC(void, glGetObjectParameterivARB, unsigned, unsigned, int *);
defPROC(void, glLinkProgram, unsigned);
defPROC(void, glShaderSource, unsigned, int, const char **, const int *);
defPROC(void, glUniform1i, int, int);
defPROC(void, glUniform2fv, int, int, const float *);
defPROC(void, glUniform4fv, int, int, const float *);
defPROC(void, glUseProgram, unsigned);
defPROC(void, glBindFramebufferEXT, unsigned, unsigned);
defPROC(void, glDeleteFramebuffersEXT, int, const unsigned *);
defPROC(void, glFramebufferTexture2DEXT, unsigned, unsigned, unsigned, unsigned,
        int);
defPROC(void, glGenFramebuffersEXT, int, unsigned *);
defPROC(void, glGenerateMipmapEXT, unsigned);
defPROC(int, glGetUniformLocation, unsigned, const char *);
defPROC(int, glCreateProgram, void);
defPROC(int, glCreateShader, unsigned);

typedef struct NCL_vec4f { float r, g, b, a; } NCL_vec4f;

inline void gpgpu_fillscreen(void) {
  glBegin(6);
  glTexCoord2f(0.0f, 0.0f);
  glVertex3f(-1.0f, -1.0f, 0.0f);
  glTexCoord2f(1.0f, 0.0f);
  glVertex3f(+1.0f, -1.0f, 0.0f);
  glTexCoord2f(1.0f, 1.0f);
  glVertex3f(+1.0f, +1.0f, 0.0f);
  glTexCoord2f(0.0f, 1.0f);
  glVertex3f(-1.0f, +1.0f, 0.0f);
  glEnd();
}

class gpgpu_texture2D {
public:
  unsigned handle;
  int width, height;

  gpgpu_texture2D(int w, int h) : width(w), height(h) {
    glGenTextures(1, &handle);
    update_data(nullptr);
    bind();
    glTexParameteri(0x0DE1, 0x2802, 0x812F);
    glTexParameteri(0x0DE1, 0x2803, 0x812F);
    bind();
    glTexParameteri(0x0DE1, 0x2801, 0x2600);
    glTexParameteri(0x0DE1, 0x2800, 0x2600);
    glGenerateMipmapEXT(0x0DE1);
  }

  ~gpgpu_texture2D() {
    glDeleteTextures(1, &handle);
    handle = 0;
  }

  void draw(void) const {
    bind();
    glEnable(0x0DE1);
    gpgpu_fillscreen();
  }

  void bind(int texture_unit = 0) const {
    glActiveTexture(0x84C0 + texture_unit);
    glBindTexture(0x0DE1, handle);
  }

  void update_data(const float *dat) {
    bind();
    glTexImage2D(0x0DE1, 0, 0x8814, width, height, 0, 0x1908, 0x1406, dat);
  }

private:
  gpgpu_texture2D(const gpgpu_texture2D &src);
  void operator=(const gpgpu_texture2D &src);
};

inline void gpgpu_tex_scale(unsigned program, gpgpu_texture2D *tex,
                            const std::string &name) {
  auto scale = glGetUniformLocation(program, (name + "Scale").c_str());
  if (scale <= -1)
    return;
  const float argARB[] = {1.0f / tex->width, 1.0f / tex->height, 0.0f, 0.0f};
  glUniform2fv(scale, 1, argARB);
}

inline void gpgpu_add(unsigned program, gpgpu_texture2D *tex,
                      const std::string &name, int texture_unit = 0) {
  glUseProgram(program);
  tex->bind(texture_unit);
  glUniform1i(glGetUniformLocation(program, name.c_str()), texture_unit);
  gpgpu_tex_scale(program, tex, name);
}

class gpgpu_framebuffer {
public:
  unsigned handle;
  gpgpu_texture2D *tex;

  explicit gpgpu_framebuffer(gpgpu_texture2D *tex_) {
    glGenFramebuffersEXT(1, &handle);
    attach(tex_);
  }

  ~gpgpu_framebuffer() {
    glDeleteFramebuffersEXT(1, &handle);
    handle = 0;
  }

  void run(unsigned prog) const {
    bind();
    glUseProgram(prog);
    auto scale_index = glGetUniformLocation(prog, "locationScale");
    if (scale_index > -1) {
      const float argARB[] = {float(tex->width), float(tex->height), 0.0f,
                              0.0f};
      glUniform4fv(scale_index, 1, argARB);
    }
    gpgpu_fillscreen();
  }

  void read(float *destination, int width, int height) const {
    bind();
    glReadPixels(0, 0, width, height, 0x1908, 0x1406, destination);
  }

  void bind(void) const {
    glBindFramebufferEXT(0x8D40, handle);
    if (tex)
      glViewport(0, 0, tex->width, tex->height);
  }

  void attach(gpgpu_texture2D *tex_) {
    tex = tex_;
    if (!tex)
      return;
    glBindFramebufferEXT(0x8D40, handle);
    glFramebufferTexture2DEXT(0x8D40, 0x8CE0, 0x0DE1, tex->handle, 0);
  }

private:
  gpgpu_framebuffer(const gpgpu_framebuffer &src);
  void operator=(const gpgpu_framebuffer &src);
};

inline void gpgpu_init() {
  static auto gpgpu_initted = false;
  if (gpgpu_initted)
    return;
  gpgpu_initted = true;

  const static PIXELFORMATDESCRIPTOR pfd = {
      0, 0, PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0, 0};

  auto hDC = GetDC(CreateWindow(
#if (defined(_MSC_VER) && !defined(__INTEL_COMPILER))
      LPCWSTR
#else
      LPCSTR
#endif
      ("edit"),
      nullptr, WS_POPUP | WS_MINIMIZE, 0, 0, 0, 0, nullptr, nullptr, nullptr,
      nullptr));

  SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
  wglMakeCurrent(hDC, wglCreateContext(hDC));

  int extListSize = 0;
  glGetIntegerv(0x821D, &extListSize);

  if (extListSize == 0)
    ExitProcess(printf("ERROR: No GPU extensions detected (OpenGL context init "
                       "might have failed).\n"));

#pragma warning(disable : 4312)
  loadPROC(glActiveTexture);
  loadPROC(glGetUniformLocation);
  loadPROC(glAttachShader);
  loadPROC(glCompileShader);
  loadPROC(glCreateProgram);
  loadPROC(glCreateShader);
  loadPROC(glDeleteShader);
  loadPROC(glGetInfoLogARB);
  loadPROC(glGetObjectParameterivARB);
  loadPROC(glGetUniformLocation);
  loadPROC(glLinkProgram);
  loadPROC(glShaderSource);
  loadPROC(glUniform1i);
  loadPROC(glUniform2fv);
  loadPROC(glUniform4fv);
  loadPROC(glUseProgram);
  loadPROC(glBindFramebufferEXT);
  loadPROC(glDeleteFramebuffersEXT);
  loadPROC(glFramebufferTexture2DEXT);
  loadPROC(glGenFramebuffersEXT);
  loadPROC(glGenerateMipmapEXT);
  loadPROC(glGetStringi);
#pragma warning(default : 4312)

  glDisable(0x0B71);
  glDisable(0x0BC0);
  glDisable(0x0BE2);
}

class gpgpu_array;

class context {
public:
  explicit context() { gpgpu_init(); }

  std::vector<gpgpu_array *> tex;
  std::string utils;

  void add_array(gpgpu_array *t) { tex.push_back(t); }
};

class gpgpu_array : public gpgpu_texture2D, public gpgpu_framebuffer {
public:
  context &env;
  std::string name;

  gpgpu_array(context &env_, std::string name, int w, int h)
      : gpgpu_texture2D(w, h), gpgpu_framebuffer(this), env(env_), name(name) {
    env.add_array(this);
  }

  std::string get_tex_decls(void) const {
    std::string s;
    for (unsigned i = 0; i < env.tex.size(); i++)
      s += "uniform sampler2D " + env.tex[i]->name + ";uniform vec2 " +
           env.tex[i]->name + "Scale;";
    return s + env.utils;
  }

  void swap(gpgpu_array &arr) {
    std::swap(gpgpu_framebuffer::handle, arr.gpgpu_framebuffer::handle);
    std::swap(gpgpu_texture2D::handle, arr.gpgpu_texture2D::handle);
  }
};

inline unsigned gpgpu_runprep(gpgpu_array &dest, unsigned code) {
  auto &env = dest.env;
  unsigned texunit = 0;
  for (unsigned i = 0; i < env.tex.size(); i++) {
    if (env.tex[i] != &dest)
      gpgpu_add(code, env.tex[i], env.tex[i]->name, texunit++);
    else
      gpgpu_tex_scale(code, env.tex[i], env.tex[i]->name);
  }
  return code;
}

// Debug GLSL
void checkShaderOp(unsigned obj, unsigned errType, const char *where) {
  int compiled;
  glGetObjectParameterivARB(obj, errType, &compiled);
  if (compiled)
    return;
  char errorLog[NanoCL_MAX_LOG_LENGTH];
  glGetInfoLogARB(obj, NanoCL_MAX_LOG_LENGTH, nullptr, errorLog);

  printf("ERROR: Could not build GLSL shader (fatal).\n\n--- CODE DUMP "
         "---\n%s\n\n--- ERROR LOG ---\n%s\n\n",
         where, errorLog);
}

unsigned makeShaderObject(int target, const char *code) {
  auto h = glCreateShader(target);
  glShaderSource(h, 1, &code, nullptr);
  glCompileShader(h);
  checkShaderOp(h, 0x8B81, code);
  return h;
}

unsigned makeProgramObject(const char *vertex, const char *fragment) {
  if (glUseProgram == nullptr)
    printf("ERROR: glUseProgram could not be loaded.\n");

  auto p = glCreateProgram();
  auto vo = makeShaderObject(0x8B31, vertex);
  auto fo = makeShaderObject(0x8B30, fragment);

  glAttachShader(p, vo);
  glAttachShader(p, fo);
  glLinkProgram(p);
  checkShaderOp(p, 0x8B82, "link");
  glDeleteShader(vo);
  glDeleteShader(fo);

  return p;
}

struct alloc {
  NCL_vec4f *data;
  unsigned dataWidth;   // (CPU) [internal] texture width
  unsigned dataHeight;  // (CPU) [internal] texture height
  gpgpu_array *gpuData; // (GPU) float-texture

  alloc(context &gpuCtx, std::string UID, unsigned length) {
    dataWidth = length;

    int x, y;

    for (x = 0; y = length / ++x | 0, x <= y;)
      if (!(x * y - length))
        dataHeight = y;

    dataWidth = length / dataHeight;

    data = new NCL_vec4f[length]();
    gpuData = new gpgpu_array(gpuCtx, UID, dataWidth, dataHeight);
  }
};

void push(alloc uID) { uID.gpuData->update_data((float *)(uID.data)); }

int make(alloc uID, const char *kernel) {
  return makeProgramObject(
      NanoCL_V,
      (NanoCL_K + (*uID.gpuData).get_tex_decls() + std::string(kernel))
          .c_str());
}

void run(alloc uID, const char *kernel) {
  (*uID.gpuData).run(gpgpu_runprep(*uID.gpuData, make(uID, kernel)));
}

void run(alloc uID, int progID) {
  (*uID.gpuData).run(gpgpu_runprep(*uID.gpuData, progID));
}

void pull(alloc uID) {
  uID.gpuData->read(((float *)(uID.data)), uID.dataWidth, uID.dataHeight);
}

void swap(alloc A, alloc B) { A.gpuData->swap(*B.gpuData); }
}

#undef defPROC
#undef loadPROC

/* EOF */

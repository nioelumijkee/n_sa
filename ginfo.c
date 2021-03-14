#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#define MAXDISP 16

typedef struct _ginfo
{
  char *opengl_ver;
  SDL_version sdl_ver_compiled;
  SDL_version sdl_ver_linked;
  char *driver;
  int num_disp;
  SDL_DisplayMode display_mode[MAXDISP];
  char *display_name[MAXDISP];
  float ddpi;
  float hdpi;
  float vdpi;
} t_ginfo;

//----------------------------------------------------------------------------//
void get_ginfo(t_ginfo *g)
{
  int i;
  
  // opengl version
  g->opengl_ver = (char *)glGetString(GL_VERSION);
  
  // SDL version
  SDL_VERSION(&g->sdl_ver_compiled);
  SDL_GetVersion(&g->sdl_ver_linked);
  
  //  driver
  g->driver = (char *)SDLCALL SDL_GetCurrentVideoDriver();
  
  // number displays
  g->num_disp = SDL_GetNumVideoDisplays();
  
  // get display mode of all displays.
  for (i = 0; (i < g->num_disp) && (i < MAXDISP); ++i)
    {
      int should_be_zero = SDL_GetCurrentDisplayMode(i, &g->display_mode[i]);
      g->display_name[i] = (char *)SDL_GetDisplayName(i);
      if (should_be_zero != 0)
	post("Could not get display mode for video display #%d\n", i);
    }
  
  // pixel format
  SDL_GetDisplayDPI(0, &g->ddpi, &g->hdpi, &g->vdpi);
}

//----------------------------------------------------------------------------//
void post_ginfo(t_ginfo g)
{
  post("<----------Info---------->");
  post("OpenGL version: %s", g.opengl_ver);
  post("SDL version compiled: %d.%d.%d", g.sdl_ver_compiled.major, g.sdl_ver_compiled.minor, g.sdl_ver_compiled.patch);
  post("SDL version linked: %d.%d.%d", g.sdl_ver_linked.major, g.sdl_ver_linked.minor, g.sdl_ver_linked.patch);
  post("Driver: %s", g.driver);
  post("Displays: %d", g.num_disp);
  for (int i = 0; i < g.num_disp; ++i)
    {
      post("Display[%d]: [%s] %dx%dpx %dhz", i, g.display_name[i], g.display_mode[i].w, g.display_mode[i].h, g.display_mode[i].refresh_rate);
    }
  post("ddpi: %f", g.ddpi);
  post("hdpi: %f", g.hdpi);
  post("vdpi: %f", g.vdpi);
}

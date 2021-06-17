#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#define MAXDISP 16

typedef struct _gi
{
  char           *opengl_ver;
  char           *opengl_vendor;
  char           *opengl_ext;
  SDL_version     sdl_ver_compiled;
  SDL_version     sdl_ver_linked;
  char           *driver;
  int             num_disp;
  SDL_DisplayMode display_mode[MAXDISP];
  char           *display_name[MAXDISP];
  float           ddpi[MAXDISP];
  float           hdpi[MAXDISP];
  float           vdpi[MAXDISP];
} t_gi;

//----------------------------------------------------------------------------//
void init_gi(t_gi *g)
{
  int i;
  g->opengl_ver = "NULL";
  g->opengl_vendor = "NULL";
  g->opengl_ext = "NULL";
  g->sdl_ver_compiled.major = 0;
  g->sdl_ver_compiled.minor = 0;
  g->sdl_ver_compiled.patch = 0;
  g->sdl_ver_linked.major = 0;
  g->sdl_ver_linked.minor = 0;
  g->sdl_ver_linked.patch = 0;
  g->driver = "NULL";
  g->num_disp = 0;
  for(i=0; i<MAXDISP; i++)
    {
      g->display_mode[i].w = 0;
      g->display_mode[i].h = 0;
      g->display_mode[i].refresh_rate = 0;
      g->display_name[i] = "NULL";
      g->ddpi[i] = 0;
      g->hdpi[i] = 0;
      g->vdpi[i] = 0;
    }
}


//----------------------------------------------------------------------------//
void get_gi(t_gi *g)
{
  int i;
  
  // opengl
  g->opengl_ver = (char *)glGetString(GL_VERSION);
  g->opengl_vendor = (char *)glGetString(GL_VENDOR);
  g->opengl_ext = (char *)glGetString(GL_EXTENSIONS);
  
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
      if (should_be_zero != 0)
        {
          printf("Could not get display mode for video display #%d\n", i);
        }
      else
        {
          g->display_name[i] = (char *)SDL_GetDisplayName(i);
          SDL_GetDisplayDPI(0, &g->ddpi[i], &g->hdpi[i], &g->vdpi[i]);
        }
    }
  
}

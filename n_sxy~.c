/* multiwave xy-scope. v0.2 */

#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "m_pd.h"


#define AF_CLIP_MINMAX(MIN, MAX, IN)            \
  if      ((IN) < (MIN))  (IN) = (MIN);         \
  else if ((IN) > (MAX))  (IN) = (MAX);

#define MAXCHANNEL 16

static t_class *n_sxy_class;

typedef struct _n_sxy_channel
{
  GLfloat *x;
  GLfloat *y;
  GLfloat x_z;
  GLfloat y_z;
  GLfloat cr;
  GLfloat cg;
  GLfloat cb;
  GLfloat ca;
  GLfloat lw;
} t_n_sxy_channel;

typedef struct _n_sxy
{
  t_object x_obj;
  t_outlet *out;               /* outlet */
  t_float sr;                  /* sample rate */
  t_clock *t;                  /* clock */
  int ttime;

  int window_on;               /* window */
  int window_w;
  int window_h;
  int window_w_z;
  int window_h_z;
  int window_size_changed;

  GLfloat back_cr;             /* background */
  GLfloat back_cg;
  GLfloat back_cb;
  GLfloat back_ca;

  int grid;                    /* grid */
  GLfloat grid_cr;
  GLfloat grid_cg;
  GLfloat grid_cb;
  GLfloat grid_ca;
  GLfloat grid_lw;

  SDL_Window *win;             /* win */
  SDL_GLContext glcontext;
  SDL_Event event;

  int on;                      /* scope */
  int channel;
  int channel_all;
  int s_max;
  int s_max_1;
  int count;
  t_n_sxy_channel ch[MAXCHANNEL];
} t_n_sxy;

//----------------------------------------------------------------------------//
void n_sxy_redraw(t_n_sxy *x)
{
  int i, j;
  float x_z;
  float y_z;
  
  // backgroud
  glColor4f(x->back_cr, x->back_cg, x->back_cb, x->back_ca);
  glBegin(GL_POLYGON);
  glVertex2f(-1., -1.);
  glVertex2f(-1., 1.);
  glVertex2f(1., 1.);
  glVertex2f(1., -1.);
  glEnd();
  
  // grid
  if (x->grid)
    {
      glDisable(GL_LINE_SMOOTH);
      glLineWidth(x->grid_lw);
      glColor4f(x->grid_cr, x->grid_cg, x->grid_cb, x->grid_ca);
      glBegin(GL_LINES);
      glVertex2f(-1., 0.);
      glVertex2f(1., 0.);
      glEnd();
      glBegin(GL_LINES);
      glVertex2f(0., 1.);
      glVertex2f(0., -1.);
      glEnd();
      glEnable(GL_LINE_SMOOTH);
    }
  
  // channels
  if (x->window_on)
    {
      for (j = 0; j < x->channel; j++)
        {
          glLineWidth(x->ch[j].lw);
          x_z = x->ch[j].x_z;
          y_z = x->ch[j].y_z;
          i = 0;
          while (i < x->s_max)
            {
              GLfloat f = x_z - x->ch[j].x[i]; // diff
              if (f < 0.)
                f = 0. - f;
              AF_CLIP_MINMAX(0., 1., f);
              f = 1. - f;
              f = f * f * f * f;
              f = f * x->ch[j].ca;
              glColor4f(x->ch[j].cr, x->ch[j].cg, x->ch[j].cb, f);
              glBegin(GL_LINES);
              glVertex2f(x_z, y_z);
              glVertex2f(x->ch[j].x[i], x->ch[j].y[i]);
              glEnd();
              x_z = x->ch[j].x[i];
              y_z = x->ch[j].y[i];
              i++;
            }
          x->ch[j].x_z = x_z;
          x->ch[j].y_z = y_z;
        }
    }
  
  glFlush();
  SDL_GL_SwapWindow(x->win);
}

//----------------------------------------------------------------------------//
t_int *n_sxy_perform(t_int *w)
{
  t_n_sxy *x = (t_n_sxy *)(w[1]);
  int n = (int)(w[2]);
  t_float **in_x = getbytes(sizeof(t_float) * x->channel);
  t_float **in_y = getbytes(sizeof(t_float) * x->channel);
  int i, j;
  for (i = 0; i < x->channel; i++)
    {
      j = i + i;
      in_x[i] = (t_float *)(w[j + 3]);
      in_y[i] = (t_float *)(w[j + 4]);
    }
  
  if (x->on)
    {
      // dsp
      while (n--)
        {
          for (i = 0; i < x->channel; i++)
            {
              x->ch[i].x[x->count] = *(in_x[i]++);
              x->ch[i].y[x->count] = *(in_y[i]++);
            }
          x->count++;
          if (x->count == x->s_max)
            {
              x->count = 0;
              n_sxy_redraw(x);
            }
        }
    }
  freebytes(in_x, sizeof(t_float) * x->channel);
  freebytes(in_y, sizeof(t_float) * x->channel);
  return (w + x->channel_all + 3);
}

//----------------------------------------------------------------------------//
void n_sxy_calc_constant(t_n_sxy *x)
{
  x->s_max = x->sr / x->ttime;
  x->s_max_1 = x->s_max - 1;
  for (int i = 0; i < x->channel; i++)
    {
      free(x->ch[i].x);
      free(x->ch[i].y);
      x->ch[i].x = malloc(sizeof(float) * x->s_max);
      x->ch[i].y = malloc(sizeof(float) * x->s_max);
      if (x->ch[i].x == NULL || x->ch[i].y == NULL)
        error("n_sxy: error allocating memory");
    }
  x->count = 0;
}

//----------------------------------------------------------------------------//
void n_sxy_dsp(t_n_sxy *x, t_signal **sp)
{
  int i;
  t_int **vec = getbytes(sizeof(t_int) * (x->channel_all + 2));
  
  x->sr = sp[0]->s_sr;
  n_sxy_calc_constant(x);
  
  vec[0] = (t_int *)x;
  vec[1] = (t_int *)sp[0]->s_n;
  for (i = 0; i < x->channel_all; i++)
    vec[i + 2] = (t_int *)sp[i]->s_vec;
  dsp_addv(n_sxy_perform, x->channel_all + 2, (t_int *)vec);
  freebytes(vec, sizeof(t_int) * (x->channel_all + 2));
}

//----------------------------------------------------------------------------//
void n_sxy_output_size(t_n_sxy *x)
{
  t_atom a[2];
  SETFLOAT(a, (t_float)x->window_w);
  SETFLOAT(a + 1, (t_float)x->window_h);
  outlet_anything(x->out, gensym("size"), 2, a);
}

//----------------------------------------------------------------------------//
void n_sxy_output_window(t_n_sxy *x)
{
  t_atom a[1];
  SETFLOAT(a, (t_float)x->window_on);
  outlet_anything(x->out, gensym("window"), 1, a);
}

//----------------------------------------------------------------------------//
void n_sxy_output_save(t_n_sxy *x, int i)
{
  t_atom a[1];
  SETFLOAT(a, (t_float)i);
  outlet_anything(x->out, gensym("save"), 1, a);
}

//----------------------------------------------------------------------------//
void n_sxy_reshape(t_n_sxy *x)
{
  glClearColor(0., 0., 0., 0.);
  glViewport(0, 0, (GLsizei)x->window_w, (GLsizei)x->window_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(-1., -1., 1., 1.);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_LINE_SMOOTH);
}

//----------------------------------------------------------------------------//
void n_sxy_window_size(t_n_sxy *x, t_floatarg w, t_floatarg h)
{
  AF_CLIP_MINMAX(32, 4096, w);
  AF_CLIP_MINMAX(32, 4096, h);
  x->window_w = w;
  x->window_h = h;
  x->window_size_changed = 1;
}

//----------------------------------------------------------------------------//
void n_sxy_window_grid(t_n_sxy *x, t_floatarg f)
{
  x->grid = f;
}

//----------------------------------------------------------------------------//
void n_sxy_window(t_n_sxy *x, t_floatarg f)
{
  x->window_on = f;
  
  if (x->window_on)
    {
      // sdl
      if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
        {
          error("SDL_Init error: %s", SDL_GetError());
          return;
        }
      
      // window
      if ((x->win = SDL_CreateWindow("n_sxy~",
                                     32,
                                     32,
                                     x->window_w,
                                     x->window_h,
                                     SDL_WINDOW_SHOWN | 
				     SDL_WINDOW_RESIZABLE | 
				     SDL_WINDOW_OPENGL)) == NULL)
        {
          error("SDL_CreateWindow error: %s", SDL_GetError());
	  return;
        }
      
      x->glcontext = SDL_GL_CreateContext(x->win);
      /* x->ttime = x->gi.display_mode[0].refresh_rate / 2; */
      n_sxy_reshape(x);
      n_sxy_redraw(x);
      clock_delay(x->t, x->ttime);
    }
  else
    {
      SDL_DestroyWindow(x->win);
      SDL_Quit();
      clock_unset(x->t);
    }
  n_sxy_output_window(x);
}

//----------------------------------------------------------------------------//
void n_sxy_events(t_n_sxy *x)
{
  clock_delay(x->t, x->ttime);
  if (x->window_on)
    {
      while (SDL_PollEvent(&x->event))
        {
          switch (x->event.type)
            {
            case SDL_WINDOWEVENT:
              switch (x->event.window.event)
                {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                  x->window_w = x->event.window.data1;
                  x->window_h = x->event.window.data2;
                  n_sxy_reshape(x);
                  n_sxy_redraw(x);
                  break;
                case SDL_WINDOWEVENT_RESIZED:
                  n_sxy_output_size(x);
                  break;
                case SDL_WINDOWEVENT_CLOSE:
                  n_sxy_window(x, 0);
                  break;
                }
              break;
            case SDL_KEYDOWN:
              switch (x->event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                  n_sxy_window(x, 0);
                  break;
                }
              break;
            }
          break;
        }
    }
  //
  if (x->window_on && x->window_size_changed)
    {
      /* SDL_SetWindowDisplayMode(x->win, &x->gi.display_mode[0]); */
      SDL_SetWindowSize(x->win, x->window_w, x->window_h);
      n_sxy_reshape(x);
      n_sxy_redraw(x);
      x->window_size_changed = 0;
      n_sxy_output_size(x);
    }
}

//----------------------------------------------------------------------------//
void n_sxy_color(int color, GLfloat *r, GLfloat *g, GLfloat *b)
{
  Uint8 ub = color;
  Uint8 ug = color >> 8;
  Uint8 ur = color >> 16;
  *b = (float)ub / 255.;
  *g = (float)ug / 255.;
  *r = (float)ur / 255.;
}

//----------------------------------------------------------------------------//
void n_sxy_on(t_n_sxy *x, t_floatarg f)
{
  x->on = f;
}

//----------------------------------------------------------------------------//
void n_sxy_set(t_n_sxy *x, t_floatarg ch, t_floatarg color, t_floatarg a, t_floatarg lw)
{
  // background
  if (ch == -2)
    {
      n_sxy_color((int)color, &x->back_cr, &x->back_cg, &x->back_cb);
      AF_CLIP_MINMAX(0., 1., a);
      x->back_ca = a;
    }
  // grid
  else if (ch == -1)
    {
      n_sxy_color((int)color, &x->grid_cr, &x->grid_cg, &x->grid_cb);
      AF_CLIP_MINMAX(0., 1., a);
      x->grid_ca = a;
      x->grid_lw = lw;
    }
  // channels
  else if (ch >= 0 && ch < MAXCHANNEL)
    {
      n_sxy_color((int)color, &x->ch[(int)ch].cr, &x->ch[(int)ch].cg, &x->ch[(int)ch].cb);
      AF_CLIP_MINMAX(0., 1., a);
      x->ch[(int)ch].ca = a;
      x->ch[(int)ch].lw = lw;
    }
}

//----------------------------------------------------------------------------//
void n_sxy_save(t_n_sxy *x, t_symbol *s)
{
  const int number_of_pixels = x->window_w * x->window_h * 3;
  unsigned char pixels[number_of_pixels];
  FILE *output_file = NULL;
  
  if (x->window_on)
    {
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadBuffer(GL_FRONT);
      glReadPixels(0, 0, x->window_w, x->window_h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    }
  else
    {
      error("n_sxy~: window");
      n_sxy_output_save(x, 0);
      return;
    }
  
  output_file = fopen(s->s_name, "w");
  if (output_file != NULL)
    {
      unsigned char size_w0 = x->window_w / 256;
      unsigned char size_w1 = x->window_w - (size_w0 * 256);
      unsigned char size_h0 = x->window_h / 256;
      unsigned char size_h1 = x->window_h - (size_h0 * 256);
      unsigned char header[] = {0, 0, 2, 0, 0, 0, 0, 0, 
                                0, 0, 
                                size_h1, size_h0,
                                size_w1, size_w0,
                                size_h1, size_h0,
                                24, 32};
      fwrite(&header, sizeof(header), 1, output_file);
      fwrite(pixels, number_of_pixels, 1, output_file);
      fclose(output_file);
      post("save to file: %s", s->s_name);
      n_sxy_output_save(x, 1);
      return;
    }
  else
    {
      error("n_sxy~: file can't open");
      n_sxy_output_save(x, 0);
      return;
    }
}

//----------------------------------------------------------------------------//
void *n_sxy_new(t_floatarg f)
{
  int i;
  t_n_sxy *x = (t_n_sxy *)pd_new(n_sxy_class);
  AF_CLIP_MINMAX(1, MAXCHANNEL, f);
  x->channel = f;
  x->channel_all = x->channel * 2;
  for (i = 1; i < x->channel_all; i++)
    {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }
  x->out = outlet_new(&x->x_obj, 0);
  x->sr = 44100.;
  x->ttime = 60 / 2; /* default */
  x->on = 0;
  x->window_on = 0;
  x->window_w = 400;
  x->window_h = 400;
  x->t = clock_new(x, (t_method)n_sxy_events);
  for (i = 0; i < x->channel; i++)
    {
      x->ch[i].x = NULL;
      x->ch[i].y = NULL;
    }
  return (x);
}

//----------------------------------------------------------------------------//
void n_sxy_free(t_n_sxy *x)
{
  SDL_DestroyWindow(x->win);
  SDL_Quit();
  clock_free(x->t);
  for (int i = 0; i < x->channel; i++)
    {
      free(x->ch[i].x);
      free(x->ch[i].y);
    }
}

//----------------------------------------------------------------------------//
#define METHOD0(F,S) \
class_addmethod(n_sxy_class,(t_method)(F),gensym((S)),0);
#define METHOD1(F,S) \
class_addmethod(n_sxy_class,(t_method)(F),gensym((S)),A_FLOAT,0);
#define METHOD2(F,S) \
class_addmethod(n_sxy_class,(t_method)(F),gensym((S)),A_FLOAT,A_FLOAT,0);
#define METHOD3(F,S) \
class_addmethod(n_sxy_class,(t_method)(F),gensym((S)),A_FLOAT,A_FLOAT,A_FLOAT,0);
#define METHODS(F,S) \
class_addmethod(n_sxy_class,(t_method)(F),gensym((S)),A_SYMBOL,0);

void n_sxy_tilde_setup(void)
{
  n_sxy_class = class_new(gensym("n_sxy~"),
			     (t_newmethod)n_sxy_new,
			     (t_method)n_sxy_free, 
			     sizeof(t_n_sxy),
			     0, A_GIMME, 0);

  METHOD0(nullfn,                      "signal");
  METHOD0(n_sxy_dsp,                   "dsp");
  /* window */
  METHOD1(n_sxy_window,                "window");
  METHOD1(n_sxy_window_ox,             "window_ox"); 
  METHOD1(n_sxy_window_oy,             "window_oy");
  METHOD1(n_sxy_window_w,              "window_w");
  METHOD1(n_sxy_window_h,              "window_h");
  METHOD1(n_sxy_window_fps,            "window_fps");
  /* par */
  METHOD2(n_sxy_color_back,            "par_back");
  METHOD3(n_sxy_color_grid,            "par_grid");
  METHOD3(n_sxy_color_ch,              "par_ch");
  /* settings */
  METHOD1(n_sxy_scope_grid_view,       "grid_view");
  METHOD1(n_sxy_scope_grid_ver,        "grid_ver");
  METHOD1(n_sxy_scope_grid_hor,        "grid_hor");
  METHOD1(n_sxy_scope_freeze,          "freeze");
  /* save */
  METHODS(n_sxy_save,                  "save");
}

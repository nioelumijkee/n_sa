/* multiwave xy-scope. v0.2 */

//----------------------------------------------------------------------------//
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "m_pd.h"

//----------------------------------------------------------------------------//
#define CHANNEL_MAX 16
#define BUFFER_MAX 65536
#define WINDOW_HEIGHT_MIN 4
#define WINDOW_HEIGHT_MAX 4096
#define WINDOW_WIDTH_MIN 4
#define WINDOW_WIDTH_MAX 4096
#define WINDOW_FPS_MIN 1.0
#define WINDOW_FPS_MAX 100.0
#define GRID_VER_MIN 0
#define GRID_VER_MAX 31
#define GRID_HOR_MIN 0
#define GRID_HOR_MAX 31

//----------------------------------------------------------------------------//
#define CLIP_MINMAX(MIN, MAX, IN)            \
  if      ((IN) < (MIN))  (IN) = (MIN);         \
  else if ((IN) > (MAX))  (IN) = (MAX);

//----------------------------------------------------------------------------//
static t_class *n_sxy_class;

//----------------------------------------------------------------------------//
typedef struct _n_sxy_color
{
  GLfloat r;
  GLfloat g;
  GLfloat b;
  GLfloat a;
} t_n_sxy_color;

//----------------------------------------------------------------------------//
typedef struct _n_sxy_channel
{
  int on;
  GLfloat x[BUFFER_MAX];
  GLfloat y[BUFFER_MAX];
  t_n_sxy_color c;
  GLfloat w;
} t_n_sxy_channel;

//----------------------------------------------------------------------------//
typedef struct _n_sxy
{
  t_object x_obj;  /* pd */
  t_outlet *x_out; /* outlet */
  int      s_n;    /* blocksize */
  t_float  s_sr;   /* sample rate */
  t_int    **v_d;  /* vector for dsp_addv */

  /* window */
  int window_on;
  int window_w;
  int window_h;
  int window_ox;
  int window_oy;
  int i_window_w;
  int i_window_h;
  int window_moved;

  int grid_view;

  int grid_hor;
  GLfloat grid_hor_y_c;
  GLfloat grid_hor_y_up[GRID_HOR_MAX];
  GLfloat grid_hor_y_dw[GRID_HOR_MAX];

  int grid_ver;
  GLfloat grid_ver_x[GRID_VER_MAX];

  int update;

  /* colors */
  t_n_sxy_color c_back;
  t_n_sxy_color c_grid;
  GLfloat       w_grid;

  /* channels */
  int channel;
  int amount_channel;

  /* scope */
  int bufmax;
  int bufmax_1;
  int count;
  t_n_sxy_channel ch[CHANNEL_MAX];
  int freeze;

  /* sdl */
  SDL_Window *win;
  SDL_GLContext glcontext;
  SDL_Event event;

  /* frame per second */
  t_float fps;

  /* clock */
  t_clock *cl;
  int cl_time;

  /* symbols */
  t_symbol *s_window;
  t_symbol *s_window_ox;
  t_symbol *s_window_oy;
  t_symbol *s_save;
  t_symbol *s_redraw;
} t_n_sxy;

//----------------------------------------------------------------------------//
// various /////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sxy_output(t_n_sxy *x, t_symbol *s, int v)
{
  t_atom a[1];
  SETFLOAT(a, (t_float)v);
  outlet_anything(x->x_out, s, 1, a);
}

//----------------------------------------------------------------------------//
// calc ////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sxy_calc_constant(t_n_sxy *x)
{
  x->bufmax = x->s_sr / x->fps; /* ? */
  x->bufmax_1 = x->bufmax - 1;
  x->count = 0;
}

//----------------------------------------------------------------------------//
void n_sxy_calc_grid_hor(t_n_sxy *x)
{
  int i;
  t_float h;
  t_float f;

  x->grid_hor_y_c = 0.0;

  h = 1.0 / ((t_float)x->grid_hor + 1.0);

  if (x->grid_hor > 0)
    {
      for (i = 0; i < x->grid_hor; i++)
	{
	  f = h * ((t_float)i + 1.0);
	  x->grid_hor_y_up[i] = 0.0 - f;
	  x->grid_hor_y_dw[i] = 0.0 + f;
	}
    }
}

//----------------------------------------------------------------------------//
void n_sxy_calc_grid_ver(t_n_sxy *x)
{
  int i;
  t_float w;

  w = 2.0 / ((t_float)x->grid_ver + 1.0);
  if (x->grid_ver > 0)
    {
      for (i = 0; i < x->grid_ver; i++)
	{
	  x->grid_ver_x[i] = (w * ((t_float)i + 1.0)) - 1.0;
	}
    }
}

//----------------------------------------------------------------------------//
// color ///////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sxy_color(t_float c, GLfloat *r, GLfloat *g, GLfloat *b)
{
  int color = c;
  Uint8 ub = color;
  Uint8 ug = color >> 8;
  Uint8 ur = color >> 16;
  *b = (float)ub / 255.;
  *g = (float)ug / 255.;
  *r = (float)ur / 255.;
}

//----------------------------------------------------------------------------//
// redraw //////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sxy_redraw(t_n_sxy *x)
{
  int i, j;

  if (x->window_on == 0 || 
      x->update == 0 ||
      x->freeze == 0) return;

  // backgroud
  glColor4f(x->c_back.r, x->c_back.g, x->c_back.b, x->c_back.a);
  glBegin(GL_POLYGON);
  glVertex2f(-1., -1.);
  glVertex2f(-1., 1.);
  glVertex2f(1., 1.);
  glVertex2f(1., -1.);
  glEnd();
  
  // grid
  if (x->grid_view)
    {
      glColor4f(x->c_grid.r, x->c_grid.g, x->c_grid.b, x->c_grid.a);
      glLineWidth(x->w_grid);

      // grid ver
      for (i=0; i<x->grid_ver; i++)
	{
	  glBegin(GL_LINES);
	  glVertex2f(x->grid_ver_x[i], 1.); // x0 y0
	  glVertex2f(x->grid_ver_x[i], -1.); // x1 y1 
	  glEnd();
	}
      // grid hor
      for (i=0; i<x->grid_hor; i++)
	{
	  glBegin(GL_LINES);
	  glVertex2f(-1.0, x->grid_hor_y_up[i]); // x0 y0
	  glVertex2f(1.0, x->grid_hor_y_up[i]); // x1 y1 
	  glEnd();

	  glBegin(GL_LINES);
	  glVertex2f(-1.0, x->grid_hor_y_dw[i]); // x0 y0
	  glVertex2f(1.0, x->grid_hor_y_dw[i]); // x1 y1 
	  glEnd();
	}

      glBegin(GL_LINES);
      glVertex2f(-1.0, x->grid_hor_y_c); // x0 y0
      glVertex2f(1.0, x->grid_hor_y_c); // x1 y1 
      glEnd();
    }
  
  // channels
  if (x->window_on)
    {
      for (j = 0; j < x->channel; j++)
        {
	  if (x->ch[j].on)
	    {
	      // points
	      glPointSize(x->ch[j].w);
	      glColor4f(x->ch[j].c.r, 
			x->ch[j].c.g, 
			x->ch[j].c.b, 
			x->ch[j].c.a);
	      glBegin(GL_POINTS);
	      for (i = 0; i < x->bufmax; i++)
		{
		  glVertex2f(x->ch[j].x[i], x->ch[j].y[i]);
		}
	      glEnd();
	    }
        }
    }
  
  glFlush();
  SDL_GL_SwapWindow(x->win);
  x->update = 0;
  n_sxy_output(x, x->s_redraw, 1);
}

//----------------------------------------------------------------------------//
void n_sxy_reshape(t_n_sxy *x)
{
  if (x->window_on == 0) return;

  glClearColor(0., 0., 0., 0.);
  glViewport(0, 0, (GLsizei)x->window_w, (GLsizei)x->window_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(-1., -1., 1., 1.);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_POINT_SMOOTH);
}

//----------------------------------------------------------------------------//
// sdl /////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sxy_sdl_window(t_n_sxy *x)
{
  // sdl
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
      post("error: n_sxy~: SDL_Init: %s",SDL_GetError()); 
    }
  x->win = SDL_CreateWindow("n_sxy~",
			    x->window_ox,
			    x->window_oy,
			    x->window_w,
			    x->window_h,
			    SDL_WINDOW_SHOWN |
			    SDL_WINDOW_OPENGL);
  if (x->win == NULL)
    {
      post("error: n_sxy~: SDL_CreateWindow: %s",SDL_GetError()); 
    }
  SDL_SetWindowMinimumSize(x->win, x->window_w, x->window_h);
  x->glcontext = SDL_GL_CreateContext(x->win);
  n_sxy_reshape(x);
  n_sxy_redraw(x);
  // clock
  clock_delay(x->cl, x->cl_time);
}

//----------------------------------------------------------------------------//
void n_sxy_sdl_window_close(t_n_sxy *x)
{
  // clock
  clock_unset(x->cl);
  // sdl
  SDL_DestroyWindow(x->win);
  SDL_Quit();
}

//----------------------------------------------------------------------------//
void n_sxy_events(t_n_sxy *x)
{
  // clock
  clock_delay(x->cl, x->cl_time);
  if (x->window_on)
    {
      while (SDL_PollEvent(&x->event))
        {
          switch (x->event.type)
            {
            case SDL_WINDOWEVENT:
              switch (x->event.window.event)
                {
                case SDL_WINDOWEVENT_MOVED:
                  x->window_ox = x->event.window.data1;
                  x->window_oy = x->event.window.data2;
		  n_sxy_output(x, x->s_window_ox, x->window_ox); 
		  n_sxy_output(x, x->s_window_oy, x->window_oy); 
                  break;
                case SDL_WINDOWEVENT_CLOSE:
		  x->window_on= 0;
                  n_sxy_sdl_window_close(x);
		  n_sxy_output(x, x->s_window, x->window_on); 
                  break;
                }
              break;
            case SDL_KEYDOWN:
              switch (x->event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
		  post("ESC");
		  x->window_on= 0;
                  n_sxy_sdl_window_close(x);
		  n_sxy_output(x, x->s_window, x->window_on); 
                  break;
                }
              break;
            }
          break;
        }
    }

  // after events
  if (x->window_on)
    {
      if (x->window_moved)
	{
	  SDL_SetWindowPosition(x->win, x->window_ox, x->window_oy);
	  x->window_moved = 0;
	}
      n_sxy_redraw(x);
    }
}

//----------------------------------------------------------------------------//
// input methods ///////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
static void n_sxy_window(t_n_sxy *x, t_floatarg f)
{
  f = (f > 0);
  if (x->window_on == f) return;
  else x->window_on = f;
  if (x->window_on)
    {
      x->window_w = x->i_window_w;
      x->window_h = x->i_window_h;
      x->update = 1;
      n_sxy_sdl_window(x);
      n_sxy_reshape(x);
      n_sxy_redraw(x);
    }
  else
    {
      n_sxy_sdl_window_close(x);
    }
}

//----------------------------------------------------------------------------//
static void n_sxy_window_ox(t_n_sxy *x, t_floatarg f)
{
  x->window_ox = f;
  x->window_moved = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_window_oy(t_n_sxy *x, t_floatarg f)
{
  x->window_oy = f;
  x->window_moved = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_window_w(t_n_sxy *x, t_floatarg f)
{
  CLIP_MINMAX(WINDOW_WIDTH_MIN, WINDOW_WIDTH_MAX, f);
  x->i_window_w = f;
}

//----------------------------------------------------------------------------//
static void n_sxy_window_h(t_n_sxy *x, t_floatarg f)
{
  CLIP_MINMAX(WINDOW_HEIGHT_MIN, WINDOW_HEIGHT_MAX, f);
  x->i_window_h = f;
}

//----------------------------------------------------------------------------//
static void n_sxy_window_fps(t_n_sxy *x, t_floatarg f)
{
  CLIP_MINMAX(WINDOW_FPS_MIN, WINDOW_FPS_MAX, f);
  x->fps = f;
  x->cl_time = (1000.0 / x->fps); /* default (in ms ?)*/
  n_sxy_calc_constant(x);
}

//----------------------------------------------------------------------------//
static void n_sxy_par_back(t_n_sxy *x,
			 t_floatarg color,
			 t_floatarg a)
{
  n_sxy_color(color, &x->c_back.r, &x->c_back.g, &x->c_back.b);
  CLIP_MINMAX(0., 1., a);
  x->c_back.a = a;
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_par_grid(t_n_sxy *x,
			 t_floatarg color)
{
  n_sxy_color(color, &x->c_grid.r, &x->c_grid.g, &x->c_grid.b);
  x->c_grid.a = 1.0; /* constant */
  x->w_grid = 1.0;
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_par_ch(t_n_sxy *x, 
			 t_floatarg ch, 
			 t_floatarg color,
			 t_floatarg a,
			 t_floatarg w)
{
  int i = ch;
  CLIP_MINMAX(0, x->channel - 1, i);
  n_sxy_color(color, &x->ch[i].c.r, &x->ch[i].c.g, &x->ch[i].c.b);
  CLIP_MINMAX(0., 1., a);
  x->ch[i].c.a = a;
  x->ch[i].w = w;
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_on(t_n_sxy *x, t_floatarg ch, t_floatarg on)
{
  int i = ch;
  CLIP_MINMAX(0, x->channel - 1, i);
  x->ch[i].on = (on > 0);
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_grid_view(t_n_sxy *x, t_floatarg f)
{
  x->grid_view = (f > 0);
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_grid_ver(t_n_sxy *x, t_floatarg f)
{
  CLIP_MINMAX(GRID_VER_MIN, GRID_VER_MAX, f);
  x->grid_ver = f;
  n_sxy_calc_grid_ver(x);
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_grid_hor(t_n_sxy *x, t_floatarg f)
{
  CLIP_MINMAX(GRID_HOR_MIN, GRID_HOR_MAX, f);
  x->grid_hor = f;
  n_sxy_calc_grid_hor(x);
  x->update = 1;
}

//----------------------------------------------------------------------------//
static void n_sxy_freeze(t_n_sxy *x, t_floatarg f)
{
  x->freeze = (f == 0);
}

//----------------------------------------------------------------------------//
// save ////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
/*     output:                   */
/*     0 - ok.                   */
/*     1 - window not open.      */
/*     2 - error open file.      */
void n_sxy_save(t_n_sxy *x, t_symbol *s)
{
  const int number_of_pixels = x->window_w * x->window_h * 3;
  unsigned char pixels[number_of_pixels];
  FILE *output_file = NULL;
  
  if (x->window_on)
    {
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadBuffer(GL_FRONT);
      glReadPixels(0, 0, 
		   x->window_w, x->window_h, 
		   GL_RGB, 
		   GL_UNSIGNED_BYTE, 
		   pixels);
    }
  else
    {
      post("error: n_sxy~: window not open");
      n_sxy_output(x, x->s_save, 1);
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
      post("n_sxy~: save to file: %s", s->s_name);
      n_sxy_output(x, x->s_save, 0);
      return;
    }
  else
    {
      post("error: n_sxy~: file can't open: %s", s->s_name);
      n_sxy_output(x, x->s_save, 2);
      return;
    }
}

//----------------------------------------------------------------------------//
// dsp /////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
t_int *n_sxy_perform(t_int *w)
{
  int i,j;
  t_n_sxy *x = (t_n_sxy *)(w[1]);
  t_sample *sig_x[CHANNEL_MAX];  /* vector in's */
  t_sample *sig_y[CHANNEL_MAX];  /* vector in's */
  for (i = 0; i < x->channel; i++)
    {
      j = i * 2;
      sig_x[i] = (t_sample *)(w[j + 2]);
      sig_y[i] = (t_sample *)(w[j + 1 + 2]);
    }
  int n = x->s_n;
  
  if (x->window_on)
    {
      // dsp
      while (n--)
        {
          for (i = 0; i < x->channel; i++)
            {
              x->ch[i].x[x->count] = *(sig_x[i]++);
              x->ch[i].y[x->count] = *(sig_y[i]++);
            }
          x->count++;
          if (x->count >= x->bufmax)
            {
              x->count = 0;
	      x->update = 1;
            }
        }
    }

  return (w + x->amount_channel + 2);
}

//----------------------------------------------------------------------------//
void n_sxy_dsp(t_n_sxy *x, t_signal **sp)
{
  int i;

  if (sp[0]->s_n != x->s_n || sp[0]->s_sr != x->s_sr)
    {
      x->s_n = sp[0]->s_n;
      x->s_sr = sp[0]->s_sr;
      n_sxy_calc_constant(x);
    }

  x->v_d[0] = (t_int *)x;
  for (i = 0; i < x->amount_channel; i++)
    {
      x->v_d[i + 1] = (t_int *)sp[i]->s_vec;
    }
  dsp_addv(n_sxy_perform, x->amount_channel + 1, (t_int *)x->v_d);
}

//----------------------------------------------------------------------------//
// setup ///////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void *n_sxy_new(t_symbol *s, int ac, t_atom *av)
{
  int i;
  t_n_sxy *x = (t_n_sxy *)pd_new(n_sxy_class);

  // arguments
  x->channel  = atom_getfloatarg(0,ac,av);
  CLIP_MINMAX(1, CHANNEL_MAX, x->channel);
  x->amount_channel = x->channel * 2;

  // create inlets
  for (i = 1; i < x->amount_channel; i++)
    {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }

  // create outlets
  x->x_out = outlet_new(&x->x_obj, 0);

  // init pd
  x->s_sr = 44100;

  // mem
  x->v_d = getbytes(sizeof(t_int *) *(x->amount_channel + 1));

  // clock
  x->cl = clock_new(x, (t_method)n_sxy_events);

  // symbols
  x->s_window     = gensym("window");
  x->s_window_ox  = gensym("window_ox");
  x->s_window_oy  = gensym("window_oy");
  x->s_save       = gensym("save");
  x->s_redraw     = gensym("redraw");

  // init window
  x->window_on = 0;
  n_sxy_window_ox(x, 100);
  n_sxy_window_oy(x, 100);
  n_sxy_window_w(x, 256);
  n_sxy_window_h(x, 256);
  n_sxy_window_fps(x, 50);

  // pars
  n_sxy_par_back(x, -2.17117e+06, 0.5);
  n_sxy_par_grid(x, -3.28965e+06);

  for (i = 0; i < x->channel; i++)
    {
      n_sxy_par_ch(x, i, -1.31075e+07, 0.5, 1.0);
      n_sxy_on(x, i, 1);
    }

  // settings
  n_sxy_grid_view(x, 1);
  n_sxy_grid_ver(x, 1);
  n_sxy_grid_hor(x, 1);
  n_sxy_freeze(x, 0);

  // init
  n_sxy_calc_constant(x);

  return (x);
  if (s) {}; // for disable error messages ...
}

//----------------------------------------------------------------------------//
void n_sxy_free(t_n_sxy *x)
{
  freebytes(x->v_d, sizeof(t_int *) * (x->amount_channel + 1));
  if (x->window_on)
    {
      x->window_on = 0;
      n_sxy_sdl_window_close(x);
    }
  clock_free(x->cl);
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
#define METHOD4(F,S) \
class_addmethod(n_sxy_class,(t_method)(F),gensym((S)),A_FLOAT,A_FLOAT, \
A_FLOAT,A_FLOAT,0);
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
  METHOD2(n_sxy_par_back,              "par_back");
  METHOD1(n_sxy_par_grid,              "par_grid");
  METHOD4(n_sxy_par_ch,                "par_channel");
  METHOD2(n_sxy_on,                    "on");
  /* settings */
  METHOD1(n_sxy_grid_view,             "grid_view");
  METHOD1(n_sxy_grid_ver,              "grid_ver");
  METHOD1(n_sxy_grid_hor,              "grid_hor");
  METHOD1(n_sxy_freeze,                "freeze");
  /* save */
  METHODS(n_sxy_save,                  "save");
}

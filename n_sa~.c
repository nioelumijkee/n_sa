/*----------------------------------------------------------------------------//
 n_sa - signal analysis tool

+-+--------------------+-+--------------------------------+-+-------------+-+
| |       SCOPE        | |            SPECTR              | |    PHASE    | |
+ +--------------------+ +--------------------------------+-+-------------+-+
| |                    | |                                | |             | |
| |                    | |                                | |             | |
| |                    | |                                | |             | |
+-+--------------------+-+--------------------------------+-+-------------+-+

//----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "m_pd.h"

//----------------------------------------------------------------------------//
#define MAX_CHANNEL 16
#define MIN_HEIGHT 100
#define MAX_HEIGHT 1024
#define MIN_WIDTH_SCOPE 1
#define MAX_WIDTH_SCOPE 1024
#define MIN_WIDTH_SPECTR 1
#define MAX_WIDTH_SPECTR 1024

#define CLIP_MINMAX(MIN, MAX, IN)		\
  if      ((IN) < (MIN)) (IN) = (MIN);          \
  else if ((IN) > (MAX)) (IN) = (MAX);

#define CLIP_MIN(MIN, IN)			\
  if ((IN) < (MIN))      (IN) = (MIN);

#define CLIP_MAX(MAX, IN)			\
  if ((IN) < (MAX))      (IN) = (MAX);


//----------------------------------------------------------------------------//
static t_class *n_sa_class;

typedef struct _n_sa_channel
{
} t_n_sa_channel;

typedef struct _n_sa
{
  t_object x_obj; /* pd */
  t_outlet *x_out;
  int s_n; /* blocksize */
  t_float s_sr; /* sample rate */
  t_int **v_d;      /* vector for dsp_addv */
  int amount_channel; /* body */
  t_n_sa_channel ch[MAX_CHANNEL];

  /* real */
  int window;
  int window_w_min;
  int window_h_min;
  int window_w;
  int window_h;
  int window_ox;
  int window_oy;
  int split_w;
  int scope_w;
  int spectr_w;
  int phase_w;
  int scope_view;
  int spectr_view;
  int phase_view;

  /* input */
  int i_window_h;
  int i_scope_w;
  int i_spectr_w;
  int i_scope_view;
  int i_spectr_view;
  int i_phase_view;

  int window_moved;

  /* colors */
  int i_color_split;
  int i_color_back;
  int i_color_grid_hi;
  int i_color_grid_lo;
  Uint32 color_split;
  Uint32 color_back;
  Uint32 color_grid_hi;
  Uint32 color_grid_lo;

  /* sdl */
  SDL_Window *win;  /* win */
  SDL_Surface *surface; /* sdl */
  int offset;
  SDL_Event event;

  /* clock */
  t_clock *cl;
  int cl_time;

  /* symbols */
  t_symbol *s_window;
  t_symbol *s_window_ox;
  t_symbol *s_window_oy;
} t_n_sa;





//----------------------------------------------------------------------------//
// various /////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sa_calc_constant(t_n_sa *x)
{
  if (x) {}
}

//----------------------------------------------------------------------------//
// output
//----------------------------------------------------------------------------//
void n_sa_output(t_n_sa *x, t_symbol *s, int v)
{
  t_atom a[1];
  SETFLOAT(a, (t_float)v);
  outlet_anything(x->x_out, s, 1, a);
}

//----------------------------------------------------------------------------//
Uint32 n_sa_color(t_n_sa *x, int color)
{
  Uint32 rgb;
  Uint8 r, g, b;
  b = color;
  g = color >> 8;
  r = color >> 16;
  rgb = SDL_MapRGB(x->surface->format, r, g, b);
  return(rgb);
}

//----------------------------------------------------------------------------//
void n_sa_calc_colors(t_n_sa *x)
{
  x->color_split = n_sa_color(x, x->i_color_split);
  x->color_back = n_sa_color(x, x->i_color_back);
  x->color_grid_hi = n_sa_color(x, x->i_color_grid_hi);
  x->color_grid_lo = n_sa_color(x, x->i_color_grid_lo);
}

//----------------------------------------------------------------------------//
// display /////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sa_calc_size_window(t_n_sa *x)
{
  x->window_h = x->i_window_h;
  x->scope_w = x->i_scope_w;
  x->spectr_w = x->i_spectr_w;
  x->scope_view = x->i_scope_view;
  x->spectr_view = x->i_spectr_view;
  x->phase_view = x->i_phase_view;

  x->phase_w = x->window_h;

  x->window_w = 
    ((x->scope_w + x->split_w) * x->scope_view) +   // spectr
    ((x->spectr_w + x->split_w) * x->spectr_view) +   // spectr
    ((x->window_h + x->split_w) * x->phase_view);    // phase
}

//----------------------------------------------------------------------------//
void n_sa_reshape(t_n_sa *x)
{
  post("RESHAPE");
  if (x) {};
}

//----------------------------------------------------------------------------//
void n_sa_redraw(t_n_sa *x)
{
  post("REDRAW");
  if (x) {};
}

//----------------------------------------------------------------------------//
void n_sa_sdl_window(t_n_sa *x)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
      post("error: n_sa~: SDL_Init: %s",SDL_GetError()); 
    }
  x->win = SDL_CreateWindow("n_sa~",
			    x->window_ox,
			    x->window_oy,
			    x->window_w,
			    x->window_h,
			    SDL_WINDOW_SHOWN);
  if (x->win == NULL)
    {
      post("error: n_sa~: SDL_CreateWindow: %s",SDL_GetError()); 
    }
  SDL_SetWindowMinimumSize(x->win, x->window_w_min, x->window_h_min);
  // clock
  clock_delay(x->cl, x->cl_time);
}

//----------------------------------------------------------------------------//
void n_sa_sdl_window_close(t_n_sa *x)
{
  // clock
  clock_unset(x->cl);
  // sdl
  SDL_Quit();
}

//----------------------------------------------------------------------------//
void n_sa_sdl_get_surface(t_n_sa *x)
{
  if (x->window)
    {
      x->surface = SDL_GetWindowSurface(x->win);
      if (x->surface == NULL)
	{
	  post("error: n_sa~: SDL_GetWindowSurface: %s",SDL_GetError()); 
	}
      x->offset = x->surface->pitch / 4;
    }
}

//----------------------------------------------------------------------------//
void n_sa_events(t_n_sa *x)
{
  // clock
  clock_delay(x->cl, x->cl_time);
  // events
  if (x->window)
    {
      while (SDL_PollEvent(&x->event))
        {
          switch (x->event.type)
            {
            case SDL_WINDOWEVENT:
              switch (x->event.window.event)
                {
                case SDL_WINDOWEVENT_MOVED:
		  post("moved");
                  x->window_ox = x->event.window.data1;
                  x->window_oy = x->event.window.data2;
		  n_sa_output(x, x->s_window_ox, x->window_ox); 
		  n_sa_output(x, x->s_window_oy, x->window_oy); 
                  break;
                case SDL_WINDOWEVENT_CLOSE:
		  post("close");
		  x->window = 0;
                  n_sa_sdl_window_close(x);
		  n_sa_output(x, x->s_window, x->window); 
                  break;
                }
              break;
            case SDL_KEYDOWN:
              switch (x->event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                  post("Escape");
		  x->window = 0;
                  n_sa_sdl_window_close(x);
		  n_sa_output(x, x->s_window, x->window); 
                  break;
                }
              break;
            }
          break;
        }
    }

  // after events
  if (x->window)
    {
      if (x->window_moved)
	{
	  SDL_SetWindowPosition(x->win, x->window_ox, x->window_oy);
	  x->window_moved = 0;
	}
    }
}


//----------------------------------------------------------------------------//
// input methods ///////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
static void n_sa_window(t_n_sa *x, t_floatarg f)
{
  x->window = f;
  if (x->window)
    {
      n_sa_calc_size_window(x);
      n_sa_sdl_window(x);
      n_sa_sdl_get_surface(x);
      n_sa_calc_colors(x);
      n_sa_reshape(x);
      n_sa_redraw(x);
    }
  else
    {
      n_sa_sdl_window_close(x);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_window_ox(t_n_sa *x, t_floatarg f)
{
  x->window_ox = f;
  x->window_moved = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_window_oy(t_n_sa *x, t_floatarg f)
{
  x->window_oy = f;
  x->window_moved = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_window_h(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_HEIGHT, MAX_HEIGHT, f);
  x->i_window_h = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_w(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_WIDTH_SCOPE, MAX_WIDTH_SCOPE, f);
  x->i_scope_w = f;
}

//----------------------------------------------------------------------------//
static void n_sa_spectr_w(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_WIDTH_SPECTR, MAX_WIDTH_SPECTR, f);
  x->i_spectr_w = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_view(t_n_sa *x, t_floatarg f)
{
  x->i_scope_view = f;
}

//----------------------------------------------------------------------------//
static void n_sa_spectr_view(t_n_sa *x, t_floatarg f)
{
  x->i_spectr_view = f;
}

//----------------------------------------------------------------------------//
static void n_sa_phase_view(t_n_sa *x, t_floatarg f)
{
  x->i_phase_view = f;
}

//----------------------------------------------------------------------------//
static void n_sa_color_split(t_n_sa *x, t_floatarg f)
{
  x->i_color_split = f * -1;
  if (x->window)
    {
      x->color_split = n_sa_color(x, x->i_color_split);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_back(t_n_sa *x, t_floatarg f)
{
  x->i_color_back = f * -1;
  if (x->window)
    {
      x->color_back = n_sa_color(x, x->i_color_back);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_grid_hi(t_n_sa *x, t_floatarg f)
{
  x->i_color_grid_hi = f * -1;
  if (x->window)
    {
      x->color_grid_hi = n_sa_color(x, x->i_color_grid_hi);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_grid_lo(t_n_sa *x, t_floatarg f)
{
  x->i_color_grid_lo = f * -1;
  if (x->window)
    {
      x->color_grid_lo = n_sa_color(x, x->i_color_grid_lo);
    }
}

//----------------------------------------------------------------------------//
// dsp /////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
t_int *n_sa_perform(t_int *w)
{
  int i;
  t_n_sa *x = (t_n_sa *)(w[1]);
  t_sample *sig[MAX_CHANNEL];  /* vector in's */
  for (i = 0; i < x->amount_channel; i++)
    {
      sig[i] = (t_sample *)(w[i + 2]);
    }
  int n = x->s_n;
  float f;

  // dsp
  while (n--)
    {
      for (i=0; i < x->amount_channel; i++)
	{
	  f += *(sig[i]++);
	}
    }
  
  return (w + x->amount_channel + 2);
}

//----------------------------------------------------------------------------//
static void n_sa_dsp(t_n_sa *x, t_signal **sp)
{
  int i;

  if (sp[0]->s_n != x->s_n || sp[0]->s_sr != x->s_sr)
    {
      x->s_n = sp[0]->s_n;
      x->s_sr = sp[0]->s_sr;
      n_sa_calc_constant(x);
    }

  x->v_d[0] = (t_int *)x;
  for (i = 0; i < x->amount_channel; i++)
    {
      x->v_d[i + 1] = (t_int *)sp[i]->s_vec;
    }
  dsp_addv(n_sa_perform, x->amount_channel + 1, (t_int *)x->v_d);
}

//----------------------------------------------------------------------------//
// setup ///////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
static void *n_sa_new(t_symbol *s, int ac, t_atom *av)
{
  int i;
  t_n_sa *x = (t_n_sa *)pd_new(n_sa_class);

  // arguments
  x->amount_channel  = atom_getfloatarg(0,ac,av);
  CLIP_MINMAX(1, MAX_CHANNEL, x->amount_channel);
  
  // create inlets
  for (i = 0; i < x->amount_channel - 1; i++)
    {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }

  // create outlets
  x->x_out = outlet_new(&x->x_obj, 0);
  
  // init
  x->s_n = 64;
  x->s_sr = 44100;
  x->window = 0;
  x->window_w_min = 0;
  x->window_h_min = 100; /* for text */
  x->window_ox = 20;
  x->window_oy = 20;
  x->i_window_h = 200;
  x->split_w = 4;
  x->i_scope_w = 200;
  x->i_spectr_w = 200;
  x->i_scope_view = 0;
  x->i_spectr_view = 0;
  x->i_phase_view = 0;

  // init
  n_sa_calc_constant(x);

  // mem
  x->v_d = getbytes(sizeof(t_int *) *(x->amount_channel + 1));

  // clock
  x->cl_time = 2; /* default (in ms ?)*/
  x->cl = clock_new(x, (t_method)n_sa_events);

  // symbols
  x->s_window       = gensym("window");
  x->s_window_ox    = gensym("window_ox");
  x->s_window_oy    = gensym("window_oy");

  return (x);
  if (s) {};
}

//----------------------------------------------------------------------------//
static void n_sa_free(t_n_sa *x)
{
  freebytes(x->v_d, sizeof(t_int *) * (x->amount_channel + 1));
}

//----------------------------------------------------------------------------//
void n_sa_tilde_setup(void)
{
  n_sa_class = class_new(gensym("n_sa~"), (t_newmethod)n_sa_new, 
			 (t_method)n_sa_free,
			 sizeof(t_n_sa), 0, A_GIMME, 0);
  class_addmethod(n_sa_class,nullfn, gensym("signal"), 0);
  class_addmethod(n_sa_class,(t_method)n_sa_dsp, gensym("dsp"), 0);
  class_addmethod(n_sa_class,(t_method)n_sa_window, gensym("window"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_window_ox, gensym("window_ox"), A_DEFFLOAT, 0); 
  class_addmethod(n_sa_class,(t_method)n_sa_window_oy, gensym("window_oy"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_window_h, gensym("window_h"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_w, gensym("scope_w"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_spectr_w, gensym("spectr_w"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_view, gensym("scope_view"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_spectr_view, gensym("spectr_view"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_phase_view, gensym("phase_view"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_split, gensym("color_split"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_back, gensym("color_back"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_grid_hi, gensym("color_grid_hi"), A_DEFFLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_grid_lo, gensym("color_grid_lo"), A_DEFFLOAT, 0);
}

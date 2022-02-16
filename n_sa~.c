/* n_sa - signal analysis tool

   Structure this file:
   - init
   - display
   - scope
   - spectr
   - input methods
   - dsp
   - setup

*/
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "m_pd.h"

#define MAX_CHANNEL 16
#define MIN_HEIGHT 100
#define MAX_HEIGHT 1024
#define MIN_WIDTH_SCOPE 1
#define MAX_WIDTH_SCOPE 1024
#define MIN_WIDTH_SPECTR 1
#define MAX_WIDTH_SPECTR 1024
#define WIDTH_SPLIT 4

#define CLIP_MINMAX(MIN, MAX, IN)		\
  if      ((IN) < (MIN)) (IN) = (MIN);          \
  else if ((IN) > (MAX)) (IN) = (MAX);

#define CLIP_MIN(MIN, IN)			\
  if ((IN) < (MIN))      (IN) = (MIN);

#define CLIP_MAX(MAX, IN)			\
  if ((IN) < (MAX))      (IN) = (MAX);


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
  int window; /* window open/close */

  /* real */
  int window_ox;
  int window_oy;
  int window_w;
  int window_h;
  int scope_w;
  int spectr_w;
  /* input */
  int i_window_ox;
  int i_window_oy;
  int i_window_w;
  int i_window_h;
  int i_scope_w;
  int i_spectr_w;

  int window_moved;
  int window_size_changed;


  Uint32 color_back; /* colors */
  Uint32 color_grid;
  Uint32 color_grid_low;
  SDL_Window *win;  /* win */
  SDL_Surface *surface; /* sdl */
  int offset;
  SDL_Event event;
  t_clock *cl;  /* clock */
  int cl_time;
  t_symbol *s_window;
  t_symbol *s_window_ox;
  t_symbol *s_window_oy;
  t_symbol *s_window_w;
  t_symbol *s_window_h;
  t_symbol *s_scope_w;
  t_symbol *s_spectr_w;
} t_n_sa;





//----------------------------------------------------------------------------//
// init
//----------------------------------------------------------------------------//
void n_sa_init(t_n_sa *x)
{
  x->s_n = 64;
  x->s_sr = 44100;
  x->window = 0;
  x->i_window_h = 200;
  x->i_scope_w = 200;
  x->i_spectr_w = 200;
}

//----------------------------------------------------------------------------//
void n_sa_calc_constant(t_n_sa *x)
{
  if (x) {}
}

//----------------------------------------------------------------------------//
// output
//----------------------------------------------------------------------------//
void n_sa_output_v1(t_n_sa *x, t_symbol *s, int v)
{
  t_atom a[1];
  SETFLOAT(a, (t_float)v);
  outlet_anything(x->x_out, s, 1, a);
}

//----------------------------------------------------------------------------//
void n_sa_output_v2(t_n_sa *x, t_symbol *s, int v1, int v2)
{
  t_atom a[2];
  SETFLOAT(a, (t_float)v1);
  SETFLOAT(a+1, (t_float)v2);
  outlet_anything(x->x_out, s, 2, a);
}

//----------------------------------------------------------------------------//
// display
//----------------------------------------------------------------------------//
void n_sa_sdl_init_window(t_n_sa *x)
{
  x->window_ox = x->i_window_ox;
  x->window_oy = x->i_window_oy;
  x->window_h = x->i_window_h;
  x->window_w = x->i_scope_w + WIDTH_SPLIT + x->i_spectr_w;
}

//----------------------------------------------------------------------------//
void n_sa_calc_size_window(t_n_sa *x)
{
  x->window_h = x->i_window_h;
  x->window_w = x->i_scope_w + WIDTH_SPLIT + x->i_spectr_w;
}


//----------------------------------------------------------------------------//
void n_sa_reshape(t_n_sa *x)
{
  post("RESHAPE");
}

//----------------------------------------------------------------------------//
void n_sa_redraw(t_n_sa *x)
{
  post("REDRAW");
}

//----------------------------------------------------------------------------//
void n_sa_sdl_window(t_n_sa *x)
{
  if (x->window)
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
				SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
      if (x->win == NULL)
	{
	  post("error: n_sa~: SDL_CreateWindow: %s",SDL_GetError()); 
	}
      clock_delay(x->cl, x->cl_time);
    }
  else
    {
      clock_unset(x->cl);
      SDL_Quit();
    }
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
  clock_delay(x->cl, x->cl_time);
  if (x->window)
    {
      while (SDL_PollEvent(&x->event))
        {
          switch (x->event.type)
            {
            case SDL_WINDOWEVENT:
              switch (x->event.window.event)
                {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
		  post("size_changed");
                  x->window_w = x->event.window.data1;
                  x->window_h = x->event.window.data2;
                  n_sa_reshape(x);
                  n_sa_redraw(x);
                  break;
                case SDL_WINDOWEVENT_RESIZED:
		  post("resized");
		  n_sa_output_v1(x, x->s_window_w, x->window_w); 
		  n_sa_output_v1(x, x->s_window_h, x->window_h); 
		  n_sa_output_v1(x, x->s_scope_w, x->scope_w); 
		  n_sa_output_v1(x, x->s_spectr_w, x->spectr_w); 
                  break;
                case SDL_WINDOWEVENT_CLOSE:
		  post("close");
		  x->window = 0;
                  n_sa_sdl_window(x);
		  n_sa_output_v1(x, x->s_window, x->window); 
                  break;
                case SDL_WINDOWEVENT_MOVED:
		  post("moved");
                  x->window_ox = x->event.window.data1;
                  x->window_oy = x->event.window.data2;
                  x->i_window_ox = x->event.window.data1;
                  x->i_window_oy = x->event.window.data2;
		  n_sa_output_v1(x, x->s_window_ox, x->window_ox); 
		  n_sa_output_v1(x, x->s_window_oy, x->window_oy); 
                  break;
                }
              break;
            case SDL_KEYDOWN:
              switch (x->event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                  post("Escape");
                  break;
                }
              break;
            }
          break;
        }
    }

  if (x->window && x->window_moved)
    {
      SDL_SetWindowPosition(x->win, x->i_window_ox, x->i_window_oy);
      x->window_moved = 0;
    }


  if (x->window && x->window_size_changed)
    {
      /* x->gi.display_mode[0].w = x->window_w; */
      /* x->gi.display_mode[0].h = x->window_h; */
      /* SDL_SetWindowDisplayMode(x->win, &x->gi.display_mode[0]); */
      n_sa_calc_size_window(x);
      SDL_SetWindowSize(x->win, x->window_w, x->window_h);
      n_sa_reshape(x);
      n_sa_redraw(x);
      /* n_sa_output_v1(x, x->s_window_w, x->window_w); */
      /* n_sa_output_v1(x, x->s_window_h, x->window_h); */
      /* n_sa_output_v1(x, x->s_scope_w, x->scope_w); */
      /* n_sa_output_v1(x, x->s_spectr_w, x->spectr_w); */
      x->window_size_changed = 0;
    }
}


//----------------------------------------------------------------------------//
// input methods
//----------------------------------------------------------------------------//
static void n_sa_window(t_n_sa *x, t_floatarg f)
{
  if (x->window != f)
    {
      x->window = f;
      n_sa_sdl_init_window(x);
      n_sa_sdl_window(x);
      n_sa_sdl_get_surface(x);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_window_ox(t_n_sa *x, t_floatarg f)
{
  x->i_window_ox = f;
  x->window_moved = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_window_oy(t_n_sa *x, t_floatarg f)
{
  x->i_window_oy = f;
  x->window_moved = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_window_h(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_HEIGHT, MAX_HEIGHT, f);
  x->i_window_h = f;
  x->window_size_changed = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_w(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_WIDTH_SCOPE, MAX_WIDTH_SCOPE, f);
  x->i_scope_w = f;
  x->window_size_changed = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_spectr_w(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_WIDTH_SPECTR, MAX_WIDTH_SPECTR, f);
  x->i_spectr_w = f;
  x->window_size_changed = 1;
}

//----------------------------------------------------------------------------//
// dsp
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

  // dsp
  while (n--)
    {
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
// setup
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
  n_sa_init(x);
  n_sa_calc_constant(x);

  // mem
  x->v_d = getbytes(sizeof(t_int *) *(x->amount_channel + 1));

  // clock
  x->cl_time = 2; /* default (in ms ?)*/
  x->cl = clock_new(x, (t_method)n_sa_events);

  // symbols
  x->s_window = gensym("window");
  x->s_window_ox = gensym("window_ox");
  x->s_window_oy = gensym("window_oy");
  x->s_window_w  = gensym("window_w");
  x->s_window_h  = gensym("window_h");
  x->s_scope_w   = gensym("scope_w");
  x->s_spectr_w  = gensym("spectr_w");

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
}

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
#define MIN_SCOPE_GRID_VER 0
#define MAX_SCOPE_GRID_VER 31
#define MIN_SCOPE_GRID_HOR 0
#define MAX_SCOPE_GRID_HOR 31
#define C_2PI 6.2831853
#define SYNC_OFF 0
#define SYNC_UP 1
#define SYNC_DOWN 2

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
  int on;
  t_float amp;
  int i_color;
  Uint32 color;
} t_n_sa_channel;

typedef struct _n_sa
{
  t_object x_obj; /* pd */
  t_outlet *x_out;
  int s_n; /* blocksize */
  t_float s_sr; /* sample rate */
  t_int **v_d;      /* vector for dsp_addv */

  /* channels */
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
  int scope_w_full;
  int spectr_w;
  int spectr_w_full;
  int phase_w;
  int phase_w_full;
  int scope_view;
  int spectr_view;
  int phase_view;

  /* scope */
  int scope_grid_view;
  int scope_grid_ver;
  int scope_grid_hor;
  int scope_sep_view;
  int scope_recpos_view;
  int scope_separate;
  int scope_sync;
  int scope_sync_channel;
  t_float scope_sync_treshold;
  int scope_sync_dc;
  t_float scope_sync_dc_freq;
  int scope_spp;


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
  int i_color_sep;
  int i_color_recpos;
  Uint32 color_split;
  Uint32 color_back;
  Uint32 color_grid_hi;
  Uint32 color_grid_lo;
  Uint32 color_sep;
  Uint32 color_recpos;

  /* scope */
  t_float scope_sync_dc_f;


  /* sdl */
  SDL_Window *win;  /* win */
  SDL_Surface *surface; /* sdl */
  int ofs;
  SDL_Event event;
  Uint32 *pix;

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
void n_sa_output(t_n_sa *x, t_symbol *s, int v)
{
  t_atom a[1];
  SETFLOAT(a, (t_float)v);
  outlet_anything(x->x_out, s, 1, a);
}

//----------------------------------------------------------------------------//
void n_sa_calc_constant(t_n_sa *x)
{
  x->scope_sync_dc_f = x->scope_sync_dc_freq * (C_2PI / (t_float)x->s_sr);
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
  int i;
  x->color_split = n_sa_color(x, x->i_color_split);
  x->color_back = n_sa_color(x, x->i_color_back);
  x->color_grid_hi = n_sa_color(x, x->i_color_grid_hi);
  x->color_grid_lo = n_sa_color(x, x->i_color_grid_lo);
  x->color_sep = n_sa_color(x, x->i_color_sep);
  x->color_recpos = n_sa_color(x, x->i_color_recpos);
  for (i=0; i<x->amount_channel; i++)
    {
      x->ch[i].color = n_sa_color(x, x->ch[i].i_color);
    }
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

  x->scope_w_full = x->scope_w + x->split_w + x->split_w;
  x->spectr_w_full = x->spectr_w + x->split_w + x->split_w;
  x->phase_w_full = x->phase_w + x->split_w + x->split_w;


  x->window_w = 
    (x->scope_w_full  * x->scope_view)   +  // spectr
    (x->spectr_w_full * x->spectr_view)  +  // spectr
    (x->phase_w_full  * x->phase_view);     // phase
}

//----------------------------------------------------------------------------//
void draw_line_hor(Uint32 *pix, int ofs, int x0, int y0, int w, Uint32 col)
{
  int x;
  Uint32 *bufp;
  Uint32 *p;
  p = pix + (y0 * ofs);
  for (x=0; x<w; x++)
    {
      bufp = p + x0 + x;
      *bufp = col;
    }
}

//----------------------------------------------------------------------------//
void draw_line_ver(Uint32 *pix, int ofs, int x0, int y0, int h, Uint32 col)
{
  int y;
  Uint32 *bufp;
  Uint32 *p;
  for (y=0; y<h; y++)
    {
      p = pix + ((y0 + y) * ofs);
      bufp = p + x0;
      *bufp = col;
    }
}

//----------------------------------------------------------------------------//
void draw_rect(Uint32 *pix, int ofs, int x0, int y0, int w, int h, Uint32 col)
{
  int x,y;
  Uint32 *bufp;
  Uint32 *p;
  for (y=0; y<h; y++)
    {
      p = pix + ((y0 + y) * ofs);
      for (x=0; x<w; x++)
	{
	  bufp = p + x0 + x;
	  *bufp = col;
	}
    }
}

//----------------------------------------------------------------------------//
void n_sa_redraw(t_n_sa *z)
{
  post("REDRAW");

  int i,j,c,k;


  // scope
  if (z->scope_view)
    {
      i = z->window_h - z->split_w;
      j = z->window_h - z->split_w - z->split_w;
      c = z->scope_w_full - z->split_w;
      k = z->window_h - z->split_w - z->split_w;
      // split
      draw_rect(z->pix, z->ofs, 0, 0, z->scope_w_full, z->split_w, z->color_split);
      draw_rect(z->pix, z->ofs, 0, i, z->scope_w_full, z->split_w, z->color_split);
      draw_rect(z->pix, z->ofs, 0, z->split_w, z->split_w, j, z->color_split);
      draw_rect(z->pix, z->ofs, c, z->split_w, z->split_w, j, z->color_split);
      // back
      draw_rect(z->pix, z->ofs, z->split_w, z->split_w, z->scope_w, k, z->color_back);
      // grid
      draw_line_hor(z->pix, z->ofs, 50, 60, 20, z->color_grid_hi);
      draw_line_ver(z->pix, z->ofs, 20, 30, 44, z->color_grid_lo);
      // separator
      // text
      // wave
    }
  // spectr
  if (z->spectr_view)
    {
    }
  // phase
  if (z->phase_view)
    {
    }

  // sdl
  SDL_UpdateWindowSurface(z->win);
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
      x->ofs = x->surface->pitch / 4;
      x->pix = (Uint32 *)x->surface->pixels;
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
  x->window = (f > 0);
  if (x->window)
    {
      n_sa_calc_size_window(x);
      n_sa_sdl_window(x);
      n_sa_sdl_get_surface(x);
      n_sa_calc_colors(x);
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
  x->i_scope_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_spectr_view(t_n_sa *x, t_floatarg f)
{
  x->i_spectr_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_phase_view(t_n_sa *x, t_floatarg f)
{
  x->i_phase_view = (f > 0);
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
static void n_sa_color_sep(t_n_sa *x, t_floatarg f)
{
  x->i_color_sep = f * -1;
  if (x->window)
    {
      x->color_sep = n_sa_color(x, x->i_color_sep);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_recpos(t_n_sa *x, t_floatarg f)
{
  x->i_color_recpos = f * -1;
  if (x->window)
    {
      x->color_recpos = n_sa_color(x, x->i_color_recpos);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_ch(t_n_sa *x, t_floatarg ch, t_floatarg f)
{
  int i = ch;
  CLIP_MINMAX(0, x->amount_channel - 1, i);
  x->ch[i].i_color = f * -1;
  if (x->window)
    {
      x->ch[i].color = n_sa_color(x, x->ch[i].i_color);
    }
}

//----------------------------------------------------------------------------//
static void n_sa_scope_grid_view(t_n_sa *x, t_floatarg f)
{
  x->scope_grid_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_grid_ver(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_SCOPE_GRID_VER, MAX_SCOPE_GRID_VER, f);
  x->scope_grid_ver = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_grid_hor(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(MIN_SCOPE_GRID_HOR, MAX_SCOPE_GRID_HOR, f);
  x->scope_grid_hor = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sep_view(t_n_sa *x, t_floatarg f)
{
  x->scope_sep_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_recpos_view(t_n_sa *x, t_floatarg f)
{
  x->scope_recpos_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_separate(t_n_sa *x, t_floatarg f)
{
  x->scope_separate = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync(t_n_sa *x, t_floatarg f)
{
  if      (f <= 0)
    x->scope_sync = SYNC_OFF;
  else if (f == 1)
    x->scope_sync = SYNC_UP;
  else
    x->scope_sync = SYNC_DOWN;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_channel(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(0, x->amount_channel - 1, f);
  x->scope_sync_channel = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_treshold(t_n_sa *x, t_floatarg f)
{
  x->scope_sync_treshold = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_dc(t_n_sa *x, t_floatarg f)
{
  x->scope_sync_dc = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_dc_freq(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(1.0, 1000.0, f);
  x->scope_sync_dc_freq = f;
  x->scope_sync_dc_f = x->scope_sync_dc_freq * (C_2PI / (t_float)x->s_sr);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_spp(t_n_sa *x, t_floatarg f)
{
  CLIP_MIN(1, f);
  x->scope_spp = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_on(t_n_sa *x, t_floatarg ch, t_floatarg f)
{
  int i = ch;
  CLIP_MINMAX(0, x->amount_channel - 1, i);
  x->ch[i].on = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_amp(t_n_sa *x, t_floatarg ch, t_floatarg f)
{
  int i = ch;
  CLIP_MINMAX(0, x->amount_channel - 1, i);
  x->ch[i].amp = f;
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
  x->i_color_split = 0;
  x->i_color_back = 0;
  x->i_color_grid_hi = 0;
  x->i_color_grid_lo = 0;
  x->i_color_sep = 0;
  x->i_color_recpos = 0;

  x->scope_grid_view = 0;
  x->scope_grid_ver = 0;
  x->scope_grid_hor = 0;
  x->scope_sep_view = 0;
  x->scope_recpos_view = 0;
  x->scope_separate = 0;
  x->scope_sync = 0;
  x->scope_sync_channel = 0;
  x->scope_sync_treshold = 0.0;
  x->scope_sync_dc = 0;
  x->scope_sync_dc_freq = 1.0;
  x->scope_spp = 1;

  for (i = 0; i < x->amount_channel; i++)
    {
      x->ch[i].i_color = 0;
      x->ch[i].on = 0;
      x->ch[i].amp = 0.0;
    }


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
  n_sa_class=class_new(gensym("n_sa~"),(t_newmethod)n_sa_new,(t_method)n_sa_free,sizeof(t_n_sa),0,A_GIMME,0);
  class_addmethod(n_sa_class,nullfn, gensym("signal"), 0);
  class_addmethod(n_sa_class,(t_method)n_sa_dsp, gensym("dsp"), 0);
  class_addmethod(n_sa_class,(t_method)n_sa_window, gensym("window"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_window_ox, gensym("window_ox"), A_FLOAT, 0); 
  class_addmethod(n_sa_class,(t_method)n_sa_window_oy, gensym("window_oy"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_window_h, gensym("window_h"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_w, gensym("scope_w"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_spectr_w, gensym("spectr_w"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_view, gensym("scope_view"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_spectr_view, gensym("spectr_view"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_phase_view, gensym("phase_view"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_split, gensym("color_split"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_back, gensym("color_back"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_grid_hi, gensym("color_grid_hi"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_grid_lo, gensym("color_grid_lo"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_sep, gensym("color_sep"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_recpos, gensym("color_recpos"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_color_ch, gensym("color_ch"), A_FLOAT, A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_grid_view, gensym("scope_grid_view"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_grid_ver, gensym("scope_grid_ver"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_grid_hor, gensym("scope_grid_hor"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_sep_view, gensym("scope_sep_view"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_recpos_view, gensym("scope_recpos_view"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_separate, gensym("scope_separate"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_sync, gensym("scope_sync"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_sync_channel, gensym("scope_sync_channel"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_sync_treshold, gensym("scope_sync_treshold"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_sync_dc, gensym("scope_sync_dc"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_sync_dc_freq, gensym("scope_sync_dc_freq"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_spp, gensym("scope_spp"), A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_on, gensym("scope_on"), A_FLOAT, A_FLOAT, 0);
  class_addmethod(n_sa_class,(t_method)n_sa_scope_amp, gensym("scope_amp"), A_FLOAT, A_FLOAT, 0);
}

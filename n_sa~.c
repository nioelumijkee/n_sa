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
#define C_PI  3.1415927
#define C_2PI 6.2831853
#define CHANNEL_MAX 16
#define WINDOW_HEIGHT_MIN 100
#define WINDOW_HEIGHT_MAX 4096
#define WINDOW_FPS_MIN 1.0
#define WINDOW_FPS_MAX 256.0
#define SCOPE_WIDTH_MIN 1
#define SCOPE_WIDTH_MAX 4096
#define SCOPE_GRID_VER_MIN 0
#define SCOPE_GRID_VER_MAX 31
#define SCOPE_GRID_HOR_MIN 0
#define SCOPE_GRID_HOR_MAX 31
#define SCOPE_SYNC_OFF 0
#define SCOPE_SYNC_UP 1
#define SCOPE_SYNC_DOWN 2
#define SCOPE_MAX_V  9999999.9
#define SCOPE_MIN_V -9999999.9
#define SPECTR_WIDTH_MIN 1
#define SPECTR_WIDTH_MAX 4096

//----------------------------------------------------------------------------//
#define CLIP_MINMAX(MIN, MAX, IN)		\
  if      ((IN) < (MIN)) (IN) = (MIN);          \
  else if ((IN) > (MAX)) (IN) = (MAX);

#define CLIP_MIN(MIN, IN)			\
  if ((IN) < (MIN))      (IN) = (MIN);

#define CLIP_MAX(MAX, IN)			\
  if ((IN) < (MAX))      (IN) = (MAX);

//----------------------------------------------------------------------------//
static t_class *n_sa_class;

//----------------------------------------------------------------------------//
typedef struct _n_sa_ch_colors
{
  Uint32   color;
  int      i_color;
} t_n_sa_ch_colors;

//----------------------------------------------------------------------------//
typedef struct _n_sa_ch_scope
{
  int      on;
  t_float  amp;
  int      c; /* center */
  int      sep_x0;
  int      sep_y0;
  int      sep_w;
  int      sep_lo;
  t_float  max[SCOPE_WIDTH_MAX];
  t_float  min[SCOPE_WIDTH_MAX];
  t_float  max_z;
  t_float  min_z;
} t_n_sa_ch_scope;

//----------------------------------------------------------------------------//
typedef struct _n_sa_ch_spectr
{
  int      on;
} t_n_sa_ch_spectr;

//----------------------------------------------------------------------------//
typedef struct _n_sa_ch_phase
{
  int      on;
} t_n_sa_ch_phase;

//----------------------------------------------------------------------------//
typedef struct _n_sa_scope
{
  int      view;
  int      w;
  int      h;
  int      i_view;
  int      i_w;

  int      ox;
  int      oy;
  int      maxy;
  int      split_x0;
  int      split_y0;
  int      split_w;
  int      split_h;

  int      grid_view;
  int      grid_ver;
  int      grid_hor;

  int      grid_hor_x0;
  int      grid_hor_y_c;
  int      grid_hor_y_up[SCOPE_GRID_HOR_MAX];
  int      grid_hor_y_dw[SCOPE_GRID_HOR_MAX];

  int      grid_ver_x[SCOPE_GRID_VER_MAX];
  int      grid_ver_y0;

  int      sep_view;
  int      separate;

  int      recpos_view;

  int      sync;
  int      sync_channel;
  t_float  sync_treshold;
  int      sync_dc;
  t_float  sync_dc_freq;
  int      spp;
  int      freeze;

  t_float  sync_dc_f;
  t_float  sync_dc_z;

  t_n_sa_ch_scope ch[CHANNEL_MAX];

  t_float  sync_z;
  int      find;
  int      disp_count;
  int      spp_count;

  int      h_one;
  int      h_half;

  int      spf; /* samples per frame */

  int      update;
} t_n_sa_scope;

//----------------------------------------------------------------------------//
typedef struct _n_sa_spectr
{
  int      view;
  int      w;
  int      h;
  int      i_view;
  int      i_w;

  int      ox;
  int      oy;
  int      maxy;
  int      split_x0;
  int      split_y0;
  int      split_w;
  int      split_h;
} t_n_sa_spectr;

//----------------------------------------------------------------------------//
typedef struct _n_sa_phase
{
  int      view;
  int      w;
  int      h;
  int      i_view;

  int      ox;
  int      oy;
  int      maxy;
  int      split_x0;
  int      split_y0;
  int      split_w;
  int      split_h;
} t_n_sa_phase;


//----------------------------------------------------------------------------//
typedef struct _n_sa
{
  t_object x_obj;  /* pd */
  t_outlet *x_out; /* outlet */
  int      s_n;    /* blocksize */
  t_float  s_sr;   /* sample rate */
  t_int    **v_d;  /* vector for dsp_addv */

  /* channels */
  int      amount_channel;

  /* window */
  int      window;
  int      window_w_min;
  int      window_h_min;
  int      window_w;
  int      window_h;
  int      window_ox;
  int      window_oy;
  int      split_w;
  int      i_window_h;

  int      split_lf_x0;
  int      split_lf_y0;
  int      split_lf_w;
  int      split_lf_h;

  int      split_up_x0;
  int      split_up_y0;
  int      split_up_w;
  int      split_up_h;

  int      split_dw_x0;
  int      split_dw_y0;
  int      split_dw_w;
  int      split_dw_h;

  int      window_moved;

  /* colors */
  int      i_color_split;
  int      i_color_back;
  int      i_color_grid;
  int      i_color_sep;
  int      i_color_recpos;
  Uint32   color_split;
  Uint32   color_back;
  Uint32   color_grid;
  Uint32   color_sep;
  Uint32   color_recpos;
  t_n_sa_ch_colors ch_colors[CHANNEL_MAX];

  /* scope */
  t_n_sa_scope sc;

  /* spectr */
  t_n_sa_spectr spectr;

  /* phase */
  t_n_sa_phase phase;

  /* sdl */
  SDL_Window *win;  /* win */
  SDL_Surface *surface; /* sdl */
  int ofs;
  SDL_Event event;
  Uint32 *pix;

  /* frame per second */
  t_float fps;

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
// calc ////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sa_calc_constant(t_n_sa *x)
{
  x->sc.sync_dc_f = x->sc.sync_dc_freq * (C_2PI / (t_float)x->s_sr);
  x->sc.sync_dc_z = 0.0;
  x->cl_time = (1000.0 / x->fps); /* default (in ms ?)*/
  x->sc.spf = x->sc.spp * x->sc.w;
}

//----------------------------------------------------------------------------//
void n_sa_calc_size_window(t_n_sa *x)
{
  x->window_h    = x->i_window_h + x->split_w + x->split_w;

  x->sc.view  = x->sc.i_view;
  x->sc.w     = x->sc.i_w;
  x->sc.h     = x->i_window_h;

  x->spectr.view = x->spectr.i_view;
  x->spectr.w    = x->spectr.i_w;
  x->spectr.h    = x->i_window_h;

  x->phase.view  = x->phase.i_view;
  x->phase.w     = x->i_window_h;
  x->phase.h     = x->i_window_h;

  int ox = 0;
  int w = 0;

  x->split_lf_x0 = ox;
  x->split_lf_y0 = x->split_w;
  x->split_lf_w  = x->split_w;
  x->split_lf_h  = x->i_window_h;

  ox += x->split_w;
  w  += x->split_w;

  
  if (x->sc.view)
    {
      x->sc.ox = ox;
      x->sc.oy = x->split_w;
      x->sc.maxy = x->sc.oy + x->sc.h;
      
      x->sc.split_x0 = ox + x->sc.w;
      x->sc.split_y0 = x->split_w;
      x->sc.split_w  = x->split_w;
      x->sc.split_h  = x->i_window_h;

      ox += x->sc.w + x->split_w;
      w  += x->sc.w + x->split_w;
    }

  if (x->spectr.view)
    {
      x->spectr.ox = ox;
      x->spectr.oy = x->split_w;
      x->spectr.maxy = x->spectr.oy + x->spectr.h;
      
      x->spectr.split_x0 = ox + x->spectr.w;
      x->spectr.split_y0 = x->split_w;
      x->spectr.split_w  = x->split_w;
      x->spectr.split_h  = x->i_window_h;

      ox += x->spectr.w + x->split_w;
      w  += x->spectr.w + x->split_w;
    }

  if (x->phase.view)
    {
      x->phase.ox = ox;
      x->phase.oy = x->split_w;
      x->phase.maxy = x->phase.oy + x->phase.h;
      
      x->phase.split_x0 = ox + x->phase.w;
      x->phase.split_y0 = x->split_w;
      x->phase.split_w  = x->split_w;
      x->phase.split_h  = x->i_window_h;

      ox += x->phase.w + x->split_w;
      w  += x->phase.w + x->split_w;
    }

  x->window_w = w;

  x->split_up_x0 = 0;
  x->split_up_y0 = 0;
  x->split_up_w  = w;
  x->split_up_h  = x->split_w;

  x->split_dw_x0 = 0;
  x->split_dw_y0 = x->split_w + x->i_window_h;
  x->split_dw_w  = w;
  x->split_dw_h  = x->split_w;
}

//----------------------------------------------------------------------------//
void n_sa_scope_calc_sep(t_n_sa *x)
{
  int i;
  int all;
  int last;

  // all in's
  all = 0;
  for (i = 0; i < x->amount_channel; i++)
    {
      if (x->sc.ch[i].on)
	all++;
    }

  // no separate
  if (x->sc.separate == 0 || all <= 1)
    {
      x->sc.h_one  = x->sc.h;
      x->sc.h_half = x->sc.h_one / 2;
      for (i = 0; i < x->amount_channel; i++)
	{
	  x->sc.ch[i].sep_x0  = x->sc.ox;
	  x->sc.ch[i].sep_y0  = x->sc.oy + x->sc.h_half;
	  x->sc.ch[i].sep_w   = x->sc.w;
	  x->sc.ch[i].sep_lo  = -1;
	  x->sc.ch[i].c       = x->sc.ch[i].sep_y0;
	}
    }

  // separate
  else
    {
      x->sc.h_one  = x->sc.h / all;
      x->sc.h_half = x->sc.h_one / 2;
      all = 0;
      last = 0;
      for (i = 0; i < x->amount_channel; i++)
	{
	  if (x->sc.ch[i].on)
	    {
	      x->sc.ch[i].sep_x0  = x->sc.ox;
	      x->sc.ch[i].sep_y0  = x->sc.oy + (x->sc.h_one * (all + 0.5));
	      x->sc.ch[i].sep_w   = x->sc.w;
	      x->sc.ch[i].sep_lo  = x->sc.oy + (x->sc.h_one * (all + 1.0));
	      x->sc.ch[i].c       = x->sc.ch[i].sep_y0;
	      last = i;
	      all++;
	    }
	}
      x->sc.ch[last].sep_lo = -1;
    }
}

//----------------------------------------------------------------------------//
void n_sa_scope_calc_grid_hor(t_n_sa *x)
{
  int i;
  t_float h;
  t_float f;
  int center;
  int half;

  x->sc.grid_hor_x0 = x->sc.ox;

  half   = x->sc.h / 2.0;
  center = x->sc.oy + (x->sc.h / 2.0);
  h = (t_float)half / ((t_float)x->sc.grid_hor + 1.0);

  x->sc.grid_hor_y_c = center;

  if (x->sc.grid_hor > 0)
    {
      for (i = 0; i < x->sc.grid_hor; i++)
	{
	  f = h * ((t_float)i + 1.0);
	  x->sc.grid_hor_y_up[i] = (t_float)center - f;
	  x->sc.grid_hor_y_dw[i] = (t_float)center + f;
	}
    }
}

//----------------------------------------------------------------------------//
void n_sa_scope_calc_grid_ver(t_n_sa *x)
{
  int i;
  t_float w;

  x->sc.grid_ver_y0 = x->sc.oy;
  w = (t_float)x->sc.w / ((t_float)x->sc.grid_ver + 1.0);
  if (x->sc.grid_ver > 0)
    {
      for (i = 0; i < x->sc.grid_ver; i++)
	{
	  x->sc.grid_ver_x[i] = (w * ((t_float)i + 1.0)) + (t_float)x->sc.ox;
	}
    }
}

//----------------------------------------------------------------------------//
// colors //////////////////////////////////////////////////////////////////////
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
  x->color_split    = n_sa_color(x, x->i_color_split);
  x->color_back     = n_sa_color(x, x->i_color_back);
  x->color_grid     = n_sa_color(x, x->i_color_grid);
  x->color_sep      = n_sa_color(x, x->i_color_sep);
  x->color_recpos   = n_sa_color(x, x->i_color_recpos);
  for (i=0; i<x->amount_channel; i++)
    {
      x->ch_colors[i].color = n_sa_color(x, x->ch_colors[i].i_color);
    }
}

//----------------------------------------------------------------------------//
// draw ////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void draw_line_hor(Uint32 *pix, int ofs, int x0, int y0, int w, Uint32 col)
{
  int x;
  Uint32 *bufp;
  Uint32 *p;
  p = pix + (y0 * ofs);
  for (x=0; x<w; x++)
    {
      bufp = p + x0 + x; /* fix this */
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
      p = pix + ((y0 + y) * ofs); /* fix this */
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
      p = pix + ((y0 + y) * ofs); /* fix this */
      for (x=0; x<w; x++)
	{
	  bufp = p + x0 + x;
	  *bufp = col;
	}
    }
}

//----------------------------------------------------------------------------//
// redraw //////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sa_redraw(t_n_sa *x)
{
  int i,j;

  // split_lf
  draw_rect(x->pix, x->ofs,
	    x->split_lf_x0,
	    x->split_lf_y0,
	    x->split_lf_w,
	    x->split_lf_h,
	    x->color_split);
  // split_up
  draw_rect(x->pix, x->ofs,
	    x->split_up_x0,
	    x->split_up_y0,
	    x->split_up_w,
	    x->split_up_h,
	    x->color_split);
  // split_dw
  draw_rect(x->pix, x->ofs,
	    x->split_dw_x0,
	    x->split_dw_y0,
	    x->split_dw_w,
	    x->split_dw_h,
	    x->color_split);



  // scope
  if (x->sc.view && x->sc.update)
    {
      // split
      draw_rect(x->pix, x->ofs,
		x->sc.split_x0,
		x->sc.split_y0,
		x->sc.split_w,
		x->sc.split_h,
		x->color_split);


      // back
      draw_rect(x->pix, x->ofs,
		x->sc.ox,
		x->sc.oy,
		x->sc.w,
		x->sc.h,
		x->color_back);


      // grid
      if (x->sc.grid_view)
	{

	  // sep lo
	  if (x->sc.separate)
	    {
	      for (i=0; i<x->amount_channel; i++)
		{
		  if (x->sc.ch[i].on && x->sc.ch[i].sep_lo != -1)
		    {
		      draw_line_hor(x->pix, x->ofs,
				    x->sc.ch[i].sep_x0,
				    x->sc.ch[i].sep_lo,
				    x->sc.ch[i].sep_w,
				    x->color_grid);
		    }
		}
	    }

	  // grid hor
	  else
	    {
	      // center
	      draw_line_hor(x->pix, x->ofs,
			    x->sc.grid_hor_x0,
			    x->sc.grid_hor_y_c,
			    x->sc.w,
			    x->color_grid);
	      // up dw
	      if (x->sc.grid_hor > 0)
		{
		  for (i=0; i<x->sc.grid_hor; i++)
		    {
		      draw_line_hor(x->pix, x->ofs,
				    x->sc.grid_hor_x0,
				    x->sc.grid_hor_y_up[i],
				    x->sc.w,
				    x->color_grid);
		      draw_line_hor(x->pix, x->ofs,
				    x->sc.grid_hor_x0,
				    x->sc.grid_hor_y_dw[i],
				    x->sc.w,
				    x->color_grid);
		    }
		}
	    }

	  // grid ver
	  if (x->sc.grid_ver > 0)
	    {
	      for (i=0; i<x->sc.grid_ver; i++)
		{
		  draw_line_ver(x->pix, x->ofs,
				x->sc.grid_ver_x[i],
				x->sc.grid_ver_y0,
				x->sc.h,
				x->color_grid);
		}
	    }
	}


      // separator
      if (x->sc.sep_view)
	{
	  if (x->sc.separate)
	    {
	      for (i=0; i<x->amount_channel; i++)
		{
		  if (x->sc.ch[i].on)
		    {
		      draw_line_hor(x->pix, x->ofs,
				    x->sc.ch[i].sep_x0,
				    x->sc.ch[i].sep_y0,
				    x->sc.ch[i].sep_w,
				    x->color_sep);
		    }
		}
	    }
	  else
	    {
	      draw_line_hor(x->pix, x->ofs,
			    x->sc.ch[0].sep_x0,
			    x->sc.ch[0].sep_y0,
			    x->sc.ch[0].sep_w,
			    x->color_sep);
	    }
	}


      // text


      // wave
      int max, min;
      int max_z, min_z;
      int x0, y0, h;
      for (i=0; i < x->amount_channel; i++)
	{
	  if (x->sc.ch[i].on)
	    {
	      // first point
	      min_z = x->sc.ch[i].min[0] * x->sc.h_half;
	      max_z = x->sc.ch[i].max[0] * x->sc.h_half;

	      // draw one wave
	      for (j=0; j < x->sc.w; j++)
		{

		  // min and max
		  min = x->sc.ch[i].min[j] * x->sc.h_half;
		  max = x->sc.ch[i].max[j] * x->sc.h_half;

		  // with z
		  if (min > max_z)
		    min = max_z;
		  else if (max < min_z)
		    max = min_z;

		  // z
		  min_z = min;
		  max_z = max;

		  // to display
		  min = x->sc.ch[i].c - min;
		  max = x->sc.ch[i].c - max;
		  
		  // clip coords
		  if (min < x->sc.oy)
		    min = x->sc.oy;
		  else if (min > x->sc.maxy)
		    min = x->sc.maxy;
		  
		  if (max < x->sc.oy)
		    max = x->sc.oy;
		  else if (max > x->sc.maxy)
		    max = x->sc.maxy;

		  // for func
		  x0 = x->sc.ox + j;
		  y0 = max;
		  h = min - max;
		  if (h < 1)
		    {
		      h = 1;
		    }

		  draw_line_ver(x->pix, x->ofs,
				x0,
				y0,
				h,
				x->ch_colors[i].color);		  
		}
	    }
	}

      // recpos
      if (x->sc.recpos_view)
	{
	  draw_line_ver(x->pix, x->ofs,
			x->sc.ox + x->sc.disp_count,
			x->sc.grid_ver_y0,
			x->sc.h,
			x->color_recpos);
	}

      x->sc.update = 0;
    }


  // spectr
  if (x->spectr.view)
    {
      // split
      draw_rect(x->pix, x->ofs,
		x->spectr.split_x0,
		x->spectr.split_y0,
		x->spectr.split_w,
		x->spectr.split_h,
		x->color_split);

      // back
      draw_rect(x->pix, x->ofs,
		x->spectr.ox,
		x->spectr.oy,
		x->spectr.w,
		x->spectr.h,
		x->color_back);
    }


  // phase
  if (x->phase.view)
    {
      // split
      draw_rect(x->pix, x->ofs,
		x->phase.split_x0,
		x->phase.split_y0,
		x->phase.split_w,
		x->phase.split_h,
		x->color_split);

      // back
      draw_rect(x->pix, x->ofs,
		x->phase.ox,
		x->phase.oy,
		x->phase.w,
		x->phase.h,
		x->color_back);
    }

  // sdl
  SDL_UpdateWindowSurface(x->win);
}

//----------------------------------------------------------------------------//
// sdl /////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
void n_sa_sdl_window(t_n_sa *x)
{
  // sdl
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
void n_sa_sdl_events(t_n_sa *x)
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
                  x->window_ox = x->event.window.data1;
                  x->window_oy = x->event.window.data2;
		  n_sa_output(x, x->s_window_ox, x->window_ox); 
		  n_sa_output(x, x->s_window_oy, x->window_oy); 
                  break;
                case SDL_WINDOWEVENT_CLOSE:
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

      n_sa_redraw(x);
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
      n_sa_scope_calc_sep(x);
      n_sa_scope_calc_grid_hor(x);
      n_sa_scope_calc_grid_ver(x);
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
  CLIP_MINMAX(WINDOW_HEIGHT_MIN, WINDOW_HEIGHT_MAX, f);
  x->i_window_h = f;
}

//----------------------------------------------------------------------------//
static void n_sa_window_fps(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(WINDOW_FPS_MIN, WINDOW_FPS_MAX, f);
  x->fps = f;
  n_sa_calc_constant(x);
}

//----------------------------------------------------------------------------//
static void n_sa_color_split(t_n_sa *x, t_floatarg f)
{
  x->i_color_split = f * -1;
  if (x->window)
    {
      x->color_split = n_sa_color(x, x->i_color_split);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_back(t_n_sa *x, t_floatarg f)
{
  x->i_color_back = f * -1;
  if (x->window)
    {
      x->color_back = n_sa_color(x, x->i_color_back);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_grid(t_n_sa *x, t_floatarg f)
{
  x->i_color_grid = f * -1;
  if (x->window)
    {
      x->color_grid = n_sa_color(x, x->i_color_grid);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_sep(t_n_sa *x, t_floatarg f)
{
  x->i_color_sep = f * -1;
  if (x->window)
    {
      x->color_sep = n_sa_color(x, x->i_color_sep);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_recpos(t_n_sa *x, t_floatarg f)
{
  x->i_color_recpos = f * -1;
  if (x->window)
    {
      x->color_recpos = n_sa_color(x, x->i_color_recpos);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_color_ch(t_n_sa *x, t_floatarg ch, t_floatarg f)
{
  int i = ch;
  CLIP_MINMAX(0, x->amount_channel - 1, i);
  x->ch_colors[i].i_color = f * -1;
  if (x->window)
    {
      x->ch_colors[i].color = n_sa_color(x, x->ch_colors[i].i_color);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_scope_view(t_n_sa *x, t_floatarg f)
{
  x->sc.i_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_w(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(SCOPE_WIDTH_MIN, SCOPE_WIDTH_MAX, f);
  x->sc.i_w = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_grid_view(t_n_sa *x, t_floatarg f)
{
  x->sc.grid_view = (f > 0);
  x->sc.update = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_grid_ver(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(SCOPE_GRID_VER_MIN, SCOPE_GRID_VER_MAX, f);
  x->sc.grid_ver = f;
  n_sa_scope_calc_grid_ver(x);
  x->sc.update = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_grid_hor(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(SCOPE_GRID_HOR_MIN, SCOPE_GRID_HOR_MAX, f);
  x->sc.grid_hor = f;
  n_sa_scope_calc_grid_hor(x);
  x->sc.update = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sep_view(t_n_sa *x, t_floatarg f)
{
  x->sc.sep_view = (f > 0);
  x->sc.update = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_separate(t_n_sa *x, t_floatarg f)
{
  x->sc.separate = (f > 0);
  n_sa_scope_calc_sep(x);
  x->sc.update = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_recpos_view(t_n_sa *x, t_floatarg f)
{
  x->sc.recpos_view = (f > 0);
  x->sc.update = 1;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync(t_n_sa *x, t_floatarg f)
{
  if      (f <= 0)
    x->sc.sync = SCOPE_SYNC_OFF;
  else if (f == 1)
    x->sc.sync = SCOPE_SYNC_UP;
  else
    x->sc.sync = SCOPE_SYNC_DOWN;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_channel(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(0, x->amount_channel - 1, f);
  x->sc.sync_channel = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_treshold(t_n_sa *x, t_floatarg f)
{
  x->sc.sync_treshold = f;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_dc(t_n_sa *x, t_floatarg f)
{
  x->sc.sync_dc = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_sync_dc_freq(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(1.0, 1000.0, f);
  x->sc.sync_dc_freq = f;
  x->sc.sync_dc_f = x->sc.sync_dc_freq * (C_2PI / (t_float)x->s_sr);
  x->sc.sync_dc_z = 0.0;
}

//----------------------------------------------------------------------------//
static void n_sa_scope_spp(t_n_sa *x, t_floatarg f)
{
  CLIP_MIN(1, f);
  x->sc.spp = f;
  n_sa_calc_constant(x);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_freeze(t_n_sa *x, t_floatarg f)
{
  x->sc.freeze = (f == 0);
}

//----------------------------------------------------------------------------//
static void n_sa_scope_on(t_n_sa *x, t_floatarg ch, t_floatarg f)
{
  int i = ch;
  CLIP_MINMAX(0, x->amount_channel - 1, i);
  x->sc.ch[i].on = (f > 0);
  if (x->window)
    {
      n_sa_scope_calc_sep(x);
      x->sc.update = 1;
    }
}

//----------------------------------------------------------------------------//
static void n_sa_scope_amp(t_n_sa *x, t_floatarg ch, t_floatarg f)
{
  int i = ch;
  CLIP_MINMAX(0, x->amount_channel - 1, i);
  x->sc.ch[i].amp = f;
}

//----------------------------------------------------------------------------//
static void n_sa_spectr_view(t_n_sa *x, t_floatarg f)
{
  x->spectr.i_view = (f > 0);
}

//----------------------------------------------------------------------------//
static void n_sa_spectr_w(t_n_sa *x, t_floatarg f)
{
  CLIP_MINMAX(SPECTR_WIDTH_MIN, SPECTR_WIDTH_MAX, f);
  x->spectr.i_w = f;
}

//----------------------------------------------------------------------------//
static void n_sa_phase_view(t_n_sa *x, t_floatarg f)
{
  x->phase.i_view = (f > 0);
}

//----------------------------------------------------------------------------//
// dsp /////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------//
t_int *n_sa_perform(t_int *w)
{
  int i;
  t_n_sa *x = (t_n_sa *)(w[1]);
  t_sample *sig[CHANNEL_MAX];  /* vector in's */
  for (i = 0; i < x->amount_channel; i++)
    {
      sig[i] = (t_sample *)(w[i + 2]);
    }
  int n; /* blocksize */
  t_float f;
  t_float in_sync;
  t_float in_sync_z = x->sc.sync_z;
  int scw;

  // dsp
  if (x->window)
    {
      // scope /////////////////////////////////////////////////////////////////
      if (x->sc.view && x->sc.freeze)
	{

	  for (n=0; n < x->s_n; n++)
	    {

	      // always update
	      if (x->sc.recpos_view)
		{
		  scw = 1;
		  x->sc.update = 1;
		}
	      // only when buffer ready
	      else
		{
		  if (x->sc.update == 0)
		    {
		      scw = 1;
		    }
		  else
		    {
		      scw = 0;
		    }
		}

	      // record sync in
	      in_sync = sig[x->sc.sync_channel][n];
	      
	      // dc filter(hp 1-pole)
	      if (x->sc.sync_dc)
		{
		  x->sc.sync_dc_z = 
		    (in_sync - x->sc.sync_dc_z) 
		    * x->sc.sync_dc_f 
		    + x->sc.sync_dc_z;
		  in_sync = in_sync - x->sc.sync_dc_z;
		}
	      
	      // find
	      if (x->sc.find == 0 && scw)
		{
		  // sync
		  if      (x->sc.sync == SCOPE_SYNC_OFF)
		    {
		      x->sc.find = 1;
		    }
		  else if (x->sc.sync == SCOPE_SYNC_UP)
		    {
		      if (in_sync   >  x->sc.sync_treshold &&
			  in_sync_z <= x->sc.sync_treshold)
			{
			  x->sc.find = 1;
			  x->sc.spp_count = 0;
			}
		    }
		  else if (x->sc.sync == SCOPE_SYNC_DOWN)
		    {
		      if (in_sync   <= x->sc.sync_treshold &&
			  in_sync_z >  x->sc.sync_treshold)
			{
			  x->sc.find = 1;
			  x->sc.disp_count = 0;
			  x->sc.spp_count = 0;
			}
		    }
		}
	      
	      // sync z
	      in_sync_z = in_sync;
	      
	      // find complete. now record.
	      if (x->sc.find)
		{
		  
		  // find min max
		  for (i=0; i < x->amount_channel; i++)
		    {
		      if (x->sc.ch[i].on)
			{
			  f = sig[i][n] * x->sc.ch[i].amp; // signal
			  if (x->sc.ch[i].max_z < f)
			    {
			      x->sc.ch[i].max_z = f;
			    }
			  if (x->sc.ch[i].min_z > f)
			    {
			      x->sc.ch[i].min_z = f;
			    }
			}
		    }
		  
		  // count spp
		  x->sc.spp_count++;
		  if (x->sc.spp_count >= x->sc.spp)
		    {
		      x->sc.spp_count = 0;
		      
		      for (i=0; i < x->amount_channel; i++)
			{
			  if (x->sc.ch[i].on)
			    {
			      x->sc.ch[i].max[x->sc.disp_count] = x->sc.ch[i].max_z;
			      x->sc.ch[i].max_z = SCOPE_MIN_V;
			      
			      x->sc.ch[i].min[x->sc.disp_count] = x->sc.ch[i].min_z;
			      x->sc.ch[i].min_z = SCOPE_MAX_V;
			    }
			}
		      
		      // count disp
		      x->sc.disp_count++;
		      if (x->sc.disp_count >= x->sc.w)
			{
			  x->sc.disp_count = 0;
			  x->sc.find = 0;
			  x->sc.update = 1;
			}
		    }
		}
	    } // end dsp
	  x->sc.sync_z = in_sync_z;
	}

      // spectr ////////////////////////////////////////////////////////////////
      if (x->spectr.view)
	{
	}

      // phase /////////////////////////////////////////////////////////////////
      if (x->phase.view)
	{
	}
    }
  
  // next
  for (i = 0; i < x->amount_channel; i++)
    {
      sig[i] += x->s_n;
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
  CLIP_MINMAX(1, CHANNEL_MAX, x->amount_channel);
  
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
  x->split_w = 2;

  x->fps = 50;

  x->sc.i_w = 200;
  /* x->i_spectr_w = 200; */
  /* x->i_sc_view = 0; */
  /* x->i_spectr_view = 0; */
  /* x->i_phase_view = 0; */
  /* x->i_color_split = 0; */
  /* x->i_color_back = 0; */
  /* x->i_color_grid = 0; */
  /* x->i_color_sep = 0; */
  /* x->i_color_recpos = 0; */

  /* x->sc_grid_view = 0; */
  /* x->sc_grid_ver = 0; */
  /* x->sc_grid_hor = 0; */
  /* x->sc_sep_view = 0; */
  /* x->sc_recpos_view = 0; */
  /* x->sc_separate = 0; */
  /* x->sc_sync = 0; */
  /* x->sc_sync_channel = 0; */
  /* x->sc_sync_treshold = 0.0; */
  /* x->sc_sync_dc = 0; */
  /* x->sc_sync_dc_freq = 1.0; */
  x->sc.spp = 1;
  x->sc.freeze = 1;

  x->sc.sync_z = 0.0;
  x->sc.find = 0;
  x->sc.disp_count = 0;
  x->sc.spp_count = 0;
  x->sc.update = 1;

  for (i = 0; i < x->amount_channel; i++)
    {
      x->ch_colors[i].i_color = 0;
      x->sc.ch[i].on = 0;
      x->sc.ch[i].amp = 0.0;
    }

  n_sa_calc_constant(x);

  // mem
  x->v_d = getbytes(sizeof(t_int *) *(x->amount_channel + 1));

  // clock
  x->cl = clock_new(x, (t_method)n_sa_sdl_events);

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
  if (x->window)
    {
      x->window = 0;
      n_sa_sdl_window_close(x);
    }
}

//----------------------------------------------------------------------------//
#define METHOD0(F,S) \
class_addmethod(n_sa_class,(t_method)(F),gensym((S)), 0);
#define METHOD1(F,S) \
class_addmethod(n_sa_class,(t_method)(F),gensym((S)), A_FLOAT, 0);
#define METHOD2(F,S) \
class_addmethod(n_sa_class,(t_method)(F),gensym((S)), A_FLOAT, A_FLOAT, 0);

void n_sa_tilde_setup(void)
{
  n_sa_class=class_new(gensym("n_sa~"),
		       (t_newmethod)n_sa_new,
		       (t_method)n_sa_free,
		       sizeof(t_n_sa),
		       0, A_GIMME, 0);
  METHOD0(nullfn,                     "signal");
  METHOD0(n_sa_dsp,                   "dsp");
  /* window */
  METHOD1(n_sa_window,                "window");
  METHOD1(n_sa_window_ox,             "window_ox"); 
  METHOD1(n_sa_window_oy,             "window_oy");
  METHOD1(n_sa_window_h,              "window_h");
  METHOD1(n_sa_window_fps,            "window_fps");
  /* colors */
  METHOD1(n_sa_color_split,           "color_split");
  METHOD1(n_sa_color_back,            "color_back");
  METHOD1(n_sa_color_grid,            "color_grid");
  METHOD1(n_sa_color_sep,             "color_sep");
  METHOD1(n_sa_color_recpos,          "color_recpos");
  METHOD2(n_sa_color_ch,              "color_ch");
  /* scope */
  METHOD1(n_sa_scope_view,            "scope_view");
  METHOD1(n_sa_scope_w,               "scope_w");
  METHOD1(n_sa_scope_grid_view,       "scope_grid_view");
  METHOD1(n_sa_scope_grid_ver,        "scope_grid_ver");
  METHOD1(n_sa_scope_grid_hor,        "scope_grid_hor");
  METHOD1(n_sa_scope_sep_view,        "scope_sep_view");
  METHOD1(n_sa_scope_separate,        "scope_separate");
  METHOD1(n_sa_scope_recpos_view,     "scope_recpos_view");
  METHOD1(n_sa_scope_sync,            "scope_sync");
  METHOD1(n_sa_scope_sync_channel,    "scope_sync_channel");
  METHOD1(n_sa_scope_sync_treshold,   "scope_sync_treshold");
  METHOD1(n_sa_scope_sync_dc,         "scope_sync_dc");
  METHOD1(n_sa_scope_sync_dc_freq,    "scope_sync_dc_freq");
  METHOD1(n_sa_scope_spp,             "scope_spp");
  METHOD1(n_sa_scope_freeze,          "scope_freeze");
  METHOD2(n_sa_scope_on,              "scope_on");
  METHOD2(n_sa_scope_amp,             "scope_amp");
  /* spectr */
  METHOD1(n_sa_spectr_view,           "spectr_view");
  METHOD1(n_sa_spectr_w,              "spectr_w");
  /* phase */
  METHOD1(n_sa_phase_view,            "phase_view");
}

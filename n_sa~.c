/* n_sa - signal analysis tool */

#include <stdlib.h>
#include <SDL/SDL.h>
#include "m_pd.h"

#define A_MAX_CHANNEL 16
#define A_MIN_WIDTH 100
#define A_MAX_WIDTH 1024
#define A_MIN_HEIGHT 100
#define A_MAX_HEIGHT 1024
#define A_DC_F 20.
#define A_FPS 22
#define A_MAX_SS 100000
#define AC_2PI 6.2831853

#define AF_CLIP_MINMAX(MIN, MAX, IN)            \
  if ((IN) < (MIN))      (IN) = (MIN);          \
  else if ((IN) > (MAX)) (IN) = (MAX);

#define AF_CLIP_MIN(MIN, IN)                    \
  if ((IN) < (MIN))          (IN) = (MIN);

#define AF_CLIP_MAX(MAX, IN)                    \
  if ((IN) < (MAX))          (IN) = (MAX);



static t_class *n_scop_class;

typedef struct _n_scop_channel
{
  int on;
  float amp;
  int color;
  Uint32 rgb_color;
  int center;
  int grid_hor_low;
  int max_z;
  int min_z;
  float *buf;
  float *buf_max;
  float *buf_min;
} t_n_scop_channel;

typedef struct _n_scop
{
  t_object x_obj;
  t_outlet *x_out;
  int p_on;
  int p_sync_dc;
  int p_sync_ch;
  float p_sync_treshold;
  int p_sync;
  float p_time;
  int p_ms_smpl;
  int p_width;
  int p_height;
  int p_color_back;
  int p_color_grid;
  int p_color_grid_low;
  int p_window;
  int p_separate;
  int p_grid_hor_on;
  int sr;
  int channel;
  int win_w;
  int win_h;
  int w_height_one;
  int w_height_half;
  int w_last;
  int width;
  int t_all;
  float t_part;
  float dc_f;
  float dc_z;
  float sync_z;
  int s_record;
  int s_find;
  int s_count_redraw_max;
  int s_count_redraw;
  int s_count_record;
  int s_count_litl;
  Uint32 rgb_color_back;
  Uint32 rgb_color_grid;
  Uint32 rgb_color_grid_low;
  SDL_Surface *win_screen;
  Uint32 *win_p;
  int win_ofs;
  struct _n_scop_channel ch[A_MAX_CHANNEL];
} t_n_scop;

//----------------------------------------------------------------------------//
void n_scop_calc_wh(t_n_scop *x)
{
  x->width = x->p_width;
  x->win_w = x->p_width;
  x->win_h = x->p_height;
}

//----------------------------------------------------------------------------//
void n_scop_calc_ch(t_n_scop *x)
{
  int i, j;
  j = 0;
  // all channel's
  for (i = 0; i < x->channel; i++)
    {
      if (x->ch[i].on)
	j++;
    }
  // no separate
  if (x->p_separate == 0 || j < 2)
    {
      x->w_height_one = x->win_h;
      x->w_height_half = x->win_h / 2;
      for (i = 0; i < x->channel; i++)
	x->ch[i].center = x->w_height_half;
      x->w_last = 0;
    }
  // separate
  else
    {
      x->w_height_one = x->win_h / j;
      x->w_height_half = x->w_height_one / 2;
      j = 0;
      for (i = 0; i < x->channel; i++)
	{
	  if (x->ch[i].on)
	    {
	      x->ch[i].center = x->w_height_one * (j + 0.5);
	      x->ch[i].grid_hor_low = x->w_height_one * (j + 1);
	      x->w_last = i;
	      j++;
	    }
	}
    }
}

//----------------------------------------------------------------------------//
void n_scop_color_rgb(t_n_scop *x, int color, Uint32 *rgb_color)
{
  Uint8 r, g, b;
  b = color;
  g = color >> 8;
  r = color >> 16;
  *rgb_color = SDL_MapRGB(x->win_screen->format, r, g, b);
}

//----------------------------------------------------------------------------//
void n_scop_color_all(t_n_scop *x)
{
  int i;
  n_scop_color_rgb(x, x->p_color_back, &x->rgb_color_back);
  n_scop_color_rgb(x, x->p_color_grid, &x->rgb_color_grid);
  n_scop_color_rgb(x, x->p_color_grid_low, &x->rgb_color_grid_low);
  for (i = 0; i < A_MAX_CHANNEL; i++)
    n_scop_color_rgb(x, x->ch[i].color, &x->ch[i].rgb_color);
}

//----------------------------------------------------------------------------//
void n_scop_calc_display(t_n_scop *x, int ch)
{
  int i, j, x0, x1;
  float min, max;
  float count;
  count = 0;
  for (i = 0; i < x->width; i++)
    {
      x0 = count;
      count += x->t_part;
      x1 = count - 1;
      max = x->ch[ch].buf[x0];
      min = x->ch[ch].buf[x0];
      for (j = x0 + 1; j < x1; j++)
	{
	  if (max < x->ch[ch].buf[j])
	    max = x->ch[ch].buf[j];
	  if (min > x->ch[ch].buf[j])
	    min = x->ch[ch].buf[j];
	}
      x->ch[ch].buf_max[i] = max;
      x->ch[ch].buf_min[i] = min;
    }
}

//----------------------------------------------------------------------------//
void n_scop_calc_constant(t_n_scop *x)
{
  x->dc_f = A_DC_F * (AC_2PI / (float)x->sr);
  x->s_count_redraw_max = x->sr / A_FPS;
}

//----------------------------------------------------------------------------//
void n_scop_redraw_display(t_n_scop *x)
{
  int i, ch, y, y0, y1, yz0, yz1;
  Uint32 *bufp;
  Uint32 *p;
  if (x->p_window)
    {
      // clear
      for (y = 0; y < x->win_h; y++)
	{
	  p = x->win_p + (y * x->win_ofs);
	  for (i = 0; i < x->win_w; i++)
	    {
	      bufp = p + i;
	      *bufp = x->rgb_color_back;
	    }
	}
      // grid vert
      // one channel
      for (ch = 0; ch < x->channel; ch++)
	{
	  if (x->ch[ch].on)
	    {
	      // start pos
	      x->ch[ch].max_z = -999999999;
	      x->ch[ch].min_z = 999999999;
	      // grid hor
	      if (x->p_grid_hor_on)
		{
		  // draw center
		  p = x->win_p + (x->ch[ch].center * x->win_ofs);
		  for (i = 0; i < x->win_w; i++)
		    {
		      bufp = p + i;
		      *bufp = x->rgb_color_grid;
		    }
		  // draw low
		  if (ch < x->w_last)
		    {
		      p = x->win_p + (x->ch[ch].grid_hor_low * x->win_ofs);
		      for (i = 0; i < x->win_w; i++)
			{
			  bufp = p + i;
			  *bufp = x->rgb_color_grid_low;
			}
		    }
		}
	      // wave
	      for (i = 0; i < x->win_w; i++)
		{
		  // p
		  p = x->win_p + i;
		  // point y0 y1
		  yz0 = x->ch[ch].center - (x->ch[ch].buf_max[i] * x->w_height_half);
		  yz1 = yz0 + ((x->ch[ch].buf_max[i] - x->ch[ch].buf_min[i]) * x->w_height_half);
		  if (x->ch[ch].max_z >= yz1)
		    y1 = x->ch[ch].max_z + 1;
		  else
		    y1 = yz1 + 1;
		  if (x->ch[ch].min_z <= yz0)
		    y0 = x->ch[ch].min_z;
		  else
		    y0 = yz0;
		  // z
		  x->ch[ch].max_z = yz0;
		  x->ch[ch].min_z = yz1;
		  // clip
		  AF_CLIP_MINMAX(0, x->win_h, y0)
		    AF_CLIP_MINMAX(0, x->win_h, y1)
		    // draw
		    for (y = y0; y < y1; y++)
		      {
			bufp = p + (y * x->win_ofs);
			*bufp = x->ch[ch].color;
		      }
		}
	    }
	}
      SDL_UpdateRect(x->win_screen, 0, 0, 0, 0);
    }
}

//----------------------------------------------------------------------------//
void n_scop_reset(t_n_scop *x)
{
  x->s_record = 0;
  x->s_find = 0;
  x->s_count_redraw = 0;
  x->s_count_record = 0;
  x->s_count_litl = 0;
}

//----------------------------------------------------------------------------//
void n_scop_calc_time(t_n_scop *x)
{
  // ms
  if (x->p_ms_smpl == 0)
    x->t_all = x->p_time * x->sr * 0.001;
  // smpl
  else
    x->t_all = x->p_time;
  AF_CLIP_MINMAX(1, A_MAX_SS, x->t_all)
    // calc
    x->t_part = (float)x->t_all / x->width;
}

//----------------------------------------------------------------------------//
t_int *n_scop_perform(t_int *w)
{
  t_n_scop *x = (t_n_scop *)(w[1]);
  int n = (int)(w[2]);
  t_float **in = getbytes(sizeof(float) * (x->channel));
  int i, j;
  float sig[x->channel];
  float sig_sync;
  for (i = 0; i < x->channel; i++)
    in[i] = (t_float *)(w[i + 3]);

  if (x->p_on)
    {
      // dsp
      while (n--)
	{
	  // record input
	  sig_sync = *in[x->p_sync_ch];
	  for (i = 0; i < x->channel; i++)
	    sig[i] = *in[i]++ * x->ch[i].amp;
	  // dc filter on ?
	  if (x->p_sync_dc)
	    {
	      x->dc_z = (sig_sync - x->dc_z) * x->dc_f + x->dc_z;
	      sig_sync = sig_sync - x->dc_z;
	    }
	  // count redraw
	  x->s_count_redraw++;
	  if (x->s_count_redraw > x->s_count_redraw_max)
	    {
	      x->s_count_redraw = 0;
	      x->s_record = 1;
	    }
	  // record ?
	  if (x->s_record)
	    {
	      // record
	      if (x->s_count_record < x->t_all)
		{
		  // find
		  if (x->s_find == 0 && x->p_sync > 0)
		    {
		      if (sig_sync > x->p_sync_treshold && x->sync_z <= x->p_sync_treshold)
			{
			  // find complete
			  x->s_find = 1;
			  x->s_count_record = 0;
			  x->s_count_litl = 0;
			}
		      x->sync_z = sig_sync;
		    }
		  else
		    x->s_find = 1;
		  // record to array's
		  if (x->s_find)
		    {
		      for (j = 0; j < x->channel; j++)
			{
			  if (x->ch[j].on)
			    x->ch[j].buf[x->s_count_record] = sig[j];
			}
		      x->s_count_record++;
		    }
		}
	      // stop record
	      else
		{
		  x->s_record = 0;
		  x->s_find = 0;
		  x->s_count_record = 0;
		  x->s_count_litl = 0;
		  // this redraw display fuction
		  for (i = 0; i < x->channel; i++)
		    {
		      if (x->ch[i].on)
			n_scop_calc_display(x, i);
		      n_scop_redraw_display(x);
		    }
		}
	    }
	}
    }
  return (w + x->channel + 3);
}

//----------------------------------------------------------------------------//
static void n_scop_dsp(t_n_scop *x, t_signal **sp)
{
  int i;
  t_int **vec = getbytes(sizeof(t_int) * (x->channel + 2));
  vec[0] = (t_int *)x;
  vec[1] = (t_int *)sp[0]->s_n;
  for (i = 0; i < x->channel; i++)
    vec[i + 2] = (t_int *)sp[i]->s_vec;
  dsp_addv(n_scop_perform, x->channel + 2, (t_int *)vec);
  freebytes(vec, sizeof(t_int) * (x->channel + 2));
  //
  x->sr = sp[0]->s_sr;
  n_scop_calc_time(x);
  n_scop_calc_constant(x);
  n_scop_reset(x);
}

//----------------------------------------------------------------------------//
void n_scop_window(t_n_scop *x)
{
  // sdl init
  if (x->p_window)
    {
      if (SDL_Init(SDL_INIT_VIDEO) != 0)
	post("SDL_Init error: %s", SDL_GetError());
      else
	{
	  // set video mode
	  x->win_screen = SDL_SetVideoMode(x->win_w, x->win_h, 32, SDL_ANYFORMAT);
	  if (x->win_screen == NULL)
	    post("SDL_SetVideoMode error: %s", SDL_GetError());
	  else
	    {
	      // name window
	      SDL_WM_SetCaption("n_scop~", NULL);
	      // offset
	      x->win_p = (Uint32 *)x->win_screen->pixels;
	      x->win_ofs = x->win_screen->pitch / 4;
	      // colors
	      n_scop_color_all(x);
	    }
	}
    }
  // sdl quit
  else
    {
      SDL_Quit();
    }
}

//----------------------------------------------------------------------------//
static void n_scop_set(t_n_scop *x, t_floatarg ch, t_floatarg par, t_floatarg val)
{
  if (ch == -1)
    {
      if (par == 0)
	{
	  x->p_on = val;
	}
      else if (par == 1)
	{
	  x->p_sync_dc = val;
	}
      else if (par == 2)
	{
	  x->p_sync_ch = val;
	  AF_CLIP_MINMAX(0, x->channel-1, x->p_sync_ch)
	    }
      else if (par == 3)
	{
	  x->p_sync_treshold = val;
	}
      else if (par == 4)
	{
	  x->p_sync = val;
	}
      else if (par == 5)
	{
	  x->p_time = val;
	  AF_CLIP_MIN(1, x->p_time)
	    n_scop_calc_time(x);
	  n_scop_reset(x);
	}
      else if (par == 6)
	{
	  x->p_ms_smpl = val;
	  n_scop_calc_time(x);
	  n_scop_reset(x);
	}
      else if (par == 7)
	{
	  x->p_width = val;
	  AF_CLIP_MINMAX(A_MIN_WIDTH, A_MAX_WIDTH, x->p_width)
	    }
      else if (par == 8)
	{
	  x->p_height = val;
	  AF_CLIP_MINMAX(A_MIN_HEIGHT, A_MAX_HEIGHT, x->p_height)
	    }
      else if (par == 9)
	{
	  x->p_color_back = val * -1;
	  if (x->p_window)
	    n_scop_color_all(x);
	}
      else if (par == 10)
	{
	  x->p_color_grid = val * -1;
	  if (x->p_window)
	    n_scop_color_all(x);
	}
      else if (par == 11)
	{
	  x->p_color_grid_low = val * -1;
	  if (x->p_window)
	    n_scop_color_all(x);
	}
      else if (par == 12)
	{
	  x->p_window = val;
	  n_scop_calc_wh(x);
	  n_scop_calc_ch(x);
	  n_scop_calc_time(x);
	  n_scop_reset(x);
	  n_scop_window(x);
	}
      else if (par == 13)
	{
	  x->p_separate = val;
	  if (x->p_window)
	    n_scop_calc_ch(x);
	}
      else if (par == 14)
	{
	  x->p_grid_hor_on = val;
	}
    }
  else if (ch < x->channel)
    {
      if (par == 0)
	{
	  x->ch[(int)ch].on = val;
	  if (x->p_window)
	    n_scop_calc_ch(x);
	}
      else if (par == 1)
	{
	  x->ch[(int)ch].amp = val;
	}
      else if (par == 2)
	{
	  x->ch[(int)ch].color = val * -1;
	  if (x->p_window)
	    n_scop_color_all(x);
	}
    }
}

//----------------------------------------------------------------------------//
static void *n_scop_new(t_floatarg f)
{
  t_n_scop *x = (t_n_scop *)pd_new(n_scop_class);
  int i;
  x->channel = f;
  AF_CLIP_MINMAX(1, A_MAX_CHANNEL, x->channel);
  // init
  x->sr = 44100;
  x->p_window = 0;
  x->width = 1;
  // mem
  for (i = 0; i < x->channel; i++)
    {
      if (((x->ch[i].buf = malloc(sizeof(float) * A_MAX_SS)) == NULL) ||
	  ((x->ch[i].buf_max = malloc(sizeof(float) * A_MAX_WIDTH)) == NULL) ||
	  ((x->ch[i].buf_min = malloc(sizeof(float) * A_MAX_WIDTH)) == NULL))
	post("n_scop : error allocating memory");
    }
  // create inlets
  for (i = 0; i < x->channel - 1; i++)
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  // create outlets
  x->x_out = outlet_new(&x->x_obj, 0);

  return (x);
}

//----------------------------------------------------------------------------//
// input methods
//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//
static void n_scop_input_window(t_n_scop *x, t_floatarg f)
{
}

//----------------------------------------------------------------------------//
static void n_scop_free(t_n_scop *x)
{
  int i;
  for (i = 0; i < x->channel; i++)
    {
      free(x->ch[i].buf);
      free(x->ch[i].buf_max);
      free(x->ch[i].buf_min);
    }
  if (x->p_window)
    SDL_Quit();
}

//----------------------------------------------------------------------------//
void n_scop_tilde_setup(void)
{
  n_scop_class = class_new(gensym("n_scop~"), (t_newmethod)n_scop_new, (t_method)n_scop_free, sizeof(t_n_scop), 0, A_DEFFLOAT, 0);
  class_addmethod(n_scop_class, nullfn, gensym("signal"), 0);
  class_addmethod(n_scop_class, (t_method)n_scop_dsp, gensym("dsp"), 0);
  /* class_addmethod(n_scop_class, (t_method)n_scop_set, gensym("set"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0); */
  class_addmethod(n_scop_class, (t_method)n_scop_input_window, gensym("window"), A_DEFFLOAT, 0);
}

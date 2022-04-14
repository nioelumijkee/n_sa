#include <math.h>
#include "constant.h"

//----------------------------------------------------------------------------//
inline void arr_windowing(float *arr, 
                          int start, int end, int len,
                          char *type,
                          float coef)
{
  int i;
  float f,l,m;

  if     (strcmp("bartlett", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          if (l < 0)
            {
              l = 0.0 - l;
            }
          arr[i + start] = (1.0 - l) * coef;
        }
    }

  else if (strcmp("blackman", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          m = 0.5 * cos(l * C_PI);
          l = 0.08 * cos(l * C_2PI);
          arr[i + start] = (0.42 + l + m) * coef;
        }
    }

  else if (strcmp("connes", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          l = 1.0 - (l * l);
          arr[i + start] = l * l * coef;
        }
    }

  else if (strcmp("gaussian", type) == 0)
    {
      if (coef == 0)
        {
          for (i = 0; i < len; i++)
            {
              arr[i + start] = 0.0;
            }
        }
      else
        {
          f = len / 2.0;
          for (i = 0; i < len; i++)
            {
              l = (i - f) / f;
              l = l / coef;
              arr[i + start] = pow(2.0, (l * l * -1));
            }
        }
    }

  else if (strcmp("hanning", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          arr[i + start] = (cos(l * C_PI) + 1.0) * 0.5 * coef;
        }
    }

  else if (strcmp("hamming", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          arr[i + start] = (0.54 + (cos(l * C_PI) * 0.46)) * coef;
        }
    }
  
  else if (strcmp("lanczos", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          if (l == 0)
            {
              arr[i + start] = coef;
            }
          else
            {
              l = l * C_PI;
              arr[i + start] = (sin(l) / l) * coef;
            }
        }
    }

  else if (strcmp("sin", type) == 0)
    {
      for (i = 0; i < len; i++)
        {
          l = i  / (float)len;
          arr[i + start] = (sin(l * C_PI)) * coef;
        }
    }
  
  else if (strcmp("welch", type) == 0)
    {
      f = len / 2.0;
      for (i = 0; i < len; i++)
        {
          l = (i - f) / f;
          arr[i + start] = (1.0 - (l * l)) * coef;
        }
    }

  else // none
    {
      for (i = start; i < end; i++)
        {
          arr[i] = 1.0;
        }
    }
}

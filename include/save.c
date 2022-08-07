#include <stdlib.h>
#include <stdio.h>
#include <GL/gl.h>

//----------------------------------------------------------------------------//
int save_glcontent_to_file(int w, int h,
			    const char *filename)
{
  int i,j;
  int r,g,b;
  int ofs, ofs_inv;
  const int number_of_pixels = w * h * 3;
  unsigned char pixels[number_of_pixels];
  unsigned char pixels_inv[number_of_pixels];
  FILE *output_file = NULL;
  
  // get gl pixels
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_FRONT);
  glReadPixels(0, 0, 
	       w, h, 
	       GL_RGB, 
	       GL_UNSIGNED_BYTE, 
	       pixels);

  // reverse gl pixels
  for (i=0; i<h; i++)
    {
      ofs = i * w * 3;
      ofs_inv = (h - i - 1) * w * 3;
      for (j=0; j<w; j++)
	{
	  // reverse RGB -> BGR
	  r = j * 3;
	  g = r + 1;
	  b = r + 2;
	  pixels_inv[ofs_inv+r] = pixels[ofs+b]; // r->b
	  pixels_inv[ofs_inv+g] = pixels[ofs+g]; // g->g
	  pixels_inv[ofs_inv+b] = pixels[ofs+r]; // b->r
	}
    }

  // write to file TGA
  output_file = fopen(filename, "w");
  if (output_file != NULL)
    {
      unsigned char size_w0 = w / 256;
      unsigned char size_w1 = w - (size_w0 * 256);
      unsigned char size_h0 = h / 256;
      unsigned char size_h1 = h - (size_h0 * 256);
      unsigned char header[] = {
	0, // id 
	0, // color map
	2, // TrueColor (no map color/ no compression)

	0, // for color map
	0, 
	0, 
	0, 
	0,

	0, 0,             // hor
	size_h1, size_h0, // ver
	size_w1, size_w0, // width
	size_h1, size_h0, // height
	24,               // color bit depth
	32                // image descriptor(0100000)
      };
      fwrite(&header, sizeof(header), 1, output_file);
      fwrite(pixels_inv, number_of_pixels, 1, output_file);
      fclose(output_file);
      return(0);
    }
  else
    {
      return(1);
    }
}

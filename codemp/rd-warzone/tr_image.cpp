/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_image.c
#include "tr_local.h"
#include "glext.h"

// Calculates log2 of number.  
double log2d( double n )  
{
    // log(n)/log(2) is log2.  
    return log( n ) / (double)log( (double)2 );  
}

int log2(int val) {
	int answer;

	answer = 0;
	while ((val >>= 1) != 0) {
		answer++;
	}
	return answer;
}

static byte			 s_intensitytable[256];
static unsigned char s_gammatable[256];

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		1024
static	image_t*		hashTable[FILE_HASH_SIZE];

/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

#pragma omp parallel for schedule(dynamic) if(bufSize >= 512)
	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	char *name;
	int	minimize, maximize;
} textureMode_t;

textureMode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string ) {
	int		i;
	image_t	*glt;

	for ( i=0 ; i< 6 ; i++ ) {
		if ( !Q_stricmp( modes[i].name, string ) ) {
			break;
		}
	}


	if ( i == 6 ) {
		ri->Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;
	
	if ( r_ext_texture_filter_anisotropic->value > glConfig.maxTextureFilterAnisotropy )
	{
		ri->Cvar_SetValue ("r_ext_texture_filter_anisotropic", glConfig.maxTextureFilterAnisotropy);
	}

	// change all the existing mipmap texture objects
	for ( i = 0 ; i < tr.numImages ; i++ ) {
		glt = tr.images[ i ];
		if ( glt->flags & IMGFLAG_MIPMAP ) {
			GL_Bind (glt);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			
			if ( r_ext_texture_filter_anisotropic->value > 0.0f )
			{
				if ( glConfig.maxTextureFilterAnisotropy > 1.0f )
				{
					qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value);
				}
				else
				{
					qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
				}
			}
		}
	}
}

/*
===============
R_SumOfUsedImages
===============
*/
int R_SumOfUsedImages( void ) {
	int	total;
	int i;

	total = 0;
	for ( i = 0; i < tr.numImages; i++ ) {
		if ( tr.images[i]->frameUsed == tr.frameCount ) {
			total += tr.images[i]->uploadWidth * tr.images[i]->uploadHeight;
		}
	}

	return total;
}

static float GetReadableSize( int bytes, char **units )
{
	float result = bytes;
	*units = "b ";

	if (result >= 1024.0f)
	{
		result /= 1024.0f;
		*units = "kb";
	}

	if (result >= 1024.0f)
	{
		result /= 1024.0f;
		*units = "Mb";
	}

	if (result >= 1024.0f)
	{
		result /= 1024.0f;
		*units = "Gb";
	}

	return result;
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	int i;
	int estTotalSize = 0;
	char *sizeSuffix;

	ri->Printf(PRINT_ALL, "\n      -w-- -h-- type  -size- --name-------\n");

	for ( i = 0 ; i < tr.numImages ; i++ )
	{
		image_t *image = tr.images[i];
		char *format = "???? ";
		int estSize;

		estSize = image->uploadHeight * image->uploadWidth;

		switch(image->internalFormat)
		{
			case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
				format = "sDXT1";
				// 64 bits per 16 pixels, so 4 bits per pixel
				estSize /= 2;
				break;
			case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
				format = "sDXT5";
				// 128 bits per 16 pixels, so 1 byte per pixel
				break;
			case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
				format = "sBPTC";
				// 128 bits per 16 pixels, so 1 byte per pixel
				break;
			case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
				format = "LATC ";
				// 128 bits per 16 pixels, so 1 byte per pixel
				break;
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
				format = "DXT1 ";
				// 64 bits per 16 pixels, so 4 bits per pixel
				estSize /= 2;
				break;
			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				format = "DXT5 ";
				// 128 bits per 16 pixels, so 1 byte per pixel
				break;
			case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
				format = "BPTC ";
				// 128 bits per 16 pixels, so 1 byte per pixel
				break;
			case GL_RGB4_S3TC:
				format = "S3TC ";
				// same as DXT1?
				estSize /= 2;
				break;
			case GL_RGBA4:
			case GL_RGBA8:
			case GL_RGBA:
				format = "RGBA ";
				// 4 bytes per pixel
				estSize *= 4;
				break;
			case GL_LUMINANCE8:
			case GL_LUMINANCE16:
			case GL_LUMINANCE:
				format = "L    ";
				// 1 byte per pixel?
				break;
			case GL_RGB5:
			case GL_RGB8:
			case GL_RGB:
				format = "RGB  ";
				// 3 bytes per pixel?
				estSize *= 3;
				break;
			case GL_LUMINANCE8_ALPHA8:
			case GL_LUMINANCE16_ALPHA16:
			case GL_LUMINANCE_ALPHA:
				format = "LA   ";
				// 2 bytes per pixel?
				estSize *= 2;
				break;
			case GL_SRGB:
			case GL_SRGB8:
				format = "sRGB ";
				// 3 bytes per pixel?
				estSize *= 3;
				break;
			case GL_SRGB_ALPHA:
			case GL_SRGB8_ALPHA8:
				format = "sRGBA";
				// 4 bytes per pixel?
				estSize *= 4;
				break;
			case GL_SLUMINANCE:
			case GL_SLUMINANCE8:
				format = "sL   ";
				// 1 byte per pixel?
				break;
			case GL_SLUMINANCE_ALPHA:
			case GL_SLUMINANCE8_ALPHA8:
				format = "sLA  ";
				// 2 byte per pixel?
				estSize *= 2;
				break;
			case GL_DEPTH_COMPONENT24:
				format = "D24  ";
				break;
		}

		// mipmap adds about 50%
		if (image->flags & IMGFLAG_MIPMAP)
			estSize += estSize / 2;

		float printSize = GetReadableSize(estSize, &sizeSuffix);

		ri->Printf(PRINT_ALL, "%4i: %4ix%4i %s %7.2f%s %s\n", i, image->uploadWidth, image->uploadHeight, format, printSize, sizeSuffix, image->imgName);
		estTotalSize += estSize;
	}

	float printSize = GetReadableSize(estTotalSize, &sizeSuffix);

	ri->Printf (PRINT_ALL, " ---------\n");
	ri->Printf (PRINT_ALL, " approx %i bytes (%.2f%s)\n", estTotalSize, printSize, sizeSuffix);
	ri->Printf (PRINT_ALL, " %i total images\n\n", tr.numImages );
}

//=======================================================================

/*
================
ResampleTexture

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function 
before or after.
================
*/
static void ResampleTexture( byte *in, int inwidth, int inheight, byte *out,  
							int outwidth, int outheight ) {
	int		i, j;
	byte	*inrow, *inrow2;
	int		frac, fracstep;
	int		p1[2048], p2[2048];
	byte	*pix1, *pix2, *pix3, *pix4;

	if (outwidth>2048)
		ri->Error(ERR_DROP, "ResampleTexture: max width");
								
	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for ( i=0 ; i<outwidth ; i++ ) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ ) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++) {
		inrow = in + 4*inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + 4*inwidth*(int)((i+0.75)*inheight/outheight);
		for (j=0 ; j<outwidth ; j++) {
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			*out++ = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			*out++ = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			*out++ = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			*out++ = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

static void RGBAtoYCoCgA(const byte *in, byte *out, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y++)
	{
		const byte *inbyte  = in  + y * width * 4;
		byte       *outbyte = out + y * width * 4;

		for (x = 0; x < width; x++)
		{
			byte r, g, b, a, rb2;

			r = *inbyte++;
			g = *inbyte++;
			b = *inbyte++;
			a = *inbyte++;
			rb2 = (r + b) >> 1;

			*outbyte++ = (g + rb2) >> 1;       // Y  =  R/4 + G/2 + B/4
			*outbyte++ = (r - b + 256) >> 1;   // Co =  R/2       - B/2
			*outbyte++ = (g - rb2 + 256) >> 1; // Cg = -R/4 + G/2 - B/4
			*outbyte++ = a;
		}
	}
}

static void YCoCgAtoRGBA(const byte *in, byte *out, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y++)
	{
		const byte *inbyte  = in  + y * width * 4;
		byte       *outbyte = out + y * width * 4;

		for (x = 0; x < width; x++)
		{
			byte _Y, Co, Cg, a;

			_Y = *inbyte++;
			Co = *inbyte++;
			Cg = *inbyte++;
			a  = *inbyte++;

			*outbyte++ = CLAMP(_Y + Co - Cg,       0, 255); // R = Y + Co - Cg
			*outbyte++ = CLAMP(_Y      + Cg - 128, 0, 255); // G = Y + Cg
			*outbyte++ = CLAMP(_Y - Co - Cg + 256, 0, 255); // B = Y - Co - Cg
			*outbyte++ = a;
		}
	}
}


// uses a sobel filter to change a texture to a normal map
static void RGBAtoNormal(const byte *in, byte *out, int width, int height, qboolean clampToEdge)
{
	int x, y, max;

	// convert to heightmap, storing in alpha
	// same as converting to Y in YCoCg
	max = 1;
	for (y = 0; y < height; y++)
	{
		const byte *inbyte  = in  + y * width * 4;
		byte       *outbyte = out + y * width * 4 + 3;

		for (x = 0; x < width; x++)
		{
			byte result = (inbyte[0] >> 2) + (inbyte[1] >> 1) + (inbyte[2] >> 2);
			result = result * result / 255; // Make linear
			*outbyte = result;
			max = MAX(max, *outbyte);
			outbyte += 4;
			inbyte  += 4;
		}
	}

	// level out heights
	if (max < 255)
	{
		for (y = 0; y < height; y++)
		{
			byte *outbyte = out + y * width * 4 + 3;

			for (x = 0; x < width; x++)
			{
				*outbyte = *outbyte + (255 - max);
				outbyte += 4;
			}
		}
	}


	// now run sobel filter over height values to generate X and Y
	// then normalize
	for (y = 0; y < height; y++)
	{
		byte *outbyte = out + y * width * 4;

		for (x = 0; x < width; x++)
		{
			// 0 1 2
			// 3 4 5
			// 6 7 8

			byte s[9];
			int x2, y2, i;
			vec3_t normal;

			i = 0;
			for (y2 = -1; y2 <= 1; y2++)
			{
				int src_y = y + y2;

				if (clampToEdge)
				{
					src_y = CLAMP(src_y, 0, height - 1);
				}
				else
				{
					src_y = (src_y + height) % height;
				}


				for (x2 = -1; x2 <= 1; x2++)
				{
					int src_x = x + x2;

					if (clampToEdge)
					{
						src_x = CLAMP(src_x, 0, width - 1);
					}
					else
					{
						src_x = (src_x + width) % width;
					}

					s[i++] = *(out + (src_y * width + src_x) * 4 + 3);
				}
			}

			normal[0] =        s[0]            -     s[2]
						 + 2 * s[3]            - 2 * s[5]
						 +     s[6]            -     s[8];

			normal[1] =        s[0] + 2 * s[1] +     s[2]

						 -     s[6] - 2 * s[7] -     s[8];

			normal[2] = s[4] * 4;

			if (!VectorNormalize2(normal, normal))
			{
				VectorSet(normal, 0, 0, 1);
			}

			*outbyte++ = FloatToOffsetByte(normal[0]);
			*outbyte++ = FloatToOffsetByte(normal[1]);
			*outbyte++ = FloatToOffsetByte(normal[2]);
			outbyte++;
		}
	}
}

#define COPYSAMPLE(a,b) *(unsigned int *)(a) = *(unsigned int *)(b)

// based on Fast Curve Based Interpolation
// from Fast Artifacts-Free Image Interpolation (http://www.andreagiachetti.it/icbi/)
// assumes data has a 2 pixel thick border of clamped or wrapped data
// expects data to be a grid with even (0, 0), (2, 0), (0, 2), (2, 2) etc pixels filled
// only performs FCBI on specified component
static void DoFCBI(byte *in, byte *out, int width, int height, int component)
{
	int x, y;
	byte *outbyte, *inbyte;

	// copy in to out
	for (y = 2; y < height - 2; y += 2)
	{
		inbyte  = in  + (y * width + 2) * 4 + component;
		outbyte = out + (y * width + 2) * 4 + component;

		for (x = 2; x < width - 2; x += 2)
		{
			*outbyte = *inbyte;
			outbyte += 8;
			inbyte += 8;
		}
	}
	
	for (y = 3; y < height - 3; y += 2)
	{
		// diagonals
		//
		// NWp  - northwest interpolated pixel
		// NEp  - northeast interpolated pixel
		// NWd  - northwest first derivative
		// NEd  - northeast first derivative
		// NWdd - northwest second derivative
		// NEdd - northeast second derivative
		//
		// Uses these samples:
		//
		//         0
		//   - - a - b - -
		//   - - - - - - -
		//   c - d - e - f
		// 0 - - - - - - -
		//   g - h - i - j
		//   - - - - - - -
		//   - - k - l - -
		//
		// x+2 uses these samples:
		//
		//         0
		//   - - - - a - b - -
		//   - - - - - - - - -
		//   - - c - d - e - f
		// 0 - - - - - - - - -
		//   - - g - h - i - j
		//   - - - - - - - - -
		//   - - - - k - l - -
		//
		// so we can reuse 8 of them on next iteration
		//
		// a=b, c=d, d=e, e=f, g=h, h=i, i=j, k=l
		//
		// only b, f, j, and l need to be sampled on next iteration

		byte sa, sb, sc, sd, se, sf, sg, sh, si, sj, sk, sl;
		byte *line1, *line2, *line3, *line4;

		x = 3;

		// optimization one
		//                       SAMPLE2(sa, x-1, y-3);
		//SAMPLE2(sc, x-3, y-1); SAMPLE2(sd, x-1, y-1); SAMPLE2(se, x+1, y-1);
		//SAMPLE2(sg, x-3, y+1); SAMPLE2(sh, x-1, y+1); SAMPLE2(si, x+1, y+1);
		//                       SAMPLE2(sk, x-1, y+3);

		// optimization two
		line1 = in + ((y - 3) * width + (x - 1)) * 4 + component;
		line2 = in + ((y - 1) * width + (x - 3)) * 4 + component;
		line3 = in + ((y + 1) * width + (x - 3)) * 4 + component;
		line4 = in + ((y + 3) * width + (x - 1)) * 4 + component;

		//                                   COPYSAMPLE(sa, line1); line1 += 8;
		//COPYSAMPLE(sc, line2); line2 += 8; COPYSAMPLE(sd, line2); line2 += 8; COPYSAMPLE(se, line2); line2 += 8;
		//COPYSAMPLE(sg, line3); line3 += 8; COPYSAMPLE(sh, line3); line3 += 8; COPYSAMPLE(si, line3); line3 += 8;
		//                                   COPYSAMPLE(sk, line4); line4 += 8;

		                         sa = *line1; line1 += 8;
		sc = *line2; line2 += 8; sd = *line2; line2 += 8; se = *line2; line2 += 8;
		sg = *line3; line3 += 8; sh = *line3; line3 += 8; si = *line3; line3 += 8;
		                         sk = *line4; line4 += 8;

		outbyte = out + (y * width + x) * 4 + component;

		for ( ; x < width - 3; x += 2)
		{
			int NWd, NEd, NWp, NEp;

			// original
			//                       SAMPLE2(sa, x-1, y-3); SAMPLE2(sb, x+1, y-3);
			//SAMPLE2(sc, x-3, y-1); SAMPLE2(sd, x-1, y-1); SAMPLE2(se, x+1, y-1); SAMPLE2(sf, x+3, y-1);
			//SAMPLE2(sg, x-3, y+1); SAMPLE2(sh, x-1, y+1); SAMPLE2(si, x+1, y+1); SAMPLE2(sj, x+3, y+1);
			//                       SAMPLE2(sk, x-1, y+3); SAMPLE2(sl, x+1, y+3);

			// optimization one
			//SAMPLE2(sb, x+1, y-3);
			//SAMPLE2(sf, x+3, y-1);
			//SAMPLE2(sj, x+3, y+1);
			//SAMPLE2(sl, x+1, y+3);

			// optimization two
			//COPYSAMPLE(sb, line1); line1 += 8;
			//COPYSAMPLE(sf, line2); line2 += 8;
			//COPYSAMPLE(sj, line3); line3 += 8;
			//COPYSAMPLE(sl, line4); line4 += 8;

			sb = *line1; line1 += 8;
			sf = *line2; line2 += 8;
			sj = *line3; line3 += 8;
			sl = *line4; line4 += 8;

			NWp = sd + si;
			NEp = se + sh;
			NWd = abs(sd - si);
			NEd = abs(se - sh);

			if (NWd > 100 || NEd > 100 || abs(NWp-NEp) > 200)
			{
				if (NWd < NEd)
					*outbyte = NWp >> 1;
				else
					*outbyte = NEp >> 1;
			}
			else
			{
				int NWdd, NEdd;

				//NEdd = abs(sg + sd + sb - 3 * (se + sh) + sk + si + sf);
				//NWdd = abs(sa + se + sj - 3 * (sd + si) + sc + sh + sl);
				NEdd = abs(sg + sb - 3 * NEp + sk + sf + NWp);
				NWdd = abs(sa + sj - 3 * NWp + sc + sl + NEp);

				if (NWdd > NEdd)
					*outbyte = NWp >> 1;
				else
					*outbyte = NEp >> 1;
			}

			outbyte += 8;

			//                    COPYSAMPLE(sa, sb);
			//COPYSAMPLE(sc, sd); COPYSAMPLE(sd, se); COPYSAMPLE(se, sf);
			//COPYSAMPLE(sg, sh); COPYSAMPLE(sh, si); COPYSAMPLE(si, sj);
			//                    COPYSAMPLE(sk, sl);

			         sa = sb;
			sc = sd; sd = se; se = sf;
			sg = sh; sh = si; si = sj;
			         sk = sl;
		}
	}

	// hack: copy out to in again
	for (y = 3; y < height - 3; y += 2)
	{
		inbyte = out + (y * width + 3) * 4 + component;
		outbyte = in + (y * width + 3) * 4 + component;

		for (x = 3; x < width - 3; x += 2)
		{
			*outbyte = *inbyte;
			outbyte += 8;
			inbyte += 8;
		}
	}
	
	for (y = 2; y < height - 3; y++)
	{
		// horizontal & vertical
		//
		// hp  - horizontally interpolated pixel
		// vp  - vertically interpolated pixel
		// hd  - horizontal first derivative
		// vd  - vertical first derivative
		// hdd - horizontal second derivative
		// vdd - vertical second derivative
		// Uses these samples:
		//
		//       0
		//   - a - b -
		//   c - d - e
		// 0 - f - g -
		//   h - i - j
		//   - k - l -
		//
		// x+2 uses these samples:
		//
		//       0
		//   - - - a - b -
		//   - - c - d - e
		// 0 - - - f - g -
		//   - - h - i - j
		//   - - - k - l -
		//
		// so we can reuse 7 of them on next iteration
		//
		// a=b, c=d, d=e, f=g, h=i, i=j, k=l
		//
		// only b, e, g, j, and l need to be sampled on next iteration

		byte sa, sb, sc, sd, se, sf, sg, sh, si, sj, sk, sl;
		byte *line1, *line2, *line3, *line4, *line5;

		//x = (y + 1) % 2;
		x = (y + 1) % 2 + 2;
		
		// optimization one
		//            SAMPLE2(sa, x-1, y-2);
		//SAMPLE2(sc, x-2, y-1); SAMPLE2(sd, x,   y-1);
		//            SAMPLE2(sf, x-1, y  );
		//SAMPLE2(sh, x-2, y+1); SAMPLE2(si, x,   y+1);
		//            SAMPLE2(sk, x-1, y+2);

		line1 = in + ((y - 2) * width + (x - 1)) * 4 + component;
		line2 = in + ((y - 1) * width + (x - 2)) * 4 + component;
		line3 = in + ((y    ) * width + (x - 1)) * 4 + component;
		line4 = in + ((y + 1) * width + (x - 2)) * 4 + component;
		line5 = in + ((y + 2) * width + (x - 1)) * 4 + component;

		//                 COPYSAMPLE(sa, line1); line1 += 8;
		//COPYSAMPLE(sc, line2); line2 += 8; COPYSAMPLE(sd, line2); line2 += 8;
		//                 COPYSAMPLE(sf, line3); line3 += 8;
		//COPYSAMPLE(sh, line4); line4 += 8; COPYSAMPLE(si, line4); line4 += 8;
        //                 COPYSAMPLE(sk, line5); line5 += 8;

		             sa = *line1; line1 += 8;
		sc = *line2; line2 += 8; sd = *line2; line2 += 8;
		             sf = *line3; line3 += 8;
		sh = *line4; line4 += 8; si = *line4; line4 += 8;
		             sk = *line5; line5 += 8;

		outbyte = out + (y * width + x) * 4 + component;

		for ( ; x < width - 3; x+=2)
		{
			int hd, vd, hp, vp;

			//            SAMPLE2(sa, x-1, y-2); SAMPLE2(sb, x+1, y-2);
			//SAMPLE2(sc, x-2, y-1); SAMPLE2(sd, x,   y-1); SAMPLE2(se, x+2, y-1);
			//            SAMPLE2(sf, x-1, y  ); SAMPLE2(sg, x+1, y  );
			//SAMPLE2(sh, x-2, y+1); SAMPLE2(si, x,   y+1); SAMPLE2(sj, x+2, y+1);
			//            SAMPLE2(sk, x-1, y+2); SAMPLE2(sl, x+1, y+2);

			// optimization one
			//SAMPLE2(sb, x+1, y-2);
			//SAMPLE2(se, x+2, y-1);
			//SAMPLE2(sg, x+1, y  );
			//SAMPLE2(sj, x+2, y+1);
			//SAMPLE2(sl, x+1, y+2);

			//COPYSAMPLE(sb, line1); line1 += 8;
			//COPYSAMPLE(se, line2); line2 += 8;
			//COPYSAMPLE(sg, line3); line3 += 8;
			//COPYSAMPLE(sj, line4); line4 += 8;
			//COPYSAMPLE(sl, line5); line5 += 8;

			sb = *line1; line1 += 8;
			se = *line2; line2 += 8;
			sg = *line3; line3 += 8;
			sj = *line4; line4 += 8;
			sl = *line5; line5 += 8;

			hp = sf + sg; 
			vp = sd + si;
			hd = abs(sf - sg);
			vd = abs(sd - si);

			if (hd > 100 || vd > 100 || abs(hp-vp) > 200)
			{
				if (hd < vd)
					*outbyte = hp >> 1;
				else
					*outbyte = vp >> 1;
			}
			else
			{
				int hdd, vdd;

				//hdd = abs(sc[i] + sd[i] + se[i] - 3 * (sf[i] + sg[i]) + sh[i] + si[i] + sj[i]);
				//vdd = abs(sa[i] + sf[i] + sk[i] - 3 * (sd[i] + si[i]) + sb[i] + sg[i] + sl[i]);

				hdd = abs(sc + se - 3 * hp + sh + sj + vp);
				vdd = abs(sa + sk - 3 * vp + sb + sl + hp);

				if (hdd > vdd)
					*outbyte = hp >> 1;
				else 
					*outbyte = vp >> 1;
			}

			outbyte += 8;

			//          COPYSAMPLE(sa, sb);
			//COPYSAMPLE(sc, sd); COPYSAMPLE(sd, se);
			//          COPYSAMPLE(sf, sg);
			//COPYSAMPLE(sh, si); COPYSAMPLE(si, sj);
			//          COPYSAMPLE(sk, sl);
			    sa = sb;
			sc = sd; sd = se;
			    sf = sg;
			sh = si; si = sj;
			    sk = sl;
		}
	}
}

// Similar to FCBI, but throws out the second order derivatives for speed
static void DoFCBIQuick(byte *in, byte *out, int width, int height, int component)
{
	int x, y;
	byte *outbyte, *inbyte;

	// copy in to out
	for (y = 2; y < height - 2; y += 2)
	{
		inbyte  = in  + (y * width + 2) * 4 + component;
		outbyte = out + (y * width + 2) * 4 + component;

		for (x = 2; x < width - 2; x += 2)
		{
			*outbyte = *inbyte;
			outbyte += 8;
			inbyte += 8;
		}
	}

	for (y = 3; y < height - 4; y += 2)
	{
		byte sd, se, sh, si;
		byte *line2, *line3;

		x = 3;

		line2 = in + ((y - 1) * width + (x - 1)) * 4 + component;
		line3 = in + ((y + 1) * width + (x - 1)) * 4 + component;

		sd = *line2; line2 += 8;
		sh = *line3; line3 += 8;

		outbyte = out + (y * width + x) * 4 + component;

		for ( ; x < width - 4; x += 2)
		{
			int NWd, NEd, NWp, NEp;

			se = *line2; line2 += 8;
			si = *line3; line3 += 8;

			NWp = sd + si;
			NEp = se + sh;
			NWd = abs(sd - si);
			NEd = abs(se - sh);

			if (NWd < NEd)
				*outbyte = NWp >> 1;
			else
				*outbyte = NEp >> 1;

			outbyte += 8;

			sd = se;
			sh = si;
		}
	}

	// hack: copy out to in again
	for (y = 3; y < height - 3; y += 2)
	{
		inbyte  = out + (y * width + 3) * 4 + component;
		outbyte = in  + (y * width + 3) * 4 + component;

		for (x = 3; x < width - 3; x += 2)
		{
			*outbyte = *inbyte;
			outbyte += 8;
			inbyte += 8;
		}
	}
	
	for (y = 2; y < height - 3; y++)
	{
		byte sd, sf, sg, si;
		byte *line2, *line3, *line4;

		x = (y + 1) % 2 + 2;

		line2 = in + ((y - 1) * width + (x    )) * 4 + component;
		line3 = in + ((y    ) * width + (x - 1)) * 4 + component;
		line4 = in + ((y + 1) * width + (x    )) * 4 + component;

		outbyte = out + (y * width + x) * 4 + component;

		sf = *line3; line3 += 8;

		for ( ; x < width - 3; x+=2)
		{
			int hd, vd, hp, vp;

			sd = *line2; line2 += 8;
			sg = *line3; line3 += 8;
			si = *line4; line4 += 8;
			
			hp = sf + sg; 
			vp = sd + si;
			hd = abs(sf - sg);
			vd = abs(sd - si);

			if (hd < vd)
				*outbyte = hp >> 1;
			else
				*outbyte = vp >> 1;

			outbyte += 8;

			sf = sg;
		}
	}
}

// Similar to DoFCBIQuick, but just takes the average instead of checking derivatives
// as well, this operates on all four components
static void DoLinear(byte *in, byte *out, int width, int height)
{
	int x, y, i;
	byte *outbyte, *inbyte;

	// copy in to out
	for (y = 2; y < height - 2; y += 2)
	{
		x = 2;

		inbyte  = in  + (y * width + x) * 4;
		outbyte = out + (y * width + x) * 4;

		for ( ; x < width - 2; x += 2)
		{
			COPYSAMPLE(outbyte, inbyte);
			outbyte += 8;
			inbyte += 8;
		}
	}

	for (y = 1; y < height - 1; y += 2)
	{
		byte sd[4] = {0}, se[4] = {0}, sh[4] = {0}, si[4] = {0};
		byte *line2, *line3;

		x = 1;

		line2 = in + ((y - 1) * width + (x - 1)) * 4;
		line3 = in + ((y + 1) * width + (x - 1)) * 4;

		COPYSAMPLE(sd, line2); line2 += 8;
		COPYSAMPLE(sh, line3); line3 += 8;

		outbyte = out + (y * width + x) * 4;

		for ( ; x < width - 1; x += 2)
		{
			COPYSAMPLE(se, line2); line2 += 8;
			COPYSAMPLE(si, line3); line3 += 8;

			for (i = 0; i < 4; i++)
			{	
				*outbyte++ = (sd[i] + si[i] + se[i] + sh[i]) >> 2;
			}

			outbyte += 4;

			COPYSAMPLE(sd, se);
			COPYSAMPLE(sh, si);
		}
	}

	// hack: copy out to in again
	for (y = 1; y < height - 1; y += 2)
	{
		x = 1;

		inbyte  = out + (y * width + x) * 4;
		outbyte = in  + (y * width + x) * 4;

		for ( ; x < width - 1; x += 2)
		{
			COPYSAMPLE(outbyte, inbyte);
			outbyte += 8;
			inbyte += 8;
		}
	}
	
	for (y = 1; y < height - 1; y++)
	{
		byte sd[4], sf[4], sg[4], si[4];
		byte *line2, *line3, *line4;

		x = y % 2 + 1;

		line2 = in + ((y - 1) * width + (x    )) * 4;
		line3 = in + ((y    ) * width + (x - 1)) * 4;
		line4 = in + ((y + 1) * width + (x    )) * 4;

		COPYSAMPLE(sf, line3); line3 += 8;

		outbyte = out + (y * width + x) * 4;

		for ( ; x < width - 1; x += 2)
		{
			COPYSAMPLE(sd, line2); line2 += 8;
			COPYSAMPLE(sg, line3); line3 += 8;
			COPYSAMPLE(si, line4); line4 += 8;

			for (i = 0; i < 4; i++)
			{
				*outbyte++ = (sf[i] + sg[i] + sd[i] + si[i]) >> 2;
			}

			outbyte += 4;

			COPYSAMPLE(sf, sg);
		}
	}
}


static void ExpandHalfTextureToGrid( byte *data, int width, int height)
{
#pragma omp parallel for schedule(dynamic) if(height > 32)
	for (int y = height / 2; y > 0; y--)
	{
		byte *outbyte = data + ((y * 2 - 1) * (width)     - 2) * 4;
		byte *inbyte  = data + (y           * (width / 2) - 1) * 4;

		for (int x = width / 2; x > 0; x--)
		{
			COPYSAMPLE(outbyte, inbyte);

			outbyte -= 8;
			inbyte -= 4;
		}
	}
}

static void FillInNormalizedZ(const byte *in, byte *out, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y++)
	{
		const byte *inbyte  = in  + y * width * 4;
		byte       *outbyte = out + y * width * 4;

		for (x = 0; x < width; x++)
		{
			byte nx, ny, nz, h;
			float fnx, fny, fll, fnz;

			nx = *inbyte++;
			ny = *inbyte++;
			inbyte++;
			h  = *inbyte++;

			fnx = OffsetByteToFloat(nx);
			fny = OffsetByteToFloat(ny);
			fll = 1.0f - fnx * fnx - fny * fny;
			if (fll >= 0.0f)
				fnz = (float)sqrt(fll);
			else
				fnz = 0.0f;

			nz = FloatToOffsetByte(fnz);

			*outbyte++ = nx;
			*outbyte++ = ny;
			*outbyte++ = nz;
			*outbyte++ = h;
		}
	}
}


// size must be even
#define WORKBLOCK_SIZE     128
#define WORKBLOCK_BORDER   4
#define WORKBLOCK_REALSIZE (WORKBLOCK_SIZE + WORKBLOCK_BORDER * 2)

// assumes that data has already been expanded into a 2x2 grid
static void FCBIByBlock(byte *data, int width, int height, qboolean clampToEdge, qboolean normalized)
{
	byte workdata[WORKBLOCK_REALSIZE * WORKBLOCK_REALSIZE * 4];
	byte outdata[WORKBLOCK_REALSIZE * WORKBLOCK_REALSIZE * 4];
	byte *inbyte, *outbyte;
	int x, y;
	int srcx, srcy;

	ExpandHalfTextureToGrid(data, width, height);

	for (y = 0; y < height; y += WORKBLOCK_SIZE)
	{
		for (x = 0; x < width; x += WORKBLOCK_SIZE)
		{
			int x2, y2;
			int workwidth, workheight, fullworkwidth, fullworkheight;

			workwidth =  MIN(WORKBLOCK_SIZE, width  - x);
			workheight = MIN(WORKBLOCK_SIZE, height - y);

			fullworkwidth =  workwidth  + WORKBLOCK_BORDER * 2;
			fullworkheight = workheight + WORKBLOCK_BORDER * 2;

			//memset(workdata, 0, WORKBLOCK_REALSIZE * WORKBLOCK_REALSIZE * 4);

			// fill in work block
			for (y2 = 0; y2 < fullworkheight; y2 += 2)
			{
				srcy = y + y2 - WORKBLOCK_BORDER;

				if (clampToEdge)
				{
					srcy = CLAMP(srcy, 0, height - 2);
				}
				else
				{
					srcy = (srcy + height) % height;
				}

				outbyte = workdata + y2   * fullworkwidth * 4;
				inbyte  = data     + srcy * width         * 4;		

				for (x2 = 0; x2 < fullworkwidth; x2 += 2)
				{
					srcx = x + x2 - WORKBLOCK_BORDER;

					if (clampToEdge)
					{
						srcx = CLAMP(srcx, 0, width - 2);
					}
					else
					{
						srcx = (srcx + width) % width;
					}

					COPYSAMPLE(outbyte, inbyte + srcx * 4);
					outbyte += 8;
				}
			}

			// submit work block
			DoLinear(workdata, outdata, fullworkwidth, fullworkheight);

			if (!normalized)
			{
				switch (r_imageUpsampleType->integer)
				{
					case 0:
						break;
					case 1:
						DoFCBIQuick(workdata, outdata, fullworkwidth, fullworkheight, 0);
						break;
					case 2:
					default:
						DoFCBI(workdata, outdata, fullworkwidth, fullworkheight, 0);
						break;
				}
			}
			else
			{
				switch (r_imageUpsampleType->integer)
				{
					case 0:
						break;
					case 1:
						DoFCBIQuick(workdata, outdata, fullworkwidth, fullworkheight, 0);
						DoFCBIQuick(workdata, outdata, fullworkwidth, fullworkheight, 1);
						break;
					case 2:
					default:
						DoFCBI(workdata, outdata, fullworkwidth, fullworkheight, 0);
						DoFCBI(workdata, outdata, fullworkwidth, fullworkheight, 1);
						break;
				}
			}

			// copy back work block
			for (y2 = 0; y2 < workheight; y2++)
			{
				inbyte = outdata + ((y2 + WORKBLOCK_BORDER) * fullworkwidth + WORKBLOCK_BORDER) * 4;
				outbyte = data +   ((y + y2)                * width         + x)                * 4;
				for (x2 = 0; x2 < workwidth; x2++)
				{
					COPYSAMPLE(outbyte, inbyte);
					outbyte += 4;
					inbyte += 4;
				}
			}
		}
	}
}
#undef COPYSAMPLE

/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
void R_LightScaleTexture (byte *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( only_gamma )
	{
		if ( !glConfig.deviceSupportsGamma )
		{
			int		i, c;
			byte	*p;

			p = in;

			c = inwidth*inheight;

			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = in;

		c = inwidth*inheight;

		if ( glConfig.deviceSupportsGamma )
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}


/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( byte *in, int inWidth, int inHeight ) {
	int			i, j, k;
	byte		*outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = (unsigned int *)ri->Hunk_AllocateTempMemory( outWidth * outHeight * 4 );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (byte *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ ) {
				total = 
					1 * (&in[ 4*(((i*2-1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask)) ])[k] +
					2 * (&in[ 4*(((i*2-1)&inHeightMask)*inWidth + ((j*2  )&inWidthMask)) ])[k] +
					2 * (&in[ 4*(((i*2-1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask)) ])[k] +
					1 * (&in[ 4*(((i*2-1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask)) ])[k] +

					2 * (&in[ 4*(((i*2  )&inHeightMask)*inWidth + ((j*2-1)&inWidthMask)) ])[k] +
					4 * (&in[ 4*(((i*2  )&inHeightMask)*inWidth + ((j*2  )&inWidthMask)) ])[k] +
					4 * (&in[ 4*(((i*2  )&inHeightMask)*inWidth + ((j*2+1)&inWidthMask)) ])[k] +
					2 * (&in[ 4*(((i*2  )&inHeightMask)*inWidth + ((j*2+2)&inWidthMask)) ])[k] +

					2 * (&in[ 4*(((i*2+1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask)) ])[k] +
					4 * (&in[ 4*(((i*2+1)&inHeightMask)*inWidth + ((j*2  )&inWidthMask)) ])[k] +
					4 * (&in[ 4*(((i*2+1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask)) ])[k] +
					2 * (&in[ 4*(((i*2+1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask)) ])[k] +

					1 * (&in[ 4*(((i*2+2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask)) ])[k] +
					2 * (&in[ 4*(((i*2+2)&inHeightMask)*inWidth + ((j*2  )&inWidthMask)) ])[k] +
					2 * (&in[ 4*(((i*2+2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask)) ])[k] +
					1 * (&in[ 4*(((i*2+2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask)) ])[k];
				outpix[k] = total / 36;
			}
		}
	}

	Com_Memcpy( in, temp, outWidth * outHeight * 4 );
	ri->Hunk_FreeTempMemory( temp );
}


static void R_MipMapsRGB( byte *in, int inWidth, int inHeight)
{
	int			i, j, k;
	int			outWidth, outHeight;
	byte		*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = (byte *)ri->Hunk_AllocateTempMemory( outWidth * outHeight * 4 );

#pragma omp parallel for schedule(dynamic) if(outHeight > 64)
	for ( i = 0 ; i < outHeight ; i++ ) {
		byte *outbyte = temp + (  i          * outWidth ) * 4;
		byte *inbyte1 = in   + (  i * 2      * inWidth  ) * 4;
		byte *inbyte2 = in   + ( (i * 2 + 1) * inWidth  ) * 4;
		for ( j = 0 ; j < outWidth ; j++ ) {
			for ( k = 0 ; k < 3 ; k++ ) {
				float total, current;

				current = ByteToFloat(inbyte1[0]); total  = sRGBtoRGB((double)current);
				current = ByteToFloat(inbyte1[4]); total += sRGBtoRGB((double)current);
				current = ByteToFloat(inbyte2[0]); total += sRGBtoRGB((double)current);
				current = ByteToFloat(inbyte2[4]); total += sRGBtoRGB((double)current);

				total *= 0.25f;

				inbyte1++;
				inbyte2++;

				current = RGBtosRGB(total);
				*outbyte++ = FloatToByte(current);
			}
			*outbyte++ = (inbyte1[0] + inbyte1[4] + inbyte2[0] + inbyte2[4]) >> 2;
			inbyte1 += 5;
			inbyte2 += 5;
		}
	}

	Com_Memcpy( in, temp, outWidth * outHeight * 4 );
	ri->Hunk_FreeTempMemory( temp );
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap (byte *in, int width, int height) {
	int		i, j;
	byte	*out;
	int		row;

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( in, width, height );
		return;
	}

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
			out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
			out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
		}
	}
}


static void R_MipMapLuminanceAlpha (const byte *in, byte *out, int width, int height)
{
	int  i, j, row;

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest

		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = 
			out[1] = 
			out[2] = (in[0] + in[4]) >> 1;
			out[3] = (in[3] + in[7]) >> 1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = 
			out[1] = 
			out[2] = (in[0] + in[4] + in[row  ] + in[row+4]) >> 2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7]) >> 2;
		}
	}

}


static void R_MipMapNormalHeight (const byte *in, byte *out, int width, int height, qboolean swizzle)
{
	int		i, j;
	int		row;
	int sx = swizzle ? 3 : 0;
	int sa = swizzle ? 0 : 3;

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	width >>= 1;
	height >>= 1;
	
	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			vec3_t v;

			v[0] =  OffsetByteToFloat(in[sx      ]);
			v[1] =  OffsetByteToFloat(in[       1]);
			v[2] =  OffsetByteToFloat(in[       2]);

			v[0] += OffsetByteToFloat(in[sx    +4]);
			v[1] += OffsetByteToFloat(in[       5]);
			v[2] += OffsetByteToFloat(in[       6]);

			v[0] += OffsetByteToFloat(in[sx+row  ]);
			v[1] += OffsetByteToFloat(in[   row+1]);
			v[2] += OffsetByteToFloat(in[   row+2]);

			v[0] += OffsetByteToFloat(in[sx+row+4]);
			v[1] += OffsetByteToFloat(in[   row+5]);
			v[2] += OffsetByteToFloat(in[   row+6]);

			VectorNormalizeFast(v);

			//v[0] *= 0.25f;
			//v[1] *= 0.25f;
			//v[2] = 1.0f - v[0] * v[0] - v[1] * v[1];
			//v[2] = sqrt(MAX(v[2], 0.0f));

			out[sx] = FloatToOffsetByte(v[0]);
			out[1 ] = FloatToOffsetByte(v[1]);
			out[2 ] = FloatToOffsetByte(v[2]);
			out[sa] = MAX(MAX(in[sa], in[sa+4]), MAX(in[sa+row], in[sa+row+4]));
		}
	}
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture( byte *data, int pixelCount, byte blend[4] ) {
	int		i;
	int		inverseAlpha;
	int		premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}

byte	mipBlendColors[16][4] = {
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};

static void RawImage_SwizzleRA( byte *data, int width, int height )
{
	int i;
	byte *ptr = data, swap;

	for (i=0; i<width*height; i++, ptr+=4)
	{
		// swap red and alpha
		swap = ptr[0];
		ptr[0] = ptr[3];
		ptr[3] = swap;
	}
}


/*
===============
RawImage_ScaleToPower2

===============
*/
static void RawImage_ScaleToPower2( byte **data, int *inout_width, int *inout_height, int *inout_scaled_width, int *inout_scaled_height, imgType_t type, int flags, byte **resampledBuffer)
{
	int width =         *inout_width;
	int height =        *inout_height;
	int scaled_width;
	int scaled_height;
	qboolean picmip = (qboolean)(flags & IMGFLAG_PICMIP);
	qboolean mipmap = (qboolean)(flags & IMGFLAG_MIPMAP);
	qboolean clampToEdge = (qboolean)(flags & IMGFLAG_CLAMPTOEDGE);

	//
	// convert to exact power of 2 sizes
	//
	if (!mipmap)
	{
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		scaled_width = NextPowerOfTwo(width);
		scaled_height = NextPowerOfTwo(height);
	}

	if ( r_roundImagesDown->integer && scaled_width > width )
		scaled_width >>= 1;
	if ( r_roundImagesDown->integer && scaled_height > height )
		scaled_height >>= 1;

	if ( picmip && data && resampledBuffer && r_imageUpsample->integer && 
	     scaled_width < r_imageUpsampleMaxSize->integer && scaled_height < r_imageUpsampleMaxSize->integer)
	{
		int finalwidth, finalheight;
		//int startTime, endTime;

		//startTime = ri->Milliseconds();

		finalwidth = scaled_width << r_imageUpsample->integer;
		finalheight = scaled_height << r_imageUpsample->integer;

		while ( finalwidth > r_imageUpsampleMaxSize->integer
			|| finalheight > r_imageUpsampleMaxSize->integer ) {
			finalwidth >>= 1;
			finalheight >>= 1;
		}

		while ( finalwidth > glConfig.maxTextureSize
			|| finalheight > glConfig.maxTextureSize ) {
			finalwidth >>= 1;
			finalheight >>= 1;
		}

		*resampledBuffer = (byte *)ri->Hunk_AllocateTempMemory( finalwidth * finalheight * 4 );

		if (scaled_width != width || scaled_height != height)
		{
			ResampleTexture (*data, width, height, *resampledBuffer, scaled_width, scaled_height);
		}
		else
		{
			byte *inbyte, *outbyte;
			int i;

			inbyte = *data;
			outbyte = *resampledBuffer;

			for (i = width * height * 4; i > 0; i--)
			{
				*outbyte++ = *inbyte++;
			}
		}

		if (type == IMGTYPE_COLORALPHA)
			RGBAtoYCoCgA(*resampledBuffer, *resampledBuffer, scaled_width, scaled_height);

		while (scaled_width < finalwidth || scaled_height < finalheight)
		{
			scaled_width <<= 1;
			scaled_height <<= 1;

			FCBIByBlock(*resampledBuffer, scaled_width, scaled_height, clampToEdge, (qboolean)(type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT));
		}

		if (type == IMGTYPE_COLORALPHA)
		{
			YCoCgAtoRGBA(*resampledBuffer, *resampledBuffer, scaled_width, scaled_height);
		}
		else if (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT)
		{
			FillInNormalizedZ(*resampledBuffer, *resampledBuffer, scaled_width, scaled_height);
		}


		//endTime = ri->Milliseconds();

		//ri->Printf(PRINT_ALL, "upsampled %dx%d to %dx%d in %dms\n", width, height, scaled_width, scaled_height, endTime - startTime);

		*data = *resampledBuffer;
		width = scaled_width;
		height = scaled_height;
	}
	else if ( scaled_width != width || scaled_height != height ) {
		if (data && resampledBuffer)
		{
			*resampledBuffer = (byte *)ri->Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
			ResampleTexture (*data, width, height, *resampledBuffer, scaled_width, scaled_height);
			*data = *resampledBuffer;
		}
		width = scaled_width;
		height = scaled_height;
	}

	//
	// perform optional picmip operation
	//
	if ( picmip ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	*inout_width         = width;
	*inout_height        = height;
	*inout_scaled_width  = scaled_width;
	*inout_scaled_height = scaled_height;
}


static qboolean RawImage_HasAlpha(const byte *scan, int numPixels)
{
	int i;

	if (!scan)
		return qtrue;

	for ( i = 0; i < numPixels; i++ )
	{
		if ( scan[i*4 + 3] != 255 ) 
		{
			return qtrue;
		}
	}

	return qfalse;
}

static GLenum RawImage_GetFormat(const byte *data, int numPixels, qboolean lightMap, imgType_t type, int flags)
{
	int samples = 3;
	GLenum internalFormat = GL_RGB;
	qboolean forceNoCompression = (qboolean)(flags & IMGFLAG_NO_COMPRESSION);
	qboolean normalmap = (qboolean)(type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT);

	if(normalmap)
	{
		if ((!RawImage_HasAlpha(data, numPixels) || (type == IMGTYPE_NORMAL)) && !forceNoCompression && (glRefConfig.textureCompression & TCR_LATC))
		{
			internalFormat = GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
		}
		else
		{
			if ( !forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB )
			{
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			}
			else if ( r_texturebits->integer == 16 )
			{
				internalFormat = GL_RGBA4;
			}
			else if ( r_texturebits->integer == 32 )
			{
				internalFormat = GL_RGBA8;
			}
			else
			{
				internalFormat = GL_RGBA;
			}
		}
	}
	else if(lightMap)
	{
		if(r_greyscale->integer)
			internalFormat = GL_LUMINANCE;
		else
			internalFormat = GL_RGBA;
	}
	else
	{
		if (RawImage_HasAlpha(data, numPixels))
		{
			samples = 4;
		}

		// select proper internal format
		if ( samples == 3 )
		{
			if(r_greyscale->integer)
			{
				if(r_texturebits->integer == 16)
					internalFormat = GL_LUMINANCE8;
				else if(r_texturebits->integer == 32)
					internalFormat = GL_LUMINANCE16;
				else
					internalFormat = GL_LUMINANCE;
			}
			else
			{
				if ( !forceNoCompression && (glRefConfig.textureCompression & TCR_BPTC) )
				{
					internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
				}
				else if ( !forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB )
				{
					internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				}
				else if ( !forceNoCompression && glConfig.textureCompression == TC_S3TC )
				{
					internalFormat = GL_RGB4_S3TC;
				}
				else if ( r_texturebits->integer == 16 )
				{
					internalFormat = GL_RGB5;
				}
				else if ( r_texturebits->integer == 32 )
				{
					internalFormat = GL_RGB8;
				}
				else
				{
					internalFormat = GL_RGB;
				}
			}
		}
		else if ( samples == 4 )
		{
			if(r_greyscale->integer)
			{
				if(r_texturebits->integer == 16)
					internalFormat = GL_LUMINANCE8_ALPHA8;
				else if(r_texturebits->integer == 32)
					internalFormat = GL_LUMINANCE16_ALPHA16;
				else
					internalFormat = GL_LUMINANCE_ALPHA;
			}
			else
			{
				if ( !forceNoCompression && (glRefConfig.textureCompression & TCR_BPTC) )
				{
					internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
				}
				else if ( !forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB )
				{
					internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				}
				else if ( r_texturebits->integer == 16 )
				{
					internalFormat = GL_RGBA4;
				}
				else if ( r_texturebits->integer == 32 )
				{
					internalFormat = GL_RGBA8;
				}
				else
				{
					internalFormat = GL_RGBA;
				}
			}
		}

		if (flags & IMGFLAG_SRGB)
		{
			switch(internalFormat)
			{
				case GL_RGB:
					internalFormat = GL_SRGB;
					break;

				case GL_RGB4:
				case GL_RGB5:
				case GL_RGB8:
					internalFormat = GL_SRGB8;
					break;

				case GL_RGBA:
					internalFormat = GL_SRGB_ALPHA;
					break;

				case GL_RGBA4:
				case GL_RGBA8:
					internalFormat = GL_SRGB8_ALPHA8;
					break;

				case GL_LUMINANCE:
					internalFormat = GL_SLUMINANCE;
					break;

				case GL_LUMINANCE8:
				case GL_LUMINANCE16:
					internalFormat = GL_SLUMINANCE8;
					break;

				case GL_LUMINANCE_ALPHA:
					internalFormat = GL_SLUMINANCE_ALPHA;
					break;

				case GL_LUMINANCE8_ALPHA8:
				case GL_LUMINANCE16_ALPHA16:
					internalFormat = GL_SLUMINANCE8_ALPHA8;
					break;

				case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
					internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
					break;

				case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
					internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
					break;

				case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
					internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
					break;
			}
		}
	}

	return internalFormat;
}

static int CalcNumMipmapLevels ( int width, int height )
{
	return static_cast<int>(ceil ((double)log2 (max (width, height))) + 1);
}

void RawImage_UploadTexture( byte *data, int x, int y, int width, int height, GLenum internalFormat, imgType_t type, int flags, qboolean subtexture )
{
	int dataFormat, dataType;

	switch(internalFormat)
	{
		case GL_DEPTH_COMPONENT:
		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32:
			dataFormat = GL_DEPTH_COMPONENT;
			dataType = GL_UNSIGNED_BYTE;
			break;
		case GL_RGBA16F:
			dataFormat = GL_RGBA;
			dataType = GL_HALF_FLOAT;
			break;
		default:
			dataFormat = GL_RGBA;
			dataType = GL_UNSIGNED_BYTE;
			break;
	}

	if ( subtexture )
	{
		qglTexSubImage2D (GL_TEXTURE_2D, 0, x, y, width, height, dataFormat, dataType, data);
	}
	else
	{
		if ( !data )
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, dataType, data );
		}
		else if ( glRefConfig.immutableTextures && !(flags & IMGFLAG_MUTABLE) )
		{
			int numLevels = (flags & IMGFLAG_MIPMAP) ? CalcNumMipmapLevels (width, height) : 1;

			qglTexStorage2D (GL_TEXTURE_2D, numLevels, internalFormat, width, height);
			qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, dataFormat, dataType, data);
		}
		else
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, dataType, data );
		}
	}

	if (flags & IMGFLAG_MIPMAP)
	{
		int miplevel = 0;
		
		while (width > 1 || height > 1)
		{
			if (data)
			{
				if (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT)
				{
					if (internalFormat == GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT)
					{
						R_MipMapLuminanceAlpha( data, data, width, height );
					}
					else
					{
						R_MipMapNormalHeight( data, data, width, height, qtrue);
					}
				}
				else if (flags & IMGFLAG_SRGB)
				{
					R_MipMapsRGB( data, width, height );
				}
				else
				{
					R_MipMap( data, width, height );
				}
			}
			
			width >>= 1;
			height >>= 1;
			if (width < 1)
				width = 1;
			if (height < 1)
				height = 1;
			miplevel++;

			if ( data && r_colorMipLevels->integer )
				R_BlendOverTexture( (byte *)data, width * height, mipBlendColors[miplevel] );

			if ( subtexture )
			{
				x >>= 1;
				y >>= 1;
				qglTexSubImage2D( GL_TEXTURE_2D, miplevel, x, y, width, height, dataFormat, dataType, data );
			}
			else
			{
				if ( !data )
				{
					qglTexImage2D (GL_TEXTURE_2D, miplevel, internalFormat, width, height, 0, dataFormat, dataType, data );
				}
				else if ( glRefConfig.immutableTextures && !(flags & IMGFLAG_MUTABLE) )
				{
					qglTexSubImage2D (GL_TEXTURE_2D, miplevel, 0, 0, width, height, dataFormat, dataType, data );
				}
				else
				{
					qglTexImage2D (GL_TEXTURE_2D, miplevel, internalFormat, width, height, 0, dataFormat, dataType, data );
				}
			}
		}
	}
}

static bool IsPowerOfTwo ( int i )
{
	return (i & (i - 1)) == 0;
}

/*
===============
Upload32

===============
*/
extern qboolean charSet;
void Upload32( byte *data, int width, int height, imgType_t type, int flags,
	qboolean lightMap, GLenum internalFormat, int *pUploadWidth, int *pUploadHeight)
{
	byte		*scaledBuffer = NULL;
	byte		*resampledBuffer = NULL;
	int			scaled_width = width;
	int			scaled_height = height;
	int			i, c;
	byte		*scan;

	if ( !IsPowerOfTwo (width) || !IsPowerOfTwo (height) )
	{
		RawImage_ScaleToPower2(&data, &width, &height, &scaled_width, &scaled_height, type, flags, &resampledBuffer);
	}

	scaledBuffer = (byte *)ri->Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );

	//
	// scan the texture for each channel's max values
	// and verify if the alpha channel is being used or not
	//
	c = width*height;
	scan = data;
	
	if( r_greyscale->integer )
	{
#pragma omp parallel for schedule(dynamic) if(c >= 64)
		for ( i = 0; i < c; i++ )
		{
			byte luma = LUMA(scan[i*4], scan[i*4 + 1], scan[i*4 + 2]);
			scan[i*4] = luma;
			scan[i*4 + 1] = luma;
			scan[i*4 + 2] = luma;
		}
	}
	else if( r_greyscale->value )
	{
#pragma omp parallel for schedule(dynamic) if(c >= 32)
		for ( i = 0; i < c; i++ )
		{
			float luma = LUMA(scan[i*4], scan[i*4 + 1], scan[i*4 + 2]);
			scan[i*4] = LERP(scan[i*4], luma, r_greyscale->value);
			scan[i*4 + 1] = LERP(scan[i*4 + 1], luma, r_greyscale->value);
			scan[i*4 + 2] = LERP(scan[i*4 + 2], luma, r_greyscale->value);
		}
	}

	// normals are always swizzled
	if (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT)
	{
		RawImage_SwizzleRA(data, width, height);
	}

	// LATC2 is only used for normals
	if (internalFormat == GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT)
	{
		byte *in = data;
		int c = width * height;
		while (c--)
		{
			in[0] = in[1];
			in[2] = in[1];
			in += 4;
		}
	}

	// copy or resample data as appropriate for first MIP level
	if ( ( scaled_width == width ) && 
		( scaled_height == height ) ) {
		if (!(flags & IMGFLAG_MIPMAP))
		{
			RawImage_UploadTexture( data, 0, 0, scaled_width, scaled_height, internalFormat, type, flags, qfalse );
			//qglTexImage2D (GL_TEXTURE_2D, 0, internalFormat, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			*pUploadWidth = scaled_width;
			*pUploadHeight = scaled_height;

			goto done;
		}
		Com_Memcpy (scaledBuffer, data, width*height*4);
	}
	else
	{
		// use the normal mip-mapping function to go down from here
		while ( width > scaled_width || height > scaled_height ) {

			if (flags & IMGFLAG_SRGB)
			{
				R_MipMapsRGB( (byte *)data, width, height );
			}
			else
			{
				R_MipMap( (byte *)data, width, height );
			}

			width >>= 1;
			height >>= 1;
			if ( width < 1 ) {
				width = 1;
			}
			if ( height < 1 ) {
				height = 1;
			}
		}
		Com_Memcpy( scaledBuffer, data, width * height * 4 );
	}

	if (!(flags & IMGFLAG_NOLIGHTSCALE))
		R_LightScaleTexture (scaledBuffer, scaled_width, scaled_height, (qboolean)(!(flags & IMGFLAG_MIPMAP)) );

	*pUploadWidth = scaled_width;
	*pUploadHeight = scaled_height;

	RawImage_UploadTexture(scaledBuffer, 0, 0, scaled_width, scaled_height, internalFormat, type, flags, qfalse);

done:

	if (flags & IMGFLAG_MIPMAP)
	{
		if (r_ext_texture_filter_anisotropic->value > 1.0f && glConfig.maxTextureFilterAnisotropy > 0.0f)
		{
			qglTexParameterf ( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
							  Com_Clamp( 1.0f, glConfig.maxTextureFilterAnisotropy, r_ext_texture_filter_anisotropic->value ) );
		}

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	GL_CheckErrors();

	if ( scaledBuffer != 0 )
		ri->Hunk_FreeTempMemory( scaledBuffer );
	if ( resampledBuffer != 0 )
		ri->Hunk_FreeTempMemory( resampledBuffer );
}


static void EmptyTexture( int width, int height, imgType_t type, int flags,
	qboolean lightMap, GLenum internalFormat, int *pUploadWidth, int *pUploadHeight )
{
	int			scaled_width, scaled_height;

	RawImage_ScaleToPower2(NULL, &width, &height, &scaled_width, &scaled_height, type, flags, NULL);

	*pUploadWidth = scaled_width;
	*pUploadHeight = scaled_height;

	RawImage_UploadTexture(NULL, 0, 0, scaled_width, scaled_height, internalFormat, type, flags, qfalse);

	if (flags & IMGFLAG_MIPMAP)
	{
		if (r_ext_texture_filter_anisotropic->integer > 1 && glConfig.maxTextureFilterAnisotropy > 0.0f)
		{
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
					(GLint)Com_Clamp( 1, glConfig.maxTextureFilterAnisotropy, r_ext_texture_filter_anisotropic->integer ) );
		}

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	// Fix for sampling depth buffer on old nVidia cards
	// from http://www.idevgames.com/forums/thread-4141-post-34844.html#pid34844
	switch(internalFormat)
	{
		case GL_DEPTH_COMPONENT:
		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32:
			qglTexParameterf(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE );
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			break;
		default:
			break;
	}

	GL_CheckErrors();
}


/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, byte *pic, int width, int height, imgType_t type, int flags, int internalFormat ) {
	image_t		*image;
	qboolean	isLightmap = qfalse;
	long		hash;
	int         glWrapClampMode;

	if (strlen(name) >= MAX_QPATH ) {
		ri->Error (ERR_DROP, "R_CreateImage: \"%s\" is too long", name);
	}
	if ( !strncmp( name, "*lightmap", 9 ) ) {
		isLightmap = qtrue;
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri->Error( ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit");
	}

	image = tr.images[tr.numImages] = (image_t *)ri->Hunk_Alloc( sizeof( image_t ), h_low );
	image->texnum = 1024 + tr.numImages;
	tr.numImages++;

	image->type = type;
	image->flags = flags;

	/*
	if (pic)
	{
		// UQ1: Testing...
		if (!(flags & IMGFLAG_NO_COMPRESSION))
		{
			image->flags |= IMGFLAG_NO_COMPRESSION;
		}

		// UQ1: Testing...
		if ( glRefConfig.immutableTextures && !(flags & IMGFLAG_MUTABLE) )
		{
			image->flags |= IMGFLAG_MUTABLE;
		}

		// UQ1: Testing...
		if (pic && !(flags & IMGFLAG_MIPMAP))
		{
			image->flags |= IMGFLAG_MIPMAP;
		}

		{// UQ1: testing high pass sharpen filter...
#if 0
			thisByte[0] = 0; // alpha????
			thisByte[1] = 0; // red
			thisByte[2] = 0; // green
			thisByte[3] = 0; // blue
#endif //0

			#define filterWidth 5
			#define filterHeight 5

			double filter[filterWidth][filterHeight] =
			{
				-1, -1, -1, -1, -1,
				-1,  2,  2,  2, -1,
				-1,  2,  8,  2, -1,
				-1,  2,  2,  2, -1,
				-1, -1, -1, -1, -1,
			};

			double factor = 1.0 / 8.0;
			double bias = 0.0;

			RGBAtoYCoCgA(pic, pic, width, height);

			//apply the filter 
			for(int x = 0; x < width; x++)
			{
				for(int y = 0; y < height; y++) 
				{ 
					double red = 0.0, green = 0.0, blue = 0.0; 

					byte *thisByte  = pic  + (y * width + x) * 4;

					//multiply every value of the filter with corresponding image pixel 
					for(int filterX = 0; filterX < filterWidth; filterX++) 
					{
						for(int filterY = 0; filterY < filterHeight; filterY++) 
						{ 
							//int imageX = (x - filterWidth / 2 + filterX + width) % width;
							//int imageY = (y - filterHeight / 2 + filterY + height) % height;

							//byte *inbyte  = pic  + (imageY * width + imageX) * 4;
							int useY = y+filterY;
							int useX = x+filterX;

							if (useY < 0) useY = (useY+height);
							if (useX < 0) useX = (useX+width);
							if (useY >= height) useY = (useY-height);
							if (useX >= width) useX = (useX-width);

							byte *inbyte  = pic  + (useY * width + useX) * 4;
							
							red += inbyte[1] * filter[filterX][filterY];
							green += inbyte[2] * filter[filterX][filterY];
							blue += inbyte[3] * filter[filterX][filterY];
						}

						//truncate values smaller than zero and larger than 255 
						thisByte[1] = min(max(int(factor * red + bias), 0), 255); 
						thisByte[2] = min(max(int(factor * green + bias), 0), 255); 
						thisByte[3] = min(max(int(factor * blue + bias), 0), 255);
					}
				}
			}

			YCoCgAtoRGBA(pic, pic, width, height);
		}
	}
	*/

	Q_strncpyz (image->imgName, name, sizeof (image->imgName));

	image->width = width;
	image->height = height;
	if (flags & IMGFLAG_CLAMPTOEDGE)
		glWrapClampMode = GL_CLAMP_TO_EDGE;
	else
		glWrapClampMode = GL_REPEAT;

	if (!internalFormat)
	{
		if (image->flags & IMGFLAG_CUBEMAP)
			internalFormat = GL_RGBA8;
		else
			internalFormat = RawImage_GetFormat(pic, width * height, isLightmap, image->type, image->flags);
	}

	image->internalFormat = internalFormat;
		

	// lightmaps are always allocated on TMU 1
	if ( qglActiveTextureARB && isLightmap ) {
		image->TMU = 1;
	} else {
		image->TMU = 0;
	}

	if ( qglActiveTextureARB ) {
		GL_SelectTexture( image->TMU );
	}

	if (image->flags & IMGFLAG_CUBEMAP)
	{
		GL_Bind(image);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		if (image->flags & IMGFLAG_MIPMAP)
		{
			qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}

		if ( !pic )
		{
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
		}
		else if (glRefConfig.immutableTextures && !(image->flags & IMGFLAG_MUTABLE))
		{
			int numLevels = (image->flags & IMGFLAG_MIPMAP) ? CalcNumMipmapLevels (width, height) : 1;

			qglTexStorage2D (GL_TEXTURE_CUBE_MAP, numLevels, internalFormat, width, height);

			for ( int i = 0; i < 6; i++ )
			{
				qglTexSubImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			}
		}
		else
		{
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
			qglTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, internalFormat, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pic);
		}

		if (image->flags & IMGFLAG_MIPMAP)
			qglGenerateMipmap(GL_TEXTURE_CUBE_MAP);

		image->uploadWidth = width;
		image->uploadHeight = height;
	}
	else
	{
		GL_Bind(image);

		if (pic)
		{
			Upload32( pic, image->width, image->height, image->type, image->flags,
				isLightmap, image->internalFormat, &image->uploadWidth,
				&image->uploadHeight );
		}
		else
		{
			EmptyTexture(image->width, image->height, image->type, image->flags,
				isLightmap, image->internalFormat, &image->uploadWidth,
				&image->uploadHeight );
		}

		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapClampMode );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapClampMode );
	}

	GL_SelectTexture( 0 );

	hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	return image;
}

void R_UpdateSubImage( image_t *image, byte *pic, int x, int y, int width, int height )
{
	byte *scaledBuffer = NULL;
	byte *resampledBuffer = NULL;
	int	 scaled_width, scaled_height, scaled_x, scaled_y;
	byte *data = pic;

	// normals are always swizzled
	if (image->type == IMGTYPE_NORMAL || image->type == IMGTYPE_NORMALHEIGHT)
	{
		RawImage_SwizzleRA(pic, width, height);
	}

	// LATC2 is only used for normals
	if (image->internalFormat == GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT)
	{
		byte *in = data;
		int c = width * height;
		while (c--)
		{
			in[0] = in[1];
			in[2] = in[1];
			in += 4;
		}
	}


	RawImage_ScaleToPower2(&pic, &width, &height, &scaled_width, &scaled_height, image->type, image->flags, &resampledBuffer);

	scaledBuffer = (byte *)ri->Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );

	if ( qglActiveTextureARB ) {
		GL_SelectTexture( image->TMU );
	}

	GL_Bind(image);	

	// copy or resample data as appropriate for first MIP level
	if ( ( scaled_width == width ) && 
		( scaled_height == height ) ) {
		if (!(image->flags & IMGFLAG_MIPMAP))
		{
			scaled_x = x * scaled_width / width;
			scaled_y = y * scaled_height / height;
			RawImage_UploadTexture( data, scaled_x, scaled_y, scaled_width, scaled_height, image->internalFormat, image->type, image->flags, qtrue );
			//qglTexSubImage2D( GL_TEXTURE_2D, 0, scaled_x, scaled_y, scaled_width, scaled_height, GL_RGBA, GL_UNSIGNED_BYTE, data );

			GL_CheckErrors();
			goto done;
		}
		Com_Memcpy (scaledBuffer, data, width*height*4);
	}
	else
	{
		// use the normal mip-mapping function to go down from here
		while ( width > scaled_width || height > scaled_height ) {

			if (image->flags & IMGFLAG_SRGB)
			{
				R_MipMapsRGB( (byte *)data, width, height );
			}
			else
			{
				R_MipMap( (byte *)data, width, height );
			}

			width >>= 1;
			height >>= 1;
			x >>= 1;
			y >>= 1;
			if ( width < 1 ) {
				width = 1;
			}
			if ( height < 1 ) {
				height = 1;
			}
		}
		Com_Memcpy( scaledBuffer, data, width * height * 4 );
	}

	if (!(image->flags & IMGFLAG_NOLIGHTSCALE))
		R_LightScaleTexture (scaledBuffer, scaled_width, scaled_height, (qboolean)(!(image->flags & IMGFLAG_MIPMAP)) );

	scaled_x = x * scaled_width / width;
	scaled_y = y * scaled_height / height;
	RawImage_UploadTexture( (byte *)data, scaled_x, scaled_y, scaled_width, scaled_height, image->internalFormat, image->type, image->flags, qtrue );

done:
	
	GL_SelectTexture( 0 );

	GL_CheckErrors();

	if ( scaledBuffer != 0 )
		ri->Hunk_FreeTempMemory( scaledBuffer );
	if ( resampledBuffer != 0 )
		ri->Hunk_FreeTempMemory( resampledBuffer );
}

extern FBO_t *FBO_Create(const char *name, int width, int height);
extern void FBO_Delete(FBO_t *fbo);
extern void FBO_AttachTextureImage(image_t *img, int index);
extern void FBO_SetupDrawBuffers();
extern qboolean R_CheckFBO(const FBO_t * fbo);

FBO_t *R_CreateNormalMapDestinationFBO( int width, int height )
{
	/*
	if (tr.NormalMapDestinationFBO) return tr.NormalMapDestinationFBO;
	tr.NormalMapDestinationFBO = FBO_Create("_generateImageDst", 8192, 8192);
	return tr.NormalMapDestinationFBO;
	*/

	if (tr.NormalMapDestinationFBO) {
		FBO_Delete(tr.NormalMapDestinationFBO);
		tr.numFBOs--; // Assume for now we are not threading...
	}
	tr.NormalMapDestinationFBO = FBO_Create("_generateImageDst", width, height);
	return tr.NormalMapDestinationFBO;
}

image_t *R_TextureESharpenGLSL ( const char *name, byte *pic, int width, int height, int flags, image_t *srcImage )
{
	int			normalFlags;
	vec4i_t		box;
	FBO_t		*dstFbo = NULL;
	image_t		*dstImage;
	imgType_t	type = srcImage->type;
	int			format = srcImage->internalFormat;

	if (!tr.esharpeningShader.program || !tr.esharpeningShader.uniformBuffer) return NULL; // Will get done later after init on usage...

	memset(srcImage->imgName, 0, sizeof(srcImage->imgName));
	sprintf(srcImage->imgName, "deleted");

	ri->Printf(PRINT_WARNING, "ESharpening [%ix%i] texture %s.\n", width, height, name);
	
	normalFlags = flags;

	dstFbo = R_CreateNormalMapDestinationFBO(width, height);
	FBO_Bind (dstFbo);
	dstImage = R_CreateImage( name, pic, width, height, type, normalFlags, format );
	qglBindTexture(GL_TEXTURE_2D, dstImage->texnum);
	FBO_AttachTextureImage(dstImage, 0);
	FBO_SetupDrawBuffers();
	R_CheckFBO(dstFbo);
	
	GLSL_BindProgram(&tr.esharpeningShader);
	GL_BindToTMU(srcImage, TB_LEVELSMAP);

	vec2_t screensize;
	screensize[0] = width;
	screensize[1] = height;
	GLSL_SetUniformVec2(&tr.esharpeningShader, UNIFORM_DIMENSIONS, screensize);
	GLSL_SetUniformMatrix16(&tr.esharpeningShader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	box[0] = 0;
	box[1] = 0;
	box[2] = width;
	box[3] = height;

	//qglGetTexImage

	FBO_BlitFromTexture(srcImage, box, NULL, dstFbo, box, &tr.esharpeningShader, NULL, 0);

	qglDeleteTextures( 1, &srcImage->texnum );

	return dstImage;
}

image_t *R_TextureESharpen2GLSL ( const char *name, byte *pic, int width, int height, int flags, image_t *srcImage )
{
	int			normalFlags;
	vec4i_t		box;
	FBO_t		*dstFbo = NULL;
	image_t		*dstImage;
	imgType_t	type = srcImage->type;
	int			format = srcImage->internalFormat;

	if (!tr.esharpening2Shader.program || !tr.esharpening2Shader.uniformBuffer) return NULL; // Will get done later after init on usage...

	memset(srcImage->imgName, 0, sizeof(srcImage->imgName));
	sprintf(srcImage->imgName, "deleted");

	ri->Printf(PRINT_WARNING, "ESharpening 2 [%ix%i] texture %s.\n", width, height, name);
	
	normalFlags = flags;

	dstFbo = R_CreateNormalMapDestinationFBO(width, height);
	FBO_Bind (dstFbo);
	dstImage = R_CreateImage( name, pic, width, height, type, normalFlags, format );
	qglBindTexture(GL_TEXTURE_2D, dstImage->texnum);
	FBO_AttachTextureImage(dstImage, 0);
	FBO_SetupDrawBuffers();
	R_CheckFBO(dstFbo);
	
	GLSL_BindProgram(&tr.esharpening2Shader);
	GL_BindToTMU(srcImage, TB_LEVELSMAP);

	vec2_t screensize;
	screensize[0] = width;
	screensize[1] = height;
	GLSL_SetUniformVec2(&tr.esharpening2Shader, UNIFORM_DIMENSIONS, screensize);
	GLSL_SetUniformMatrix16(&tr.esharpening2Shader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	box[0] = 0;
	box[1] = 0;
	box[2] = width;
	box[3] = height;

	//qglGetTexImage

	FBO_BlitFromTexture(srcImage, box, NULL, dstFbo, box, &tr.esharpening2Shader, NULL, 0);

	qglDeleteTextures( 1, &srcImage->texnum );

	return dstImage;
}

image_t *R_TextureDarkExpandGLSL ( const char *name, byte *pic, int width, int height, int flags, image_t *srcImage )
{
	int			normalFlags;
	vec4i_t		box;
	FBO_t		*dstFbo = NULL;
	image_t		*dstImage;
	imgType_t	type = srcImage->type;
	int			format = srcImage->internalFormat;

	if (!tr.darkexpandShader.program || !tr.darkexpandShader.uniformBuffer) return NULL; // Will get done later after init on usage...

	memset(srcImage->imgName, 0, sizeof(srcImage->imgName));
	sprintf(srcImage->imgName, "deleted");

	ri->Printf(PRINT_WARNING, "DarkXpand [%ix%i] texture %s.\n", width, height, name);
	
	normalFlags = flags;

	dstFbo = R_CreateNormalMapDestinationFBO(width, height);
	FBO_Bind (dstFbo);
	dstImage = R_CreateImage( name, pic, width, height, type, normalFlags, format );
	qglBindTexture(GL_TEXTURE_2D, dstImage->texnum);
	FBO_AttachTextureImage(dstImage, 0);
	FBO_SetupDrawBuffers();
	R_CheckFBO(dstFbo);
	
	GLSL_BindProgram(&tr.darkexpandShader);
	GL_BindToTMU(srcImage, TB_LEVELSMAP);

	vec2_t screensize;
	screensize[0] = width;
	screensize[1] = height;
	GLSL_SetUniformVec2(&tr.darkexpandShader, UNIFORM_DIMENSIONS, screensize);
	GLSL_SetUniformMatrix16(&tr.darkexpandShader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	box[0] = 0;
	box[1] = 0;
	box[2] = width;
	box[3] = height;

	//qglGetTexImage

	FBO_BlitFromTexture(srcImage, box, NULL, dstFbo, box, &tr.darkexpandShader, NULL, 0);

	qglDeleteTextures( 1, &srcImage->texnum );

	return dstImage;
}

image_t *R_TextureCleanGLSL ( const char *name, byte *pic, int width, int height, int flags, image_t *srcImage )
{
	int			normalFlags;
	vec4i_t		box;
	FBO_t		*dstFbo = NULL;
	image_t		*dstImage;
	imgType_t	type = srcImage->type;
	int			format = srcImage->internalFormat;

	if (!tr.texturecleanShader.program || !tr.texturecleanShader.uniformBuffer) return NULL; // Will get done later after init on usage...

	memset(srcImage->imgName, 0, sizeof(srcImage->imgName));
	sprintf(srcImage->imgName, "deleted");

	ri->Printf(PRINT_WARNING, "Cleaning [%ix%i] texture %s.\n", width, height, name);
	
	normalFlags = flags;

	dstFbo = R_CreateNormalMapDestinationFBO(width, height);
	FBO_Bind (dstFbo);
	dstImage = R_CreateImage( name, pic, width, height, type, normalFlags, format );
	qglBindTexture(GL_TEXTURE_2D, dstImage->texnum);
	FBO_AttachTextureImage(dstImage, 0);
	FBO_SetupDrawBuffers();
	R_CheckFBO(dstFbo);
	
	GLSL_BindProgram(&tr.texturecleanShader);
	GL_BindToTMU(srcImage, TB_DIFFUSEMAP);

	vec2_t screensize;
	screensize[0] = width;
	screensize[1] = height;
	GLSL_SetUniformVec2(&tr.texturecleanShader, UNIFORM_DIMENSIONS, screensize);
	GLSL_SetUniformMatrix16(&tr.texturecleanShader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	vec4_t local0;
	VectorSet4(local0, r_textureCleanSigma->value, r_textureCleanBSigma->value, r_textureCleanMSize->value, 0);
	GLSL_SetUniformVec4(&tr.texturecleanShader, UNIFORM_LOCAL0, local0);

	box[0] = 0;
	box[1] = 0;
	box[2] = width;
	box[3] = height;

	//qglGetTexImage

	FBO_BlitFromTexture(srcImage, box, NULL, dstFbo, box, &tr.texturecleanShader, NULL, 0);
	
	qglDeleteTextures( 1, &srcImage->texnum );

	return dstImage;
}

void R_SaveNormalMap (const char *name, image_t *dstImage)
{
	char filename[128];
	sprintf(filename, "%s.tga", name);

	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	
	int linelen, padlen = 0;
	size_t offset = 18, memcount;

	//allbuf = (byte *)ri->Hunk_AllocateTempMemory(4 * dstImage->width * dstImage->height);
	allbuf = (byte *)malloc((4 * dstImage->width * dstImage->height) + offset);

	GL_Bind(dstImage);
	qglReadPixels(0, 0, dstImage->width, dstImage->height, GL_RGBA, GL_UNSIGNED_BYTE, allbuf);
		
	buffer = allbuf + offset - 18;
	
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = dstImage->width & 255;
	buffer[13] = dstImage->width >> 8;
	buffer[14] = dstImage->height & 255;
	buffer[15] = dstImage->height >> 8;
	buffer[16] = 32;	// pixel size
	buffer[17] = 0x20;

	// swap rgb to bgr and remove padding from line endings -- UQ1: DT says this is incorrect for normals - I switched them back...
	linelen = dstImage->width * 4;
	
	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * dstImage->height;
	
	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{// This is correct according to visual studio... Not sure what the built-in code is doing...
			byte r = srcptr[0];
			byte a = srcptr[1];
			byte b = srcptr[2];
			byte g = srcptr[3];
			*destptr++ = r;
			*destptr++ = g;
			*destptr++ = b;
			*destptr++ = a;
			
			srcptr += 4;
		}
		
		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * dstImage->height;

	// gamma correct
	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(allbuf + offset, memcount);

	ri->FS_WriteFile(filename, buffer, memcount + 18);

	//ri->Hunk_FreeTempMemory(allbuf);
	free(allbuf);
}

image_t *R_CreateNormalMapGLSL ( const char *name, byte *pic, int width, int height, int flags, image_t	*srcImage )
{
	int			normalFlags;
	vec4i_t		box;
	FBO_t		*dstFbo = NULL;
	image_t		*dstImage;

	if (!tr.generateNormalMapShader.program || !tr.generateNormalMapShader.uniformBuffer) return NULL; // Will get done later after init on usage...

	//ri->Printf(PRINT_WARNING, "Generating [%ix%i] normal map %s.\n", width, height, name);
	
	normalFlags = (flags & ~(IMGFLAG_GENNORMALMAP | IMGFLAG_SRGB | IMGFLAG_CLAMPTOEDGE)) | IMGFLAG_NOLIGHTSCALE /*| IMGFLAG_NO_COMPRESSION*/;

	dstFbo = R_CreateNormalMapDestinationFBO(width, height);
	FBO_Bind (dstFbo);
	//dstImage = R_CreateImage( name, pic, width, height, IMGTYPE_NORMAL, normalFlags, GL_RGBA8 );
	dstImage = R_CreateImage( name, NULL, width, height, IMGTYPE_NORMAL, normalFlags, 0 );
	//qglBindTexture(GL_TEXTURE_2D, dstImage->texnum);
	FBO_AttachTextureImage(dstImage, 0);
	FBO_SetupDrawBuffers();
	R_CheckFBO(dstFbo);

	GL_Bind(srcImage);
	
	GLSL_BindProgram(&tr.generateNormalMapShader);
	GLSL_SetUniformInt(&tr.generateNormalMapShader, UNIFORM_DIFFUSEMAP, TB_DIFFUSEMAP);
	GL_BindToTMU(srcImage, TB_DIFFUSEMAP);

	vec2_t screensize;
	screensize[0] = width;
	screensize[1] = height;
	GLSL_SetUniformVec2(&tr.generateNormalMapShader, UNIFORM_DIMENSIONS, screensize);
	//GLSL_SetUniformMatrix16(&tr.generateNormalMapShader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	box[0] = 0;
	box[1] = 0;
	box[2] = width;
	box[3] = height;

	//qglGetTexImage

	qglViewport(0, 0, width, height);
	qglScissor(0, 0, width, height);

	vec4_t color;
	VectorSet4(color, 0.0, 0.0, 0.0, 0.0);

	FBO_BlitFromTexture(srcImage, NULL/*box*/, NULL, dstFbo, NULL/*box*/, &tr.generateNormalMapShader, color, 0);
	dstImage->generatedNormalMap = true;
	dstImage->height = height;
	dstImage->width = width;

	//R_SaveNormalMap(name, dstImage); // It seems the image loading code messes up loading tga files... Also causes load stutter loading them anyway...

	return dstImage;
}

image_t *R_CreateNormalMap ( const char *name, byte *pic, int width, int height, int flags, image_t	*srcImage )
{
	char normalName[MAX_QPATH];
	image_t *normalImage;
	//int normalWidth, normalHeight;
	int normalFlags;
	
	//normalFlags = (flags & ~(IMGFLAG_GENNORMALMAP | IMGFLAG_SRGB)) | IMGFLAG_NOLIGHTSCALE;
	normalFlags = (flags & ~(IMGFLAG_GENNORMALMAP | IMGFLAG_SRGB | IMGFLAG_CLAMPTOEDGE)) | IMGFLAG_NOLIGHTSCALE /*| IMGFLAG_NO_COMPRESSION*/;
	
	COM_StripExtension(name, normalName, MAX_QPATH);
	Q_strcat(normalName, MAX_QPATH, "_n");
	
	// find normalmap in case it's there
	normalImage = R_FindImageFile(normalName, IMGTYPE_NORMAL, normalFlags);

	//if (normalImage != NULL) ri->Printf(PRINT_WARNING, "Loaded real normal map file %s.\n", normalName);
	//else ri->Printf(PRINT_WARNING, "No real normal map file %s.\n", normalName);
	
	/*if (normalImage == NULL)
	{
		return R_CreateNormalMapGLSL( normalName, pic, width, height, flags, srcImage );
	}*/

	return normalImage;
#if 0
	// if not, generate it
	if (normalImage == NULL)
	{
		byte *normalPic;
		int x, y;
		
		normalWidth = width;
		normalHeight = height;
		normalPic = (byte *)Z_Malloc(width * height * 4, TAG_GENERAL);
		RGBAtoNormal(pic, normalPic, width, height, (qboolean)(flags & IMGFLAG_CLAMPTOEDGE));
		
#if 1
		// Brighten up the original image to work with the normal map
		RGBAtoYCoCgA(pic, pic, width, height);
		for (y = 0; y < height; y++)
		{
			byte *picbyte  = pic       + y * width * 4;
			byte *normbyte = normalPic + y * width * 4;
			for (x = 0; x < width; x++)
			{
				int div = MAX(normbyte[2] - 127, 16);
				picbyte[0] = CLAMP(picbyte[0] * 128 / div, 0, 255);
				picbyte  += 4;
				normbyte += 4;
			}
		}
		YCoCgAtoRGBA(pic, pic, width, height);
#else
		// Blur original image's luma to work with the normal map
		{
			byte *blurPic;
			
			RGBAtoYCoCgA(pic, pic, width, height);
			blurPic = ri.Malloc(width * height);
			
			for (y = 1; y < height - 1; y++)
			{
				byte *picbyte  = pic     + y * width * 4;
				byte *blurbyte = blurPic + y * width;
				
				picbyte += 4;
				blurbyte += 1;
				
				for (x = 1; x < width - 1; x++)
				{
					int result;
					
					result = *(picbyte - (width + 1) * 4) + *(picbyte - width * 4) + *(picbyte - (width - 1) * 4) +
					*(picbyte -          1  * 4) + *(picbyte            ) + *(picbyte +          1  * 4) +
					*(picbyte + (width - 1) * 4) + *(picbyte + width * 4) + *(picbyte + (width + 1) * 4);
					
					result /= 9;
					
					*blurbyte = result;
					picbyte += 4;
					blurbyte += 1;
				}
			}
			
			// FIXME: do borders
			
			for (y = 1; y < height - 1; y++)
			{
				byte *picbyte  = pic     + y * width * 4;
				byte *blurbyte = blurPic + y * width;
				
				picbyte += 4;
				blurbyte += 1;
				
				for (x = 1; x < width - 1; x++)
				{
					picbyte[0] = *blurbyte;
					picbyte += 4;
					blurbyte += 1;
				}
			}
			
			ri->Free(blurPic);
			
			YCoCgAtoRGBA(pic, pic, width, height);
		}
#endif
		
		R_CreateImage( normalName, normalPic, normalWidth, normalHeight, IMGTYPE_NORMAL, normalFlags, 0 );
		Z_Free( normalPic );
	}
#endif
}

static void R_CreateSpecularMap ( const char *name, byte *pic, int width, int height, int flags )
{
	char specularName[MAX_QPATH];
	image_t *specularImage;
	int normalFlags;
	
	normalFlags = (flags & ~(IMGFLAG_GENNORMALMAP | IMGFLAG_SRGB)) | IMGFLAG_NOLIGHTSCALE;
	
	COM_StripExtension(name, specularName, MAX_QPATH);
	Q_strcat(specularName, MAX_QPATH, "_s");
	
	// find normalmap in case it's there
	specularImage = R_FindImageFile(specularName, IMGTYPE_SPECULAR, normalFlags);

	//if (normalImage != NULL) ri->Printf(PRINT_WARNING, "Loaded real normal map file %s.\n", normalName);
	//else ri->Printf(PRINT_WARNING, "No real normal map file %s.\n", normalName);

	if (specularImage == NULL)
	{
		memset(specularName, 0, sizeof(specularName));
		COM_StripExtension(name, specularName, MAX_QPATH);
		Q_strcat(specularName, MAX_QPATH, "_spec");
		specularImage = R_FindImageFile(specularName, IMGTYPE_SPECULAR, normalFlags);
	}
}

static void R_CreateSubsurfaceMap ( const char *name, byte *pic, int width, int height, int flags )
{
	char SubsurfaceName[MAX_QPATH];
	image_t *SubsurfaceImage;
	int normalFlags;
	
	normalFlags = (flags & ~(IMGFLAG_GENNORMALMAP | IMGFLAG_SRGB)) | IMGFLAG_NOLIGHTSCALE;
	
	COM_StripExtension(name, SubsurfaceName, MAX_QPATH);
	Q_strcat(SubsurfaceName, MAX_QPATH, "_sub");
	
	// find normalmap in case it's there
	SubsurfaceImage = R_FindImageFile(SubsurfaceName, IMGTYPE_SUBSURFACE, normalFlags);

	//if (normalImage != NULL) ri->Printf(PRINT_WARNING, "Loaded real normal map file %s.\n", normalName);
	//else ri->Printf(PRINT_WARNING, "No real normal map file %s.\n", normalName);

	if (SubsurfaceImage == NULL)
	{
		memset(SubsurfaceName, 0, sizeof(SubsurfaceName));
		COM_StripExtension(name, SubsurfaceName, MAX_QPATH);
		Q_strcat(SubsurfaceName, MAX_QPATH, "_subsurface");
		SubsurfaceImage = R_FindImageFile(SubsurfaceName, IMGTYPE_SUBSURFACE, normalFlags);
	}
}

/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
extern shader_t *R_CreateGenericAdvancedShader( const char *name, const int *lightmapIndexes, const byte *styles, qboolean mipRawImage );
extern qboolean R_ShaderExists( const char *name, const int *lightmapIndexes, const byte *styles, qboolean mipRawImage );

char previous_name_loaded[256];

extern void StripCrap( const char *in, char *out, int destsize );

image_t	*R_FindImageFile( const char *name, imgType_t type, int flags )
{
	image_t	*image;
	int		width, height;
	byte	*pic;
	long	hash;

	if (!name || ri->Cvar_VariableIntegerValue( "dedicated" )) {
		return NULL;
	}

	hash = generateHashValue(name);

	//
	// see if the image is already loaded
	//
	for (image=hashTable[hash]; image; image=image->next) {
		if ( !strcmp( name, image->imgName ) ) {
			// the white image can be used with any set of parms, but other mismatches are errors
			if ( strcmp( name, "*white" ) ) {
				if ( image->flags != flags ) {
					ri->Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name, image->flags, flags );
				}
			}
			return image;
		}
	}

	//
	// load the pic from disk
	//
	R_LoadImage( name, &pic, &width, &height );

	if ( pic == NULL ) {
		return NULL;
	}

	image = R_CreateImage( name, pic, width, height, type, flags, GL_RGBA8 );

	if (name[0] != '*' && name[0] != '!' && name[0] != '$' && name[0] != '_' && type != IMGTYPE_NORMAL && type != IMGTYPE_SPECULAR  && type != IMGTYPE_SUBSURFACE && !(flags & IMGFLAG_CUBEMAP))
	{
		if (image && r_textureClean->integer)
		{
			GL_Bind(image);
			image = R_TextureCleanGLSL( name, pic, width, height, flags, image );
		}

		/*
		if (image && r_esharpening->integer)
		{
			GL_Bind(image);
			image = R_TextureESharpenGLSL( name, pic, width, height, flags, image );
		}
		if (image && r_esharpening2->integer)
		{
			GL_Bind(image);
			image = R_TextureESharpen2GLSL( name, pic, width, height, flags, image );
		}
		if (image && r_darkexpand->integer)
		{
			GL_Bind(image);
			image = R_TextureDarkExpandGLSL( name, pic, width, height, flags, image );
		}
		*/

		if (r_normalMapping->integer) 
		{
			if (image
				&& !(StringContainsWord(name, "sky") || StringContainsWord(name, "skies") || StringContainsWord(name, "cloud") || StringContainsWord(name, "glow") || StringContainsWord(name, "gfx/") || StringContainsWord(name, "gfx_base/")))
			{
				GL_Bind(image);
				R_CreateNormalMap( name, pic, width, height, flags, image );
			}
		}

		if (r_specularMapping->integer) 
			R_CreateSpecularMap( name, pic, width, height, flags );

		R_CreateSubsurfaceMap( name, pic, width, height, flags );
	}

	Z_Free( pic );
	
	return image;
}


/*
================
R_CreateDlightImage
================
*/
#define	DLIGHT_SIZE	16
static void R_CreateDlightImage( void ) {
	int		width, height;
	byte	*pic;

	R_LoadImage("gfx/2d/dlight", &pic, &width, &height);
	if (pic)
	{
		tr.dlightImage = R_CreateImage("*dlight", pic, width, height, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0 );
		Z_Free(pic);
	}
	else
	{	// if we dont get a successful load
		int		x,y;
		byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
		int		b;

		// make a centered inverse-square falloff blob for dynamic lighting
		for (x=0 ; x<DLIGHT_SIZE ; x++) {
			for (y=0 ; y<DLIGHT_SIZE ; y++) {
				float	d;

				d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
					( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
				b = 4000 / d;
				if (b > 255) {
					b = 255;
				} else if ( b < 75 ) {
					b = 0;
				}
				data[y][x][0] = 
				data[y][x][1] = 
				data[y][x][2] = b;
				data[y][x][3] = 255;			
			}
		}
		tr.dlightImage = R_CreateImage("*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0 );
	}
}


/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
	int		i;
	float	d;
	float	exp;
	
	exp = 0.5;

	for ( i = 0 ; i < FOG_TABLE_SIZE ; i++ ) {
		d = pow ( (float)i/(FOG_TABLE_SIZE-1), exp );

		tr.fogTable[i] = d;
	}
}

/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float	R_FogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0f/32.0f) / (30.0f/32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[ (int)(s * (FOG_TABLE_SIZE-1)) ];

	return d;
}

/*
================
R_CreateFogImage
================
*/
#define	FOG_S	256
#define	FOG_T	32
static void R_CreateFogImage( void ) {
	int		x,y;
	byte	*data;
	float	d;
	float	borderColor[4];

	data = (byte *)ri->Hunk_AllocateTempMemory( FOG_S * FOG_T * 4 );

	// S is distance, T is depth
#pragma omp parallel for schedule(dynamic)
	for (x=0 ; x<FOG_S ; x++) {
		for (y=0 ; y<FOG_T ; y++) {
			d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );

			data[(y*FOG_S+x)*4+0] = 
			data[(y*FOG_S+x)*4+1] = 
			data[(y*FOG_S+x)*4+2] = 255;
			data[(y*FOG_S+x)*4+3] = 255*d;
		}
	}
	// standard openGL clamping doesn't really do what we want -- it includes
	// the border color at the edges.  OpenGL 1.2 has clamp-to-edge, which does
	// what we want.
	tr.fogImage = R_CreateImage("*fog", (byte *)data, FOG_S, FOG_T, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0 );
	ri->Hunk_FreeTempMemory( data );

	borderColor[0] = 1.0;
	borderColor[1] = 1.0;
	borderColor[2] = 1.0;
	borderColor[3] = 1;

	qglTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor );
}

/*
==================
R_CreateDefaultImage
==================
*/
#define	DEFAULT_SIZE	16
static void R_CreateDefaultImage( void ) {
	int		x;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image will be a box, to allow you to see the mapping coordinates
	Com_Memset( data, 32, sizeof( data ) );
	for ( x = 0 ; x < DEFAULT_SIZE ; x++ ) {
		data[0][x][0] =
		data[0][x][1] =
		data[0][x][2] =
		data[0][x][3] = 255;

		data[x][0][0] =
		data[x][0][1] =
		data[x][0][2] =
		data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
		data[DEFAULT_SIZE-1][x][1] =
		data[DEFAULT_SIZE-1][x][2] =
		data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
		data[x][DEFAULT_SIZE-1][1] =
		data[x][DEFAULT_SIZE-1][2] =
		data[x][DEFAULT_SIZE-1][3] = 255;
	}
	tr.defaultImage = R_CreateImage("*default", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_MIPMAP, 0);
}

/*
==================
R_CreateBuiltinImages
==================
*/
void R_CreateBuiltinImages( void ) {
	int		x,y;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	// we use a solid white image instead of disabling texturing
	Com_Memset( data, 255, sizeof( data ) );
	tr.whiteImage = R_CreateImage("*white", (byte *)data, 8, 8, IMGTYPE_COLORALPHA, IMGFLAG_NONE, 0);

	tr.randomImage = R_FindImageFile("gfx/random.png", IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION);

	if (r_dlightMode->integer >= 2)
	{
		for( x = 0; x < MAX_DLIGHTS; x++)
		{
			tr.shadowCubemaps[x] = R_CreateImage(va("*shadowcubemap%i", x), NULL, PSHADOW_MAP_SIZE, PSHADOW_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE | IMGFLAG_CUBEMAP, 0);
		}
	}

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;			
		}
	}

	tr.identityLightImage = R_CreateImage("*identityLight", (byte *)data, 8, 8, IMGTYPE_COLORALPHA, IMGFLAG_NONE, 0);


	for(x=0;x<32;x++) {
		// scratchimage is usually used for cinematic drawing
		tr.scratchImage[x] = R_CreateImage("*scratch", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_PICMIP | IMGFLAG_CLAMPTOEDGE | IMGFLAG_MUTABLE, 0);
	}

	R_CreateDlightImage();
	R_CreateFogImage();

	int width, height, hdrFormat, rgbFormat;

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	hdrFormat = GL_RGBA8;
	if (r_hdr->integer)
		hdrFormat = GL_RGBA16F;

	rgbFormat = GL_RGBA8;

	tr.renderImage = R_CreateImage("_render", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);

	tr.normalImage = R_CreateImage("*normal", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.normalDetailedImage = R_CreateImage("*normaldetailed", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);

	tr.glowImage = R_CreateImage("*glow", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
#if 0
	tr.glowImageScaled[0] = R_CreateImage("*glowScaled0", NULL, width / 2, height / 2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.glowImageScaled[1] = R_CreateImage("*glowScaled1", NULL, width / 4, height / 4, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.glowImageScaled[2] = R_CreateImage("*glowScaled2a", NULL, width / 8, height / 8, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.glowImageScaled[3] = R_CreateImage("*glowScaled2b", NULL, width / 8, height / 8, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
#else
	int glowImageWidth = width;
	int glowImageHeight = height;
	for ( int i = 0; i < ARRAY_LEN(tr.glowImageScaled); i++ )
	{
		tr.glowImageScaled[i] = R_CreateImage(va("*glowScaled%d", i), NULL, glowImageWidth, glowImageHeight, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
		glowImageWidth = max(1, glowImageWidth >> 1);
		glowImageHeight = max(1, glowImageHeight >> 1);
	}
#endif

	if (r_drawSunRays->integer)
		tr.sunRaysImage = R_CreateImage("*sunRays", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, rgbFormat);

	tr.renderDepthImage  = R_CreateImage("*renderdepth",  NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);
	tr.textureDepthImage = R_CreateImage("*texturedepth", NULL, PSHADOW_MAP_SIZE, PSHADOW_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);

	//
	// UQ1: Added...
	//

	tr.genericFBOImage  = R_CreateImage("_generic",  NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.genericFBO2Image  = R_CreateImage("_generic2",  NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);

	tr.bloomRenderFBOImage[0]  = R_CreateImage("_bloom0",  NULL, width/2, height/2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.bloomRenderFBOImage[1]  = R_CreateImage("_bloom1",  NULL, width/2, height/2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.bloomRenderFBOImage[2]  = R_CreateImage("_bloom2",  NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);

	tr.anamorphicRenderFBOImage[0]  = R_CreateImage("_anamorphic0",  NULL, width/8, height/8, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.anamorphicRenderFBOImage[1]  = R_CreateImage("_anamorphic1",  NULL, width/8, height/8, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	tr.anamorphicRenderFBOImage[2]  = R_CreateImage("_anamorphic2",  NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);

	//
	// UQ1: End Added...
	//


	{
		unsigned short sdata[4];
		void *p;

		if (hdrFormat == GL_RGBA16F)
		{
			sdata[0] = FloatToHalf(0.0f);
			sdata[1] = FloatToHalf(0.45f);
			sdata[2] = FloatToHalf(1.0f);
			sdata[3] = FloatToHalf(1.0f);
			p = &sdata[0];
		}
		else
		{
			data[0][0][0] = 0;
			data[0][0][1] = 0.45f * 255;
			data[0][0][2] = 255;
			data[0][0][3] = 255;
			p = data;
		}

		tr.calcLevelsImage =   R_CreateImage("*calcLevels",    (byte *)p, 1, 1, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
		tr.targetLevelsImage = R_CreateImage("*targetLevels",  (byte *)p, 1, 1, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
		tr.fixedLevelsImage =  R_CreateImage("*fixedLevels",   (byte *)p, 1, 1, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
	}

	for (x = 0; x < 2; x++)
	{
		tr.textureScratchImage[x] = R_CreateImage(va("*textureScratch%d", x), NULL, 256, 256, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
	}
	for (x = 0; x < 2; x++)
	{
		tr.quarterImage[x] = R_CreateImage(va("*quarter%d", x), NULL, width / 2, height / 2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
	}

	if (r_ssao->integer || r_hbao->integer)
	{
		tr.screenSsaoImage = R_CreateImage("*screenSsao", NULL, width / 2, height / 2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
		tr.hdrDepthImage = R_CreateImage("*hdrDepth", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_INTENSITY32F_ARB);
	}

	if (r_shadows->integer == 4)
	{
		for( x = 0; x < MAX_DRAWN_PSHADOWS; x++)
		{
			tr.pshadowMaps[x] = R_CreateImage(va("*shadowmap%i", x), NULL, PSHADOW_MAP_SIZE, PSHADOW_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
		}
	}

	if (r_sunlightMode->integer)
	{
		for ( x = 0; x < 3; x++)
		{
			tr.sunShadowDepthImage[x] = R_CreateImage(va("*sunshadowdepth%i", x), NULL, r_shadowMapSize->integer, r_shadowMapSize->integer, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);
		}

		tr.screenShadowImage = R_CreateImage("*screenShadow", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
	}

#ifdef __DYNAMIC_SHADOWS__
	for ( int y = 0; y < MAX_DYNAMIC_SHADOWS; y++ )
	{
		for ( x = 0; x < 3; x++)
		{
			tr.dlightShadowDepthImage[y][x] = R_CreateImage(va("*dlightshadowdepth%i_%i", y, x), NULL, r_shadowMapSize->integer, r_shadowMapSize->integer, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);
		}
	}

	//tr.screenDlightShadowImage = R_CreateImage("*screenDlightShadow", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
#endif // __DYNAMIC_SHADOWS__

	if (r_cubeMapping->integer)
	{
		//tr.renderCubeImage = R_CreateImage("*renderCube", NULL, CUBE_MAP_SIZE, CUBE_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE | IMGFLAG_MIPMAP | IMGFLAG_CUBEMAP, rgbFormat);
		tr.renderCubeImage = R_CreateImage("*renderCube", NULL, r_cubeMapSize->integer, r_cubeMapSize->integer, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE | IMGFLAG_MIPMAP | IMGFLAG_CUBEMAP, rgbFormat);
	}
}


/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings( void ) {
	int		i, j;
	float	g;
	int		inf;

	// setup the overbright lighting
	tr.overbrightBits = r_overBrightBits->integer;

	// allow 2 overbright bits
	if ( tr.overbrightBits > 2 ) {
		tr.overbrightBits = 2;
	} else if ( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0f / ( 1 << tr.overbrightBits );
	tr.identityLightByte = 255 * tr.identityLight;

	if ( r_intensity->value <= 1 ) {
		ri->Cvar_Set( "r_intensity", "1" );
	}

	if ( r_gamma->value < 0.5f ) {
		ri->Cvar_Set( "r_gamma", "0.5" );
	} else if ( r_gamma->value > 3.0f ) {
		ri->Cvar_Set( "r_gamma", "3.0" );
	}

	g = r_gamma->value;

#pragma omp parallel for schedule(dynamic)
	for ( i = 0; i < 256; i++ ) {
		int i2;

		if (r_srgb->integer)
		{
			i2 = 255 * RGBtosRGB(i/255.0f) + 0.5f;
		}
		else
		{
			i2 = i;
		}

		if ( g == 1 ) {
			inf = i2;
		} else {
			inf = 255 * pow ( i2/255.0f, 1.0f / g ) + 0.5f;
		}

		if (inf < 0) {
			inf = 0;
		}
		if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for (i=0 ; i<256 ; i++) {
		j = i * r_intensity->value;
		if (j > 255) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if ( glConfig.deviceSupportsGamma )
	{
		GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
	}
}

/*
===============
R_InitImages
===============
*/
void	R_InitImages( void ) {
	Com_Memset(hashTable, 0, sizeof(hashTable));
	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}

/*
===============
R_DeleteTextures
===============
*/
void R_DeleteTextures( void ) {
	int		i;

	for ( i=0; i<tr.numImages ; i++ ) {
		qglDeleteTextures( 1, &tr.images[i]->texnum );
	}
	Com_Memset( tr.images, 0, sizeof( tr.images ) );

	tr.numImages = 0;

	Com_Memset( glState.currenttextures, 0, sizeof( glState.currenttextures ) );
	if ( qglActiveTextureARB ) {
		GL_SelectTexture( 1 );
		qglBindTexture( GL_TEXTURE_2D, 0 );
		GL_SelectTexture( 0 );
		qglBindTexture( GL_TEXTURE_2D, 0 );
	} else {
		qglBindTexture( GL_TEXTURE_2D, 0 );
	}
}

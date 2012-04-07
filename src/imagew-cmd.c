// imagew-cmd.c
// Part of ImageWorsener, Copyright (c) 2011 by Jason Summers.
// For more information, see the readme.txt file.

// This file implements a command-line application, and is not
// part of the ImageWorsener library.

// Note that applications that are not distributed with ImageWorsener are
// not expected to include imagew-config.h.
#include "imagew-config.h"

#ifdef IW_WINDOWS
#define IW_NO_LOCALE
#endif

#ifdef IW_WINDOWS
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

#ifdef IW_WINDOWS
#include <malloc.h>
#include <fcntl.h>
#include <io.h> // for _setmode
#endif

#ifndef IW_NO_LOCALE
#include <locale.h>
#include <langinfo.h>
#endif

#include "imagew.h"

#ifdef IW_WINDOWS
#include <strsafe.h>
#endif

#define IWCMD_ENCODING_AUTO   0
#define IWCMD_ENCODING_ASCII  1
#define IWCMD_ENCODING_UTF8   2
#define IWCMD_ENCODING_UTF16  3

struct rgb_color {
	double r,g,b;
};

struct resize_alg {
	int family;
	double param1, param2;
};

struct resize_blur {
	double blur;
	int interpolate; // If set, muliply 'blur' by the scaling factor (if downscaling)
};

struct uri_struct {
#define IWCMD_SCHEME_FILE       1
#define IWCMD_SCHEME_CLIPBOARD  2
	int scheme;
	const char *uri;
	const char *filename; // Points to a position in id. Meaningful if scheme==FILE.
};

struct params_struct {
	struct uri_struct input_uri;
	struct uri_struct output_uri;
	int nowarn;
	int noinfo;
	int src_width, src_height;
	int adjusted_src_width, adjusted_src_height;
	int dst_width_req, dst_height_req;
	int rel_width_flag, rel_height_flag;
	int noresize_flag;
	double rel_width, rel_height;
	int dst_width, dst_height;
	struct resize_alg resize_alg_x;
	struct resize_alg resize_alg_y;
	struct resize_blur resize_blur_x;
	struct resize_blur resize_blur_y;
	int bestfit;
	int depth;
	int depth_r, depth_g, depth_b, depth_k, depth_a;
	int compression;
	int grayscale, condgrayscale;
	double offset_r_h, offset_g_h, offset_b_h;
	double offset_r_v, offset_g_v, offset_b_v;
	double translate_x, translate_y;
	int translate_src_flag; // If 1, translate_[xy] is in source pixels.
	int dither_family_all, dither_family_nonalpha, dither_family_alpha;
	int dither_family_red, dither_family_green, dither_family_blue, dither_family_gray;
	int dither_subtype_all, dither_subtype_nonalpha, dither_subtype_alpha;
	int dither_subtype_red, dither_subtype_green, dither_subtype_blue, dither_subtype_gray;
	int color_count_all, color_count_nonalpha, color_count_alpha;
	int color_count_red, color_count_green, color_count_blue, color_count_gray;
	int apply_bkgd;
	int bkgd_checkerboard;
	int bkgd_check_size;
	int bkgd_check_origin_x, bkgd_check_origin_y;
	int use_bkgd_label;
	int use_crop, crop_x, crop_y, crop_w, crop_h;
	struct rgb_color bkgd;
	struct rgb_color bkgd2;
	int page_to_read;
	int jpeg_quality;
	int jpeg_samp_factor_h, jpeg_samp_factor_v;
	int jpeg_arith_coding;
	int bmp_trns;
	double webp_quality;
	int zipcmprlevel;
	int zipcmprlevel_set;
	int interlace;
	int randomize;
	int random_seed;
	int infmt;
	int outfmt;
	int no_gamma;
	int intclamp;
	int edge_policy_x,edge_policy_y;

#define IWCMD_DENSITY_POLICY_AUTO    0
#define IWCMD_DENSITY_POLICY_NONE    1 // Don't write a density (if possible)
#define IWCMD_DENSITY_POLICY_KEEP    2 // Keep density the same
#define IWCMD_DENSITY_POLICY_ADJUST  3 // Keep physical image size the same
#define IWCMD_DENSITY_POLICY_FORCED  4 // Use a specific density
	int density_policy;

	int pref_units;
	double density_forced_x, density_forced_y; // in pixels/meter
	int grayscale_formula;
	double grayscale_weight[3];
	int no_cslabel;
	int include_screen;
	int noopt_grayscale,noopt_binarytrns,noopt_palette;
	int noopt_reduceto8,noopt_stripalpha;
	int cs_in_set, cs_out_set;
	struct iw_csdescr cs_in;
	struct iw_csdescr cs_out;
	int output_encoding;
	int output_encoding_setmode;
#ifdef IW_WINDOWS
	int clipboard_is_open;
	HANDLE clipboard_data_handle;
	SIZE_T clipboard_data_size;
	SIZE_T clipboard_data_pos;
	iw_byte *clipboard_data;

	iw_byte tmp_fileheader[14];
	HANDLE clipboard_data_handle_w;
	SIZE_T clipboard_data_size_w;
	SIZE_T clipboard_data_pos_w;
	iw_byte *clipboard_data_w;
#endif
};

#ifdef _UNICODE
static char *iwcmd_utf16_to_utf8_strdup(const WCHAR *src)
{
	char *dst;
	int dstlen;
	int ret;

	// Calculate the size required by the target string.
	ret = WideCharToMultiByte(CP_UTF8,0,src,-1,NULL,0,NULL,NULL);
	if(ret<1) return NULL;

	dstlen = ret;
	dst = (char*)malloc(dstlen*sizeof(char));
	if(!dst) return NULL;

	ret = WideCharToMultiByte(CP_UTF8,0,src,-1,dst,dstlen,NULL,NULL);
	if(ret<1) {
		free(dst);
		return NULL;
	}
	return dst;
}

static WCHAR *iwcmd_utf8_to_utf16_strdup(const char *src)
{
	WCHAR *dst;
	int dstlen;
	int ret;

	// Calculate the size required by the target string.
	ret = MultiByteToWideChar(CP_UTF8,0,src,-1,NULL,0);
	if(ret<1) return NULL;

	dstlen = ret;
	dst = (WCHAR*)malloc(dstlen*sizeof(WCHAR));
	if(!dst) return NULL;

	ret = MultiByteToWideChar(CP_UTF8,0,src,-1,dst,dstlen);
	if(ret<1) {
		free(dst);
		return NULL;
	}
	return dst;
}

static void iwcmd_utf8_to_utf16(const char *src, WCHAR *dst, int dstlen)
{
	MultiByteToWideChar(CP_UTF8,0,src,-1,dst,dstlen);
}
#endif

// Output a NUL-terminated string.
// The input string is encoded in UTF-8.
// If the output encoding is not UTF-8, it will be converted.
static void iwcmd_puts_utf8(struct params_struct *p, const char *s)
{
	char buf[500];
#ifdef _UNICODE
	WCHAR bufW[500];
#endif

	switch(p->output_encoding) {
#ifdef _UNICODE
	case IWCMD_ENCODING_UTF16:
		iwcmd_utf8_to_utf16(s,bufW,sizeof(bufW)/sizeof(WCHAR));
		fputws(bufW,stdout);
		break;
#endif
	case IWCMD_ENCODING_UTF8:
		fputs(s,stdout);
		break;
	default:
		iw_utf8_to_ascii(s,buf,sizeof(buf));
		fputs(buf,stdout);
	}
}

static void iwcmd_vprint_utf8(struct params_struct *p, const char *fmt, va_list ap)
{
	char buf[500];

#ifdef IW_WINDOWS

	StringCbVPrintfA(buf,sizeof(buf),fmt,ap);

#else

	vsnprintf(buf,sizeof(buf),fmt,ap);
	buf[sizeof(buf)-1]='\0';

#endif

	iwcmd_puts_utf8(p,buf);
}

static void iwcmd_message(struct params_struct *p, const char *fmt, ...)
  iw_gnuc_attribute ((format (printf, 2, 3)));

static void iwcmd_message(struct params_struct *p, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	iwcmd_vprint_utf8(p,fmt,ap);
	va_end(ap);
}

static void iwcmd_warning(struct params_struct *p, const char *fmt, ...)
  iw_gnuc_attribute ((format (printf, 2, 3)));

static void iwcmd_warning(struct params_struct *p, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	iwcmd_vprint_utf8(p,fmt,ap);
	va_end(ap);
}

static void iwcmd_error(struct params_struct *p, const char *fmt, ...)
  iw_gnuc_attribute ((format (printf, 2, 3)));

static void iwcmd_error(struct params_struct *p, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	iwcmd_vprint_utf8(p,fmt,ap);
	va_end(ap);
}

// Wrappers for fopen()
#if defined(IW_WINDOWS) && defined(_UNICODE)

static FILE* iwcmd_fopen(const char *fn, const char *mode)
{
	FILE *f = NULL;
	errno_t ret;
	WCHAR *fnW;
	WCHAR *modeW;

	fnW = iwcmd_utf8_to_utf16_strdup(fn);
	modeW = iwcmd_utf8_to_utf16_strdup(mode);

	ret = _wfopen_s(&f,fnW,modeW);

	free(fnW);
	free(modeW);

	if(ret!=0) {
		// failure
		if(f) fclose(f);
		f=NULL;
	}
	return f;
}

#elif defined(IW_WINDOWS) && !defined(_UNICODE)

static FILE* iwcmd_fopen(const char *fn, const char *mode)
{
	FILE *f = NULL;
	errno_t ret;

	ret = fopen_s(&f,fn,mode);
	if(ret!=0) {
		// failure
		if(f) fclose(f);
		f=NULL;
	}
	return f;
}

#else

static FILE* iwcmd_fopen(const char *fn, const char *mode)
{
	return fopen(fn,mode);
}

#endif

static void my_warning_handler(struct iw_context *ctx, const char *msg)
{
	struct params_struct *p;
	p = (struct params_struct *)iw_get_userdata(ctx);
	if(!p->nowarn) {
		iwcmd_warning(p,"Warning: %s\n",msg);
	}
}

// This is used to process the parameter of -infmt/-outfmt.
static int get_fmt_from_name(const char *s)
{
	if(!strcmp(s,"png")) return IW_FORMAT_PNG;
	if(!strcmp(s,"jpg")) return IW_FORMAT_JPEG;
	if(!strcmp(s,"jpeg")) return IW_FORMAT_JPEG;
	if(!strcmp(s,"bmp")) return IW_FORMAT_BMP;
	if(!strcmp(s,"tif")) return IW_FORMAT_TIFF;
	if(!strcmp(s,"tiff")) return IW_FORMAT_TIFF;
	if(!strcmp(s,"miff")) return IW_FORMAT_MIFF;
	if(!strcmp(s,"webp")) return IW_FORMAT_WEBP;
	if(!strcmp(s,"gif")) return IW_FORMAT_GIF;
	return IW_FORMAT_UNKNOWN;
}

// Reads the first few bytes in the file to try to figure out
// the file format. Then sets the file pointer back to the
// beginning of the file.
static int detect_fmt_of_file(FILE *fp)
{
	unsigned char buf[12];
	size_t n;

	n=fread(buf,1,12,fp);
	clearerr(fp);
	fseek(fp,0,SEEK_SET);
	if(n<2) return IW_FORMAT_UNKNOWN;

	return iw_detect_fmt_of_file((const iw_byte*)buf,n);
}

// Updates p->dst_width and p->dst_height.
// Returns 0 if we fit to width, 1 if we fit to height.
static int do_bestfit(struct params_struct *p)
{
	int x;
	double exp_factor;
	int retval = 0;

	// If we fit-width, what would the height be?
	exp_factor = ((double)p->dst_width) / p->adjusted_src_width;
	x = (int)(0.5+ ((double)p->adjusted_src_height) * exp_factor);
	if(x<=p->dst_height) {
		// It fits. Use it.
		p->dst_height = x;
		goto done;
	}

	// Fit to height instead.
	retval = 1;
	exp_factor = ((double)p->dst_height) / p->adjusted_src_height;
	x = (int)(0.5+ ((double)p->adjusted_src_width) * exp_factor);
	if(x<p->dst_width) {
		p->dst_width = x;
	}

done:
	if(p->dst_width<1) p->dst_width=1;
	if(p->dst_height<1) p->dst_height=1;
	return retval;
}

static void iwcmd_set_resize(struct iw_context *ctx, int dimension,
	struct resize_alg *alg, struct resize_blur *rblur)
{
	iw_set_resize_alg(ctx,dimension,alg->family,rblur->blur,alg->param1,alg->param2);
}

static int my_readfn(struct iw_context *ctx, struct iw_iodescr *iodescr, void *buf, size_t nbytes,
   size_t *pbytesread)
{
	*pbytesread = fread(buf,1,nbytes,(FILE*)iodescr->fp);
	return 1;
}

static int my_getfilesizefn(struct iw_context *ctx, struct iw_iodescr *iodescr, iw_int64 *pfilesize)
{
	int ret;
	long lret;
	FILE *fp = (FILE*)iodescr->fp;

	// TODO: Rewrite this to support >4GB file sizes.
	ret=fseek(fp,0,SEEK_END);
	if(ret!=0) return 0;
	lret=ftell(fp);
	if(lret<0) return 0;
	*pfilesize = (iw_int64)lret;
	fseek(fp,0,SEEK_SET);
	return 1;
}

static int my_seekfn(struct iw_context *ctx, struct iw_iodescr *iodescr, iw_int64 offset, int whence)
{
	FILE *fp = (FILE*)iodescr->fp;
	fseek(fp,(long)offset,whence);
	return 1;
}

static int my_writefn(struct iw_context *ctx, struct iw_iodescr *iodescr, const void *buf, size_t nbytes)
{
	fwrite(buf,1,nbytes,(FILE*)iodescr->fp);
	return 1;
}

////////////////// clipboard ////////////////////
#ifdef IW_WINDOWS

static void iwcmd_close_clipboard_r(struct params_struct *p, struct iw_context *ctx)
{
	if(p->clipboard_is_open) {
		if(p->clipboard_data) {
			GlobalUnlock(p->clipboard_data_handle);
			p->clipboard_data = NULL;
		}
		p->clipboard_data_handle = NULL;
		CloseClipboard();
		p->clipboard_is_open=0;
	}
}

static void iwcmd_close_clipboard_w(struct params_struct *p, struct iw_context *ctx)
{
	if(p->clipboard_data_handle_w) {
		if(p->clipboard_data_w) {
			GlobalUnlock(p->clipboard_data_handle_w);
			p->clipboard_data_w = NULL;
		}
		GlobalFree(p->clipboard_data_handle_w);
		p->clipboard_data_handle_w = NULL;
	}
}

static int iwcmd_open_clipboard_for_read(struct params_struct *p, struct iw_context *ctx)
{
	BOOL b;
	HANDLE hClip = NULL;

	b=OpenClipboard(GetConsoleWindow());
	if(!b) {
		iw_set_error(ctx,"Failed to open the clipboard");
		return 0;
	}

	p->clipboard_is_open = 1;

	p->clipboard_data_handle = GetClipboardData(CF_DIB);
	if(!p->clipboard_data_handle) {
		iw_set_error(ctx,"Can\xe2\x80\x99t find an image on the clipboard");
		iwcmd_close_clipboard_r(p,ctx);
		return 0;
	}

	p->clipboard_data_size = GlobalSize(p->clipboard_data_handle);
	p->clipboard_data = (iw_byte*)GlobalLock(p->clipboard_data_handle);
	p->clipboard_data_pos = 0;
	return 1;
}

static int my_clipboard_readfn(struct iw_context *ctx, struct iw_iodescr *iodescr, void *buf, size_t nbytes,
   size_t *pbytesread)
{
	struct params_struct *p;
	size_t nbytes_to_return;

	p = (struct params_struct *)iw_get_userdata(ctx);
	if(!p->clipboard_data) return 0;
	nbytes_to_return = nbytes;
	if(nbytes_to_return > p->clipboard_data_size-p->clipboard_data_pos)
		nbytes_to_return = p->clipboard_data_size-p->clipboard_data_pos;
	memcpy(buf,&p->clipboard_data[p->clipboard_data_pos],nbytes_to_return);
	*pbytesread = nbytes_to_return;
	p->clipboard_data_pos += nbytes_to_return;
	return 1;
}

static int my_clipboard_getfilesizefn(struct iw_context *ctx, struct iw_iodescr *iodescr, iw_int64 *pfilesize)
{
	struct params_struct *p;
	p = (struct params_struct *)iw_get_userdata(ctx);
	*pfilesize = (iw_int64)p->clipboard_data_size;
	return 1;
}

static int my_clipboard_writefn(struct iw_context *ctx, struct iw_iodescr *iodescr, const void *buf, size_t nbytes)
{
	struct params_struct *p;
	size_t bytes_consumed = 0;
	size_t bytes_remaining;
	iw_uint32 filesize;

	p = (struct params_struct *)iw_get_userdata(ctx);

	// Store the first 14 bytes in p->tmp_fileheader
	if(p->clipboard_data_pos_w < 14) {
		bytes_consumed = 14 - p->clipboard_data_pos_w;
		if(bytes_consumed > nbytes) bytes_consumed = nbytes;
		memcpy(&p->tmp_fileheader[p->clipboard_data_pos_w],buf,bytes_consumed);
		p->clipboard_data_pos_w += bytes_consumed;

		// If we've read the whole fileheader, look at it to figure out the file size
		if(p->clipboard_data_pos_w >= 14) {
			filesize = p->tmp_fileheader[2] | (p->tmp_fileheader[3]<<8) |
				p->tmp_fileheader[4]<<16 | (p->tmp_fileheader[5]<<24);

			if(filesize<14 || filesize>500000000) { // Sanity check
				return 0;
			}

			// Create a memory block to eventually place on the clipboard.

			p->clipboard_data_size_w = filesize-14;
			p->clipboard_data_handle_w = GlobalAlloc(GMEM_ZEROINIT|GMEM_MOVEABLE,p->clipboard_data_size_w);
			if(!p->clipboard_data_handle_w) return 0;
			p->clipboard_data_w = GlobalLock(p->clipboard_data_handle_w);
			if(!p->clipboard_data_w) return 0;
		}
	}

	bytes_remaining = nbytes-bytes_consumed;
	if(bytes_remaining>0 && p->clipboard_data_pos_w>=14 && p->clipboard_data_w) {
		memcpy(&p->clipboard_data_w[p->clipboard_data_pos_w-14],buf,bytes_remaining);
		p->clipboard_data_pos_w += bytes_remaining;
	}

	return 1;
}

static int finish_clipboard_write(struct params_struct *p, struct iw_context *ctx)
{
	int retval = 0;

	if(!p->clipboard_data_handle_w) return 0;

	// Prevent the clipboard from being changed by other apps.
	if(!OpenClipboard(GetConsoleWindow())) {
		return 0;
	}

	// Claim ownership of the clipboard
	if(!EmptyClipboard()) {
		goto done;
	}

	// Release our lock on the data we'll put on the clipboard.
	GlobalUnlock(p->clipboard_data_handle_w);
	p->clipboard_data_w = NULL;

	if(!SetClipboardData(CF_DIB,p->clipboard_data_handle_w)) {
		goto done;
	}

	// SetClipboardData succeeded, so the clipboard now owns the data.
	// Clear our copy of the handle.
	p->clipboard_data_handle_w = NULL;

	retval = 1;
done:
	if(!retval) {
		iw_set_error(ctx,"Failed to set clipboard data");
	}
	CloseClipboard();
	return retval;
}

#endif

/////////////////////////////////////////////////
static int iwcmd_calc_rel_size(double rel, int d)
{
	int n;
	n = (int)(0.5 + rel * (double)d);
	if(n<1) n=1;
	return n;
}

static void* my_mallocfn(void *userdata, unsigned int flags, size_t n)
{
	void *mem=NULL;

	if(flags & IW_MALLOCFLAG_ZEROMEM)
		mem = calloc(n,1);
	else
		mem = malloc(n);
	return mem;
}

static void my_freefn(void *userdata, void *mem)
{
	free(mem);
}

static void figure_out_size_and_density(struct params_struct *p, struct iw_context *ctx)
{
	int fit_flag = 0;
	int fit_dimension = -1; // 0 = We're fitting to a specific width. 1=height. -1=neither.
	double xdens,ydens; // density read from source image
	int density_code; // from src image
	double adjusted_dens_x, adjusted_dens_y;
	int nonsquare_pixels_flag = 0;
	int width_specified, height_specified;

	iw_get_input_density(ctx,&xdens,&ydens,&density_code);

	if(p->noresize_flag) {
		// Pretend the user requested a target height and width that are exactly
		// those of the source image.
		p->dst_width_req = p->src_width;
		p->dst_height_req = p->src_height;

		// These options shouldn't have been used, but if so, pretend they weren't.
		p->rel_width_flag = 0;
		p->rel_height_flag = 0;
		p->bestfit = 0;
	}

	if(density_code!=IW_DENSITY_UNKNOWN) {
		if(fabs(xdens-ydens)>=0.00001) {
			nonsquare_pixels_flag = 1;
		}
	}

	width_specified = p->dst_width_req>0 || p->rel_width_flag;
	height_specified = p->dst_height_req>0 || p->rel_height_flag;
	// If the user failed to specify a width, or a height, or used the 'bestfit'
	// option, then we need to "fit" the image in some way.
	fit_flag = !width_specified || !height_specified || p->bestfit;

	// Set adjusted_* variables, which will be different from the original ones
	// of there are nonsquare pixels. For certain operations, we'll pretend that
	// the adjusted settings are the real setting.
	p->adjusted_src_width = p->src_width;
	p->adjusted_src_height = p->src_height;
	adjusted_dens_x = xdens;
	adjusted_dens_y = ydens;
	if(nonsquare_pixels_flag && fit_flag) {
		if(xdens > ydens) {
			p->adjusted_src_height = (int)(0.5+ (xdens/ydens) * (double)p->adjusted_src_height);
			adjusted_dens_y = xdens;
		}
		else {
			p->adjusted_src_width = (int)(0.5+ (ydens/xdens) * (double)p->adjusted_src_width);
			adjusted_dens_x = ydens;
		}
	}

	p->dst_width = p->dst_width_req;
	p->dst_height = p->dst_height_req;

	if(p->dst_width<0) p->dst_width = -1;
	if(p->dst_height<0) p->dst_height = -1;
	if(p->dst_width==0) p->dst_width = 1;
	if(p->dst_height==0) p->dst_height = 1;

	if(p->rel_width_flag) {
		p->dst_width = iwcmd_calc_rel_size(p->rel_width, p->adjusted_src_width);
	}
	if(p->rel_height_flag) {
		p->dst_height = iwcmd_calc_rel_size(p->rel_height, p->adjusted_src_height);
	}

	if(p->dst_width == -1 && p->dst_height == -1) {
		// Neither -width nor -height specified. Keep image the same size.
		// (But if the pixels were not square, pretend the image was a different
		// size, and had square pixels.)
		p->dst_width=p->adjusted_src_width;
		p->dst_height=p->adjusted_src_height;
	}
	else if(p->dst_height == -1) {
		// -width given but not -height. Fit to width.
		p->dst_height=1000000;
		do_bestfit(p);
		fit_dimension=0;
	}
	else if(p->dst_width == -1) {
		// -height given but not -width. Fit to height.
		p->dst_width=1000000;
		do_bestfit(p);
		fit_dimension=1;
	}
	else if(p->bestfit) {
		// -width and -height and -bestfit all given. Best-fit into the given dimensions.
		fit_dimension=do_bestfit(p);
	}
	else {
		// -width and -height given but not -bestfit. Use the exact dimensions given.
		;
	}

	if(p->dst_width<1) p->dst_width=1;
	if(p->dst_height<1) p->dst_height=1;

	// Figure out what policy=AUTO means.
	if(p->density_policy==IWCMD_DENSITY_POLICY_AUTO) {
		if(density_code==IW_DENSITY_UNKNOWN) {
			p->density_policy=IWCMD_DENSITY_POLICY_NONE;
		}
		else if(p->dst_width==p->src_width && p->dst_height==p->src_height) {
			p->density_policy=IWCMD_DENSITY_POLICY_KEEP;
		}
		else if(!width_specified && !height_specified) {
			// If the user did not request the size to be changed, but we're
			// changing it anyway (presumably due to nonsquare pixels), keep
			// the image the same physical size.
			p->density_policy=IWCMD_DENSITY_POLICY_ADJUST;
		}
		else {
			p->density_policy=IWCMD_DENSITY_POLICY_NONE;
		}
	}

	// Finally, set the new density, based on the POLICY.
	if(p->density_policy==IWCMD_DENSITY_POLICY_KEEP) {
		if(density_code!=IW_DENSITY_UNKNOWN) {
			iw_set_output_density(ctx,adjusted_dens_x,adjusted_dens_y,density_code);
		}
	}
	else if(p->density_policy==IWCMD_DENSITY_POLICY_ADJUST) {
		if(density_code!=IW_DENSITY_UNKNOWN) {
			double newdens_x,newdens_y;
			// If we don't do anything to prevent it, the "adjust" policy will
			// tend to create images whose pixels are slightly non-square. While
			// not *wrong*, this is usually undesirable.
			// The ideal solution is probably to scale the dimensions by *exactly*
			// the same factor, but we don't support that yet.
			// In the meantime, if the source image had square pixels, fudge the
			// density label so that the target image also has square pixels, even
			// if that makes the label less accurate.
			// If possible, fix it up by changing the density of the dimension whose
			// size wasn't set by the user.
			newdens_x = adjusted_dens_x*(((double)p->dst_width)/p->adjusted_src_width);
			newdens_y = adjusted_dens_y*(((double)p->dst_height)/p->adjusted_src_height);
			if(fit_flag) {
				if(fit_dimension==0) {
					// X dimension is the important one; tweak the Y density.
					newdens_y = newdens_x;
				}
				else if(fit_dimension==1) {
					newdens_x = newdens_y;
				}
				else {
					// Don't know which dimension is important; use the average.
					newdens_x = (newdens_x+newdens_y)/2.0;
					newdens_y = newdens_x;
				}
			}
			iw_set_output_density(ctx,newdens_x,newdens_y,density_code);
		}
	}
	else if(p->density_policy==IWCMD_DENSITY_POLICY_FORCED) {
		iw_set_output_density(ctx,p->density_forced_x,p->density_forced_y,
			IW_DENSITY_UNITS_PER_METER);
	}
}

static int run(struct params_struct *p)
{
	int retval = 0;
	struct iw_context *ctx = NULL;
	int imgtype_read;
	struct iw_iodescr readdescr;
	struct iw_iodescr writedescr;
	char errmsg[200];
	struct iw_init_params init_params;
	const char *s;
	unsigned int profile;
	int maxdepth;

	memset(&init_params,0,sizeof(struct iw_init_params));
	memset(&readdescr,0,sizeof(struct iw_iodescr));
	memset(&writedescr,0,sizeof(struct iw_iodescr));

	if(!p->noinfo) {
		iwcmd_message(p,"%s \xe2\x86\x92 %s\n",p->input_uri.filename,p->output_uri.filename);
	}

	init_params.api_version = IW_VERSION_INT;
	init_params.userdata = (void*)p;
	init_params.mallocfn = my_mallocfn;
	init_params.freefn = my_freefn;

	ctx = iw_create_context(&init_params);
	if(!ctx) goto done;

	iw_set_warning_fn(ctx,my_warning_handler);
#if IW_SUPPORT_ZLIB == 1
	iw_enable_zlib(ctx);
#endif

	// Decide on the output format as early as possible, so we can give up
	// quickly if it's not supported.
	if(p->outfmt==IW_FORMAT_UNKNOWN) {
		if(p->output_uri.scheme==IWCMD_SCHEME_FILE) {
			p->outfmt=iw_detect_fmt_from_filename(p->output_uri.filename);
		}
		else if(p->output_uri.scheme==IWCMD_SCHEME_CLIPBOARD) {
			p->outfmt=IW_FORMAT_BMP;
		}
	}

	if(p->outfmt==IW_FORMAT_UNKNOWN) {
		iw_set_error(ctx,"Unknown output format; use -outfmt.");
		goto done;
	}
	else if(!iw_is_output_fmt_supported(p->outfmt)) {
		s = iw_get_fmt_name(p->outfmt);
		if(!s) s="(unknown)";
		iw_set_errorf(ctx,"Writing %s files is not supported.",s);
		goto done;
	}

	if(p->random_seed!=0 || p->randomize) {
		iw_set_random_seed(ctx,p->randomize, p->random_seed);
	}

	if(p->no_gamma) iw_set_value(ctx,IW_VAL_DISABLE_GAMMA,1);
	if(p->intclamp) iw_set_value(ctx,IW_VAL_INT_CLAMP,1);
	if(p->no_cslabel) iw_set_value(ctx,IW_VAL_NO_CSLABEL,1);
	if(p->noopt_grayscale) iw_set_allow_opt(ctx,IW_OPT_GRAYSCALE,0);
	if(p->noopt_palette) iw_set_allow_opt(ctx,IW_OPT_PALETTE,0);
	if(p->noopt_reduceto8) iw_set_allow_opt(ctx,IW_OPT_16_TO_8,0);
	if(p->noopt_stripalpha) iw_set_allow_opt(ctx,IW_OPT_STRIP_ALPHA,0);
	if(p->noopt_binarytrns) iw_set_allow_opt(ctx,IW_OPT_BINARY_TRNS,0);
	if(p->edge_policy_x>=0) iw_set_value(ctx,IW_VAL_EDGE_POLICY_X,p->edge_policy_x);
	if(p->edge_policy_y>=0) iw_set_value(ctx,IW_VAL_EDGE_POLICY_Y,p->edge_policy_y);
	if(p->grayscale_formula>=0) {
		iw_set_value(ctx,IW_VAL_GRAYSCALE_FORMULA,p->grayscale_formula);
		if(p->grayscale_formula==IW_GSF_WEIGHTED || p->grayscale_formula==IW_GSF_ORDERBYVALUE) {
			iw_set_grayscale_weights(ctx,p->grayscale_weight[0],p->grayscale_weight[1],p->grayscale_weight[2]);
		}
	}
	if(p->page_to_read>0) iw_set_value(ctx,IW_VAL_PAGE_TO_READ,p->page_to_read);
	if(p->include_screen>=0) iw_set_value(ctx,IW_VAL_INCLUDE_SCREEN,p->include_screen);

	if(p->input_uri.scheme==IWCMD_SCHEME_FILE) {
		readdescr.read_fn = my_readfn;
		readdescr.getfilesize_fn = my_getfilesizefn;
		readdescr.fp = (void*)iwcmd_fopen(p->input_uri.filename,"rb");
		if(!readdescr.fp) {
			iw_set_errorf(ctx,"Failed to open for reading (error code=%d)",(int)errno);
			goto done;
		}
	}
#ifdef IW_WINDOWS
	else if(p->input_uri.scheme==IWCMD_SCHEME_CLIPBOARD) {
		if(!iwcmd_open_clipboard_for_read(p,ctx)) goto done;
		readdescr.read_fn = my_clipboard_readfn;
		readdescr.getfilesize_fn = my_clipboard_getfilesizefn;
		readdescr.fp = NULL;
	}
#endif
	else {
		iw_set_error(ctx,"Unsupported input scheme");
		goto done;
	}

	// Decide on the input format.
	if(p->infmt==IW_FORMAT_UNKNOWN) {
		if(p->input_uri.scheme==IWCMD_SCHEME_FILE) {
			p->infmt=detect_fmt_of_file((FILE*)readdescr.fp);
		}
		else if(p->input_uri.scheme==IWCMD_SCHEME_CLIPBOARD) {
			p->infmt=IW_FORMAT_BMP;
		}
	}

	if(p->infmt==IW_FORMAT_UNKNOWN) {
		iw_set_error(ctx,"Unknown input file format.");
		goto done;
	}

	if(p->input_uri.scheme==IWCMD_SCHEME_CLIPBOARD && p->infmt==IW_FORMAT_BMP) {
		iw_set_value(ctx,IW_VAL_BMP_NO_FILEHEADER,1);
	}

	if(!iw_read_file_by_fmt(ctx,&readdescr,p->infmt)) goto done;

#ifdef IW_WINDOWS
	iwcmd_close_clipboard_w(p,ctx);
#endif
	if(p->input_uri.scheme==IWCMD_SCHEME_FILE) {
		fclose((FILE*)readdescr.fp);
	}
	readdescr.fp=NULL;

	imgtype_read = iw_get_value(ctx,IW_VAL_INPUT_IMAGE_TYPE);

	// We have to tell the library the output format, so it can know what
	// kinds of images are allowed (e.g. whether transparency is allowed).
	profile = iw_get_profile_by_fmt(p->outfmt);
	if(p->bmp_trns && p->outfmt==IW_FORMAT_BMP) {
		// TODO: This is part of a "temporary" hack.
		// We support BMP transparency, but with a maximum of 255 opaque
		// colors, instead of the full 256 that ought to be supported.
		profile |= IW_PROFILE_PALETTETRNS|IW_PROFILE_TRANSPARENCY;
	}
	iw_set_output_profile(ctx, profile);

	// Fixup p->depth, if needed.
	maxdepth = p->depth_r;
	if(p->depth_g>maxdepth) maxdepth=p->depth_g;
	if(p->depth_b>maxdepth) maxdepth=p->depth_b;
	if(p->depth_k>maxdepth) maxdepth=p->depth_k;
	if(p->depth_a>maxdepth) maxdepth=p->depth_a;
	if(p->depth < maxdepth) {
		p->depth = maxdepth;
	}

	if(p->depth) {
		if(p->depth_r) iw_set_output_max_color_code(ctx,IW_CHANNELTYPE_RED  ,(1<<p->depth_r)-1);
		if(p->depth_g) iw_set_output_max_color_code(ctx,IW_CHANNELTYPE_GREEN,(1<<p->depth_g)-1);
		if(p->depth_b) iw_set_output_max_color_code(ctx,IW_CHANNELTYPE_BLUE ,(1<<p->depth_b)-1);
		if(p->depth_k) iw_set_output_max_color_code(ctx,IW_CHANNELTYPE_GRAY ,(1<<p->depth_k)-1);
		if(p->depth_a) iw_set_output_max_color_code(ctx,IW_CHANNELTYPE_ALPHA,(1<<p->depth_a)-1);
		iw_set_output_depth(ctx,p->depth);
	}

	if(p->cs_in_set) {
		iw_set_input_colorspace(ctx,&p->cs_in);
	}
	if(p->cs_out_set) {
		iw_set_output_colorspace(ctx,&p->cs_out);
	}

	if(p->dither_family_all>=0)   iw_set_dither_type(ctx,IW_CHANNELTYPE_ALL  ,p->dither_family_all  ,p->dither_subtype_all);
	if(p->dither_family_nonalpha>=0) iw_set_dither_type(ctx,IW_CHANNELTYPE_NONALPHA,p->dither_family_nonalpha,p->dither_subtype_nonalpha);
	if(p->dither_family_red>=0)   iw_set_dither_type(ctx,IW_CHANNELTYPE_RED  ,p->dither_family_red  ,p->dither_subtype_red);
	if(p->dither_family_green>=0) iw_set_dither_type(ctx,IW_CHANNELTYPE_GREEN,p->dither_family_green,p->dither_subtype_green);
	if(p->dither_family_blue>=0)  iw_set_dither_type(ctx,IW_CHANNELTYPE_BLUE ,p->dither_family_blue ,p->dither_subtype_blue);
	if(p->dither_family_gray>=0)  iw_set_dither_type(ctx,IW_CHANNELTYPE_GRAY ,p->dither_family_gray ,p->dither_subtype_gray);
	if(p->dither_family_alpha>=0) iw_set_dither_type(ctx,IW_CHANNELTYPE_ALPHA,p->dither_family_alpha,p->dither_subtype_alpha);

	if(p->color_count_all) iw_set_color_count  (ctx,IW_CHANNELTYPE_ALL  ,p->color_count_all);
	if(p->color_count_nonalpha) iw_set_color_count(ctx,IW_CHANNELTYPE_NONALPHA,p->color_count_nonalpha);
	if(p->color_count_red)   iw_set_color_count(ctx,IW_CHANNELTYPE_RED  ,p->color_count_red);
	if(p->color_count_green) iw_set_color_count(ctx,IW_CHANNELTYPE_GREEN,p->color_count_green);
	if(p->color_count_blue)  iw_set_color_count(ctx,IW_CHANNELTYPE_BLUE ,p->color_count_blue);
	if(p->color_count_gray)  iw_set_color_count(ctx,IW_CHANNELTYPE_GRAY ,p->color_count_gray);
	if(p->color_count_alpha) iw_set_color_count(ctx,IW_CHANNELTYPE_ALPHA,p->color_count_alpha);

	if(p->condgrayscale) {
		if(iw_get_value(ctx,IW_VAL_INPUT_NATIVE_GRAYSCALE)) {
			iw_set_value(ctx,IW_VAL_CVT_TO_GRAYSCALE,1);
		}
	}
	else if(p->grayscale) {
		iw_set_value(ctx,IW_VAL_CVT_TO_GRAYSCALE,1);
	}

	if(p->offset_r_h!=0.0) iw_set_channel_offset(ctx,IW_CHANNELTYPE_RED,  IW_DIMENSION_H,p->offset_r_h);
	if(p->offset_g_h!=0.0) iw_set_channel_offset(ctx,IW_CHANNELTYPE_GREEN,IW_DIMENSION_H,p->offset_g_h);
	if(p->offset_b_h!=0.0) iw_set_channel_offset(ctx,IW_CHANNELTYPE_BLUE, IW_DIMENSION_H,p->offset_b_h);
	if(p->offset_r_v!=0.0) iw_set_channel_offset(ctx,IW_CHANNELTYPE_RED,  IW_DIMENSION_V,p->offset_r_v);
	if(p->offset_g_v!=0.0) iw_set_channel_offset(ctx,IW_CHANNELTYPE_GREEN,IW_DIMENSION_V,p->offset_g_v);
	if(p->offset_b_v!=0.0) iw_set_channel_offset(ctx,IW_CHANNELTYPE_BLUE, IW_DIMENSION_V,p->offset_b_v);

	if(p->apply_bkgd) {

		// iw_set_applybkgd() requires background color to be in a linear
		// colorspace, so convert it (from sRGB) if needed.
		if(!p->no_gamma) {
			struct iw_csdescr cs_srgb;

			// Make an sRGB descriptor to use with iw_convert_sample_to_linear.
			iw_make_srgb_csdescr(&cs_srgb,IW_SRGB_INTENT_PERCEPTUAL);

			p->bkgd.r = iw_convert_sample_to_linear(p->bkgd.r,&cs_srgb);
			p->bkgd.g = iw_convert_sample_to_linear(p->bkgd.g,&cs_srgb);
			p->bkgd.b = iw_convert_sample_to_linear(p->bkgd.b,&cs_srgb);

			if(p->bkgd_checkerboard) {
				p->bkgd2.r = iw_convert_sample_to_linear(p->bkgd2.r,&cs_srgb);
				p->bkgd2.g = iw_convert_sample_to_linear(p->bkgd2.g,&cs_srgb);
				p->bkgd2.b = iw_convert_sample_to_linear(p->bkgd2.b,&cs_srgb);
			}
		}

		iw_set_apply_bkgd(ctx,p->bkgd.r,p->bkgd.g,p->bkgd.b);
		if(p->bkgd_checkerboard) {
			iw_set_bkgd_checkerboard(ctx,p->bkgd_check_size,p->bkgd2.r,p->bkgd2.g,p->bkgd2.b);
			iw_set_bkgd_checkerboard_origin(ctx,p->bkgd_check_origin_x,p->bkgd_check_origin_y);
		}
	}

	if(p->use_bkgd_label) {
		iw_set_value(ctx,IW_VAL_USE_BKGD_LABEL,1);
	}

	p->src_width=iw_get_value(ctx,IW_VAL_INPUT_WIDTH);
	p->src_height=iw_get_value(ctx,IW_VAL_INPUT_HEIGHT);

	// If we're cropping, adjust the src_width and height accordingly.
	if(p->use_crop) {
		if(p->crop_x<0) p->crop_x=0;
		if(p->crop_y<0) p->crop_y=0;
		if(p->crop_x>p->src_width-1) p->crop_x=p->src_width-1;
		if(p->crop_y>p->src_height-1) p->crop_y=p->src_height-1;
		if(p->crop_w<0 || p->crop_w>p->src_width-p->crop_x) p->crop_w=p->src_width-p->crop_x;
		if(p->crop_h<0 || p->crop_h>p->src_height-p->crop_y) p->crop_h=p->src_height-p->crop_y;
		if(p->crop_w<1) p->crop_w=1;
		if(p->crop_h<1) p->crop_h=1;

		p->src_width = p->crop_w;
		p->src_height = p->crop_h;
	}

	figure_out_size_and_density(p,ctx);

	if(p->translate_src_flag) {
		// Convert from dst pixels to src pixels
		if(p->translate_x!=0.0) {
			p->translate_x *= ((double)p->dst_width)/p->src_width;
		}
		if(p->translate_y!=0.0) {
			p->translate_y *= ((double)p->dst_height)/p->src_height;
		}
	}
	if(p->translate_x!=0.0) iw_set_value_dbl(ctx,IW_VAL_TRANSLATE_X,p->translate_x);
	if(p->translate_y!=0.0) iw_set_value_dbl(ctx,IW_VAL_TRANSLATE_Y,p->translate_y);


	// Wait until we know the target image size to set the resize algorithm, so
	// that we can support our "interpolate" option.
	if(p->resize_alg_x.family) {
		if(p->resize_blur_x.interpolate && p->dst_width<p->src_width) {
			// If downscaling, "sharpen" the filter to emulate interpolation.
			p->resize_blur_x.blur *= ((double)p->dst_width)/p->src_width;
		}
		iwcmd_set_resize(ctx,IW_DIMENSION_H,&p->resize_alg_x,&p->resize_blur_x);
	}
	if(p->resize_alg_y.family) {
		if(p->resize_blur_y.interpolate && p->dst_height<p->src_height) {
			p->resize_blur_y.blur *= ((double)p->dst_height)/p->src_height;
		}
		iwcmd_set_resize(ctx,IW_DIMENSION_V,&p->resize_alg_y,&p->resize_blur_y);
	}

	if( (!p->resize_alg_x.family && (p->resize_blur_x.blur!=1.0 || p->resize_blur_x.interpolate)) ||
		(!p->resize_alg_y.family && (p->resize_blur_y.blur!=1.0 || p->resize_blur_y.interpolate)) )
	{
		if(!p->nowarn)
			iwcmd_warning(p,"Warning: -blur option requires -filter\n");
	}

	if(p->noinfo) {
		;
	}
	else if(p->dst_width==p->src_width && p->dst_height==p->src_height) {
		iwcmd_message(p,"Processing: %d\xc3\x97%d\n",p->dst_width,p->dst_height);
	}
	else {
		iwcmd_message(p,"Resizing: %d\xc3\x97%d \xe2\x86\x92 %d\xc3\x97%d\n",p->src_width,p->src_height,
			p->dst_width,p->dst_height);
	}

	iw_set_output_canvas_size(ctx,p->dst_width,p->dst_height);
	if(p->use_crop) {
		iw_set_input_crop(ctx,p->crop_x,p->crop_y,p->crop_w,p->crop_h);
	}

	if(!iw_process_image(ctx)) goto done;

	if(p->compression>0) {
		iw_set_value(ctx,IW_VAL_COMPRESSION,p->compression);
	}
	if(p->interlace) {
		iw_set_value(ctx,IW_VAL_OUTPUT_INTERLACED,1);
	}

	if(p->output_uri.scheme==IWCMD_SCHEME_FILE) {
		writedescr.write_fn = my_writefn;
		writedescr.seek_fn = my_seekfn;
		writedescr.fp = (void*)iwcmd_fopen(p->output_uri.filename,"wb");
		if(!writedescr.fp) {
			iw_set_errorf(ctx,"Failed to open for writing (error code=%d)",(int)errno);
			goto done;
		}
	}
#ifdef IW_WINDOWS
	else if(p->output_uri.scheme==IWCMD_SCHEME_CLIPBOARD) {
		writedescr.write_fn = my_clipboard_writefn;
	}
#endif
	else {
		iw_set_error(ctx,"Unsupported output scheme");
		goto done;
	}

	if(p->zipcmprlevel_set)
		iw_set_value(ctx,IW_VAL_DEFLATE_CMPR_LEVEL,p->zipcmprlevel);

	if(p->outfmt==IW_FORMAT_JPEG) {
		if(p->jpeg_quality>0) iw_set_value(ctx,IW_VAL_JPEG_QUALITY,p->jpeg_quality);
		if(p->jpeg_samp_factor_h>0)
			iw_set_value(ctx,IW_VAL_JPEG_SAMP_FACTOR_H,p->jpeg_samp_factor_h);
		if(p->jpeg_samp_factor_v>0)
			iw_set_value(ctx,IW_VAL_JPEG_SAMP_FACTOR_V,p->jpeg_samp_factor_v);
		if(p->jpeg_arith_coding)
			iw_set_value(ctx,IW_VAL_JPEG_ARITH_CODING,1);
	}
	else if(p->outfmt==IW_FORMAT_WEBP) {
		if(p->webp_quality>=0) iw_set_value_dbl(ctx,IW_VAL_WEBP_QUALITY,p->webp_quality);
	}

	if(!iw_write_file_by_fmt(ctx,&writedescr,p->outfmt)) goto done;

	if(p->output_uri.scheme==IWCMD_SCHEME_FILE) {
		fclose((FILE*)writedescr.fp);
	}
#ifdef IW_WINDOWS
	else if(p->output_uri.scheme==IWCMD_SCHEME_CLIPBOARD) {
		finish_clipboard_write(p,ctx);
	}
#endif
	writedescr.fp=NULL;

	retval = 1;

done:
#ifdef IW_WINDOWS
	iwcmd_close_clipboard_r(p,ctx);
	iwcmd_close_clipboard_w(p,ctx);
#endif
	if(readdescr.fp) fclose((FILE*)readdescr.fp);
	if(writedescr.fp) fclose((FILE*)writedescr.fp);

	if(ctx) {
		if(iw_get_errorflag(ctx)) {
			iwcmd_error(p,"imagew error: %s\n",iw_get_errormsg(ctx,errmsg,200));
		}
	}

	iw_destroy_context(ctx);
	return retval;
}


// Find where a number ends (at a comma, or the end of string),
// and record if it contained a slash.
static int iwcmd_get_number_len(const char *s, int *pslash_pos)
{
	int i;
	for(i=0;s[i];i++) {
		if(s[i]=='/') {
			*pslash_pos = i;
		}
		if(s[i]!=',') continue;
		return i;
	}
	return i;
}

// Returns 0 if no valid number found.
static int iwcmd_parse_number_internal(const char *s,
		   double *presult, int *pcharsread)
{
	int len;
	int slash_pos = -1;

	*presult = 0.0;
	*pcharsread = 0;

	len = iwcmd_get_number_len(s,&slash_pos);
	if(len<1) return 0;
	*pcharsread = len;

	if(slash_pos>=0) {
		// a rational number
		double numer, denom;
		numer = atof(s);
		denom = atof(s+slash_pos+1);
		if(denom==0.0)
			*presult = 0.0;
		else
			*presult = numer/denom;
	}
	else {
		*presult = atof(s);
	}
	return 1;
}

static void iwcmd_parse_number_list(const char *s,
	int max_numbers, // max number of numbers to parse
	double *results, // array of doubles to hold the results
	int *pnumresults) // number of numbers parsed
{
	int n;
	int charsread;
	int curpos=0;
	int ret;

	*pnumresults = 0;
	for(n=0;n<max_numbers;n++) {
		results[n]=0.0;
	}

	for(n=0;n<max_numbers;n++) {
		ret=iwcmd_parse_number_internal(&s[curpos], &results[n], &charsread);
		if(!ret) return;
		(*pnumresults)++;
		curpos+=charsread;
		if(s[curpos]==',') {
			curpos++;
		}
		else {
			return;
		}
	}
}

static double iwcmd_parse_dbl(const char *s)
{
	double result;
	int charsread;
	iwcmd_parse_number_internal(s, &result, &charsread);
	return result;
}

static int iwcmd_round_to_int(double x)
{
	if(x<0.0) return -(int)(0.5-x);
	return (int)(0.5+x);
}

static int iwcmd_parse_int(const char *s)
{
	double result;
	int charsread;
	iwcmd_parse_number_internal(s, &result, &charsread);
	return iwcmd_round_to_int(result);
}

// Parse two numbers separated by a comma.
// If the string doesn't contain enough numbers, some parameters will be
// left unchanged.
static void iwcmd_parse_dbl_pair(const char *s, double *n1, double *n2)
{
	double nums[2];
	int count;

	iwcmd_parse_number_list(s,2,nums,&count);
	if(count>=1) *n1 = nums[0];
	if(count>=2) *n2 = nums[1];
}

// Parse two integers separated by a comma.
// If the string doesn't contain enough numbers, some parameters will be
// left unchanged.
static void iwcmd_parse_int_pair(const char *s, int *i1, int *i2)
{
	double nums[2];
	int count;

	iwcmd_parse_number_list(s,2,nums,&count);
	if(count>=1) *i1 = iwcmd_round_to_int(nums[0]);
	if(count>=2) *i2 = iwcmd_round_to_int(nums[1]);
}

// Parse up to four integers separated by commas.
// If the string doesn't contain enough numbers, some parameters will be
// left unchanged.
static void iwcmd_parse_int_4(const char *s, int *i1, int *i2, int *i3, int *i4)
{
	double nums[4];
	int count;

	iwcmd_parse_number_list(s,4,nums,&count);
	if(count>=1) *i1 = iwcmd_round_to_int(nums[0]);
	if(count>=2) *i2 = iwcmd_round_to_int(nums[1]);
	if(count>=3) *i3 = iwcmd_round_to_int(nums[2]);
	if(count>=4) *i4 = iwcmd_round_to_int(nums[3]);
}

static int hexdigit_value(char d)
{
	if(d>='0' && d<='9') return ((int)d)-'0';
	if(d>='a' && d<='f') return ((int)d)+10-'a';
	if(d>='A' && d<='F') return ((int)d)+10-'A';
	return 0;
}

static double hexvalue1(char d1)
{
	return ((double)hexdigit_value(d1))/15.0;
}

static double hexvalue2(char d1, char d2)
{
	return ((double)(16*hexdigit_value(d1) + hexdigit_value(d2)))/255.0;
}

static double hexvalue4(char d1, char d2, char d3, char d4)
{
	return ((double)(4096*hexdigit_value(d1) + 256*hexdigit_value(d2) +
		16*hexdigit_value(d3) + hexdigit_value(d4)))/65535.0;
}

// Allowed formats: 3 hex digits, 6 hex digits, or 12 hex digits.
static void parse_bkgd_color(struct rgb_color *c, const char *s, size_t s_len)
{
	if(s_len==3) {
		c->r = hexvalue1(s[0]);
		c->g = hexvalue1(s[1]);
		c->b = hexvalue1(s[2]);
	}
	else if(s_len==6) {
		c->r = hexvalue2(s[0],s[1]);
		c->g = hexvalue2(s[2],s[3]);
		c->b = hexvalue2(s[4],s[5]);
	}
	else if(s_len==12) {
		c->r = hexvalue4(s[0],s[1],s[2] ,s[3]);
		c->g = hexvalue4(s[4],s[5],s[6] ,s[7]);
		c->b = hexvalue4(s[8],s[9],s[10],s[11]);
	}
	else {
		// Invalid color description.
		c->r = 1.0;
		c->g = 0.0;
		c->b = 1.0;
	}
}

// 's' is either a single color, or two colors separated with a comma.
static void parse_bkgd(struct params_struct *p, const char *s)
{
	char *cpos;
	cpos = strchr(s,',');
	if(!cpos) {
		// Just a single color
		parse_bkgd_color(&p->bkgd,s,strlen(s));
		return;
	}

	// Two colors
	p->bkgd_checkerboard=1;
	parse_bkgd_color(&p->bkgd,s,cpos-s);
	parse_bkgd_color(&p->bkgd2,cpos+1,strlen(cpos+1));
}

// Find where the "name" ends and the parameters (numbers) begin.
static int iwcmd_get_name_len(const char *s)
{
	int i;
	for(i=0;s[i];i++) {
		if(s[i]>='a' && s[i]<='z') continue;
		if(s[i]>='A' && s[i]<='Z') continue;
		return i;
	}
	return i;
}

static int iwcmd_string_to_resizetype(struct params_struct *p,
	const char *s, struct resize_alg *alg)
{
	int i;
	int len, namelen;
	struct resizetable_struct {
		const char *name;
		int resizetype;
	};
	static const struct resizetable_struct resizetable[] = {
		{"mix",IW_RESIZETYPE_MIX},
		{"nearest",IW_RESIZETYPE_NEAREST},
		{"point",IW_RESIZETYPE_NEAREST},
		{"linear",IW_RESIZETYPE_TRIANGLE},
		{"triangle",IW_RESIZETYPE_TRIANGLE},
		{"quadratic",IW_RESIZETYPE_QUADRATIC},
		{"hermite",IW_RESIZETYPE_HERMITE},
		{"box",IW_RESIZETYPE_BOX},
		{"gaussian",IW_RESIZETYPE_GAUSSIAN},
		{"auto",IW_RESIZETYPE_AUTO},
		{"null",IW_RESIZETYPE_NULL},
		{NULL,0}
	};

	memset(alg,0,sizeof(struct resize_alg));
	alg->param1=0.0;
	alg->param2=0.0;

	for(i=0; resizetable[i].name!=NULL; i++) {
		if(!strcmp(s,resizetable[i].name)) {
			alg->family = resizetable[i].resizetype;
			return 1;
		}
	}

	len=(int)strlen(s);
	namelen=iwcmd_get_name_len(s);

	if(namelen==7 && !strncmp(s,"lanczos",namelen)) {
		if(len>namelen)
			alg->param1 = iwcmd_parse_dbl(&s[namelen]);
		else
			alg->param1 = 3.0;
		alg->family = IW_RESIZETYPE_LANCZOS;
		return 1;
	}
	else if((namelen==4 && !strncmp(s,"hann",namelen)) ||
		    (namelen==7 && !strncmp(s,"hanning",namelen)) )
	{
		if(len>namelen)
			alg->param1 = iwcmd_parse_dbl(&s[namelen]);
		else
			alg->param1 = 4.0;
		alg->family = IW_RESIZETYPE_HANNING;
		return 1;
	}
	else if(namelen==8 && !strncmp(s,"blackman",namelen)) {
		if(len>namelen)
			alg->param1 = iwcmd_parse_dbl(&s[namelen]);
		else
			alg->param1 = 4.0;
		alg->family = IW_RESIZETYPE_BLACKMAN;
		return 1;
	}
	else if(namelen==4 && !strncmp(s,"sinc",namelen)) {
		if(len>namelen)
			alg->param1 = iwcmd_parse_dbl(&s[namelen]);
		else
			alg->param1 = 4.0;
		alg->family = IW_RESIZETYPE_SINC;
		return 1;
	}
	else if(!strcmp(s,"catrom")) {
		alg->family = IW_RESIZETYPE_CUBIC;
		alg->param1 = 0.0; alg->param2 = 0.5;
		return 1;
	}
	else if(!strcmp(s,"mitchell")) {
		alg->family = IW_RESIZETYPE_CUBIC;
		alg->param1 = 1.0/3; alg->param2 = 1.0/3;
		return 1;
	}
	else if(!strcmp(s,"bspline")) {
		alg->family = IW_RESIZETYPE_CUBIC;
		alg->param1 = 1.0; alg->param2 = 0.0;
		return 1;
	}
	else if(namelen==5 && !strncmp(s,"cubic",namelen)) {
		// Format is "cubic<B>,<C>"
		char *cpos;
		if(len < namelen+3) goto done; // error
		cpos = strchr(s,',');
		if(!cpos) goto done;
		alg->param1 = iwcmd_parse_dbl(&s[namelen]);
		alg->param2 = iwcmd_parse_dbl(cpos+1);
		alg->family = IW_RESIZETYPE_CUBIC;
		return 1;
	}
	else if(namelen==4 && !strncmp(s,"keys",namelen)) {
		// Format is "keys<alpha>"
		if(len>namelen)
			alg->param2 = iwcmd_parse_dbl(&s[namelen]);
		else
			alg->param2 = 0.5;
		alg->param1 = 1.0-2.0*alg->param2;
		alg->family = IW_RESIZETYPE_CUBIC;
		return 1;
	}

done:
	iwcmd_error(p,"Unknown resize type \xe2\x80\x9c%s\xe2\x80\x9d\n",s);
	return -1;
}

static int iwcmd_string_to_blurtype(struct params_struct *p,
	const char *s, struct resize_blur *rblur)
{
	int namelen;

	namelen=iwcmd_get_name_len(s);

	if(namelen==1 && !strncmp(s,"x",namelen)) {
		rblur->interpolate = 1;
		if(strlen(s)==1)
			rblur->blur = 1.0;
		else
			rblur->blur = iwcmd_parse_dbl(&s[namelen]);
		return 1;
	}

	rblur->interpolate = 0;
	rblur->blur = iwcmd_parse_dbl(s);
	return 1;
}

static int iwcmd_string_to_dithertype(struct params_struct *p,const char *s,int *psubtype)
{
	int i;
	struct dithertable_struct {
		const char *name;
		int ditherfamily;
		int dithersubtype;
	};
	static const struct dithertable_struct dithertable[] = {
	 {"f"         ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_FS},
	 {"fs"        ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_FS},
	 {"o"         ,IW_DITHERFAMILY_ORDERED,IW_DITHERSUBTYPE_DEFAULT},
	 {"halftone"  ,IW_DITHERFAMILY_ORDERED,IW_DITHERSUBTYPE_HALFTONE},
	 {"r"         ,IW_DITHERFAMILY_RANDOM ,IW_DITHERSUBTYPE_DEFAULT},
	 {"r2"        ,IW_DITHERFAMILY_RANDOM ,IW_DITHERSUBTYPE_SAMEPATTERN},
	 {"jjn"       ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_JJN},
	 {"stucki"    ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_STUCKI},
	 {"burkes"    ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_BURKES},
	 {"sierra"    ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_SIERRA3},
	 {"sierra3"   ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_SIERRA3},
	 {"sierra2"   ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_SIERRA2},
	 {"sierralite",IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_SIERRA42A},
	 {"atkinson"  ,IW_DITHERFAMILY_ERRDIFF,IW_DITHERSUBTYPE_ATKINSON},
	 {"none"      ,IW_DITHERFAMILY_NONE   ,IW_DITHERSUBTYPE_DEFAULT},
	 {NULL        ,0                      ,0}
	};

	for(i=0; dithertable[i].name; i++) {
		if(!strcmp(s,dithertable[i].name)) {
			*psubtype = dithertable[i].dithersubtype;
			return dithertable[i].ditherfamily;
		}
	}

	iwcmd_message(p,"Unknown dither type \xe2\x80\x9c%s\xe2\x80\x9d\n",s);
	*psubtype = IW_DITHERSUBTYPE_DEFAULT;
	return -1;
}


static int iwcmd_string_to_colorspace(struct params_struct *p,
  struct iw_csdescr *cs, const char *s)
{
	int namelen;
	int len;

	len=(int)strlen(s);
	namelen = iwcmd_get_name_len(s);

	if(namelen==5 && len>5 && !strncmp(s,"gamma",namelen)) {
		double g;
		g = iwcmd_parse_dbl(&s[namelen]);
		iw_make_gamma_csdescr(cs,g);
	}
	else if(!strcmp(s,"linear")) {
		iw_make_linear_csdescr(cs);
	}
	else if(len>=4 && !strncmp(s,"srgb",4)) {
		int intent;
		switch(s[4]) {
			case 'p': intent=IW_SRGB_INTENT_PERCEPTUAL; break;
			case 'r': intent=IW_SRGB_INTENT_RELATIVE; break;
			case 's': intent=IW_SRGB_INTENT_SATURATION; break;
			case 'a': intent=IW_SRGB_INTENT_ABSOLUTE; break;
			default:  intent=IW_SRGB_INTENT_PERCEPTUAL; break;
		}
		iw_make_srgb_csdescr(cs,intent);
	}
	else {
		iwcmd_error(p,"Unknown color space \xe2\x80\x9c%s\xe2\x80\x9d\n",s);
		return -1;
	}
	return 1;
}

static int iwcmd_process_noopt(struct params_struct *p, const char *s)
{
	if(!strcmp(s,"all")) {
		p->noopt_grayscale=1;
		p->noopt_palette=1;
		p->noopt_reduceto8=1;
		p->noopt_stripalpha=1;
		p->noopt_binarytrns=1;
	}
	else if(!strcmp(s,"grayscale")) {
		p->noopt_grayscale=1;
	}
	else if(!strcmp(s,"palette")) {
		p->noopt_palette=1;
	}
	else if(!strcmp(s,"reduceto8")) {
		p->noopt_reduceto8=1;
	}
	else if(!strcmp(s,"stripalpha")) {
		p->noopt_stripalpha=1;
	}
	else if(!strcmp(s,"binarytrns")) {
		p->noopt_binarytrns=1;
	}
	else {
		iwcmd_error(p,"Unknown optimization \xe2\x80\x9c%s\xe2\x80\x9d\n",s);
		return 0;
	}

	return 1;
}

static void iwcmd_process_forced_density(struct params_struct *p, const char *s)
{
	double nums[2];
	int count;

	iwcmd_parse_number_list(&s[1],2,nums,&count);
	if(count<1) return;

	p->density_policy = IWCMD_DENSITY_POLICY_FORCED;

	p->density_forced_x = nums[0];
	if(count>=2)
		p->density_forced_y = nums[1];
	else
		p->density_forced_y = nums[0];

	if(s[0]=='c') {
		// Density was given in dots/cm.
		p->pref_units = IW_PREF_UNITS_METRIC;
		p->density_forced_x *= 100.0; // Convert to dots/meter.
		p->density_forced_y *= 100.0;
	}
	else {
		// Density was given in dots/inch.
		p->pref_units = IW_PREF_UNITS_IMPERIAL;
		p->density_forced_x *= 100.0/2.54; // Convert to dots/meter.
		p->density_forced_y *= 100.0/2.54;
	}
}

static int iwcmd_process_density(struct params_struct *p, const char *s)
{
	int namelen;

	namelen=iwcmd_get_name_len(s);

	if(namelen==1 && !strncmp(s,"i",namelen)) {
		iwcmd_process_forced_density(p,s);
	}
	else if(namelen==1 && !strncmp(s,"c",namelen)) {
		iwcmd_process_forced_density(p,s);
	}
	else if(!strcmp(s,"auto")) {
		p->density_policy = IWCMD_DENSITY_POLICY_AUTO;
	}
	else if(!strcmp(s,"none")) {
		p->density_policy = IWCMD_DENSITY_POLICY_NONE;
	}
	else if(!strcmp(s,"keep")) {
		p->density_policy = IWCMD_DENSITY_POLICY_KEEP;
	}
	else if(!strcmp(s,"adjust")) {
		p->density_policy = IWCMD_DENSITY_POLICY_ADJUST;
	}
	else {
		iwcmd_error(p,"Invalid density \xe2\x80\x9c%s\xe2\x80\x9d\n",s);
		return 0;
	}
	return 1;
}

static int process_edge_policy(struct params_struct *p, const char *s)
{
	if(s[0]=='s') return IW_EDGE_POLICY_STANDARD;
	else if(s[0]=='r') return IW_EDGE_POLICY_REPLICATE;
	else if(s[0]=='t') return IW_EDGE_POLICY_TRANSPARENT;
	iwcmd_error(p,"Unknown edge policy\n");
	return -1;
}

static int iwcmd_process_gsf(struct params_struct *p, const char *s)
{
	int namelen;
	int count;

	namelen=iwcmd_get_name_len(s);

	if(!strcmp(s,"s")) p->grayscale_formula=IW_GSF_STANDARD;
	else if(!strcmp(s,"c")) p->grayscale_formula=IW_GSF_COMPATIBLE;
	else if(namelen==1 && !strncmp(s,"w",1)) {
		iwcmd_parse_number_list(&s[1],3,p->grayscale_weight,&count);
		if(count!=3) {
			iwcmd_error(p,"Invalid grayscale formula\n");
			return 0;
		}
		p->grayscale_formula=IW_GSF_WEIGHTED;
	}
	else if(namelen==1 && !strncmp(s,"v",1)) {
		iwcmd_parse_number_list(&s[1],3,p->grayscale_weight,&count);
		if(count<1) {
			iwcmd_error(p,"Invalid grayscale formula\n");
			return 0;
		}
		p->grayscale_formula=IW_GSF_ORDERBYVALUE;
	}
	else {
		iwcmd_error(p,"Unknown grayscale formula\n");
		return 0;
	}
	return 1;
}

static int get_compression_from_name(struct params_struct *p, const char *s)
{
	if(!strcmp(s,"none")) return IW_COMPRESSION_NONE;
	else if(!strcmp(s,"zip")) return IW_COMPRESSION_ZIP;
	else if(!strcmp(s,"lzw")) return IW_COMPRESSION_LZW;
	else if(!strcmp(s,"jpeg")) return IW_COMPRESSION_JPEG;
	else if(!strcmp(s,"rle")) return IW_COMPRESSION_RLE;
	iwcmd_error(p,"Unknown compression \xe2\x80\x9c%s\xe2\x80\x9d\n",s);
	return -1;
}

static void usage_message(struct params_struct *p)
{
	iwcmd_message(p,
		"Usage: imagew [-w <width>] [-h <height>] [options] <in-file> <out-file>\n"
		"Options include -filter, -grayscale, -depth, -cc, -dither, -bkgd, -cs,\n"
		" -crop, -quiet, -version.\n"
		"This program is free software, distributed under the terms of the GNU GPL v3.\n"
		"See the ImageWorsener documentation for more information.\n"
	);
}

static void do_printversion(struct params_struct *p)
{
	char buf[200];
	int buflen;
	int ver;

	buflen = (int)(sizeof(buf)/sizeof(char));

	ver = iw_get_version_int();
	iwcmd_message(p,"ImageWorsener version %s\n",iw_get_version_string(NULL,buf,buflen));
	iwcmd_message(p,"%s\n",iw_get_copyright_string(NULL,buf,buflen));
	iwcmd_message(p,"Features: %d-bit",(int)(8*sizeof(void*)));
	iwcmd_message(p,", %d bits/sample",8*iw_get_sample_size());
	iwcmd_message(p,"\n");

#if IW_SUPPORT_JPEG == 1
	iwcmd_message(p,"Uses libjpeg version %s\n",iw_get_libjpeg_version_string(buf,buflen));
#endif
#if IW_SUPPORT_PNG == 1
	iwcmd_message(p,"Uses libpng version %s\n",iw_get_libpng_version_string(buf,buflen));
#endif
#if IW_SUPPORT_ZLIB == 1
	// TODO: WebP might use zlib, even if IW_SUPPORT_ZLIB is not set.
	iwcmd_message(p,"Uses zlib version %s\n",iw_get_zlib_version_string(buf,buflen));
#endif
#if IW_SUPPORT_WEBP == 1
	iwcmd_message(p,"Uses libwebp encoder v%s",iw_get_libwebp_enc_version_string(buf,buflen));
	iwcmd_message(p,", decoder v%s\n",iw_get_libwebp_dec_version_string(buf,buflen));
#endif
}

enum iwcmd_param_types {
 PT_NONE=0, PT_WIDTH, PT_HEIGHT, PT_DEPTH, PT_DEPTHGRAY, PT_DEPTHALPHA, PT_INPUTCS, PT_CS,
 PT_RESIZETYPE, PT_RESIZETYPE_X, PT_RESIZETYPE_Y,
 PT_BLUR_FACTOR, PT_BLUR_FACTOR_X, PT_BLUR_FACTOR_Y,
 PT_DITHER, PT_DITHERCOLOR, PT_DITHERALPHA, PT_DITHERRED, PT_DITHERGREEN, PT_DITHERBLUE, PT_DITHERGRAY,
 PT_CC, PT_CCCOLOR, PT_CCALPHA, PT_CCRED, PT_CCGREEN, PT_CCBLUE, PT_CCGRAY,
 PT_BKGD, PT_BKGD2, PT_CHECKERSIZE, PT_CHECKERORG, PT_CROP,
 PT_OFFSET_R_H, PT_OFFSET_G_H, PT_OFFSET_B_H, PT_OFFSET_R_V, PT_OFFSET_G_V,
 PT_OFFSET_B_V, PT_OFFSET_RB_H, PT_OFFSET_RB_V, PT_TRANSLATE,
 PT_COMPRESS, PT_JPEGQUALITY, PT_JPEGSAMPLING, PT_JPEGARITH, PT_BMPTRNS,
 PT_WEBPQUALITY, PT_ZIPCMPRLEVEL, PT_INTERLACE,
 PT_RANDSEED, PT_INFMT, PT_OUTFMT, PT_EDGE_POLICY, PT_EDGE_POLICY_X,
 PT_EDGE_POLICY_Y, PT_GRAYSCALEFORMULA,
 PT_DENSITY_POLICY, PT_PAGETOREAD, PT_INCLUDESCREEN, PT_NOINCLUDESCREEN,
 PT_BESTFIT, PT_NOBESTFIT, PT_NORESIZE, PT_GRAYSCALE, PT_CONDGRAYSCALE, PT_NOGAMMA,
 PT_INTCLAMP, PT_NOCSLABEL, PT_NOOPT, PT_USEBKGDLABEL,
 PT_QUIET, PT_NOWARN, PT_NOINFO, PT_VERSION, PT_HELP, PT_ENCODING
};

struct parsestate_struct {
	enum iwcmd_param_types param_type;
	int untagged_param_count;
	int printversion;
	int showhelp;
};

static int process_option_name(struct params_struct *p, struct parsestate_struct *ps, const char *n)
{
	struct opt_struct {
		const char *name;
		enum iwcmd_param_types code;
		int has_param;
	};
	static const struct opt_struct opt_info[] = {
		{"w",PT_WIDTH,1},
		{"width",PT_WIDTH,1},
		{"h",PT_HEIGHT,1},
		{"height",PT_HEIGHT,1},
		{"depth",PT_DEPTH,1},
		{"depthgray",PT_DEPTHGRAY,1},
		{"depthalpha",PT_DEPTHALPHA,1},
		{"inputcs",PT_INPUTCS,1},
		{"cs",PT_CS,1},
		{"filter",PT_RESIZETYPE,1},
		{"filterx",PT_RESIZETYPE_X,1},
		{"filtery",PT_RESIZETYPE_Y,1},
		{"blur",PT_BLUR_FACTOR,1},
		{"blurx",PT_BLUR_FACTOR_X,1},
		{"blury",PT_BLUR_FACTOR_Y,1},
		{"dither",PT_DITHER,1},
		{"dithercolor",PT_DITHERCOLOR,1},
		{"ditheralpha",PT_DITHERALPHA,1},
		{"ditherred",PT_DITHERRED,1},
		{"dithergreen",PT_DITHERGREEN,1},
		{"ditherblue",PT_DITHERBLUE,1},
		{"dithergray",PT_DITHERGRAY,1},
		{"cc",PT_CC,1},
		{"cccolor",PT_CCCOLOR,1},
		{"ccalpha",PT_CCALPHA,1},
		{"ccred",PT_CCRED,1},
		{"ccgreen",PT_CCGREEN,1},
		{"ccblue",PT_CCBLUE,1},
		{"ccgray",PT_CCGRAY,1},
		{"bkgd",PT_BKGD,1},
		{"checkersize",PT_CHECKERSIZE,1},
		{"checkerorigin",PT_CHECKERORG,1},
		{"crop",PT_CROP,1},
		{"offsetred",PT_OFFSET_R_H,1},
		{"offsetgreen",PT_OFFSET_G_H,1},
		{"offsetblue",PT_OFFSET_B_H,1},
		{"offsetrb",PT_OFFSET_RB_H,1},
		{"offsetvred",PT_OFFSET_R_V,1},
		{"offsetvgreen",PT_OFFSET_G_V,1},
		{"offsetvblue",PT_OFFSET_B_V,1},
		{"offsetvrb",PT_OFFSET_RB_V,1},
		{"translate",PT_TRANSLATE,1},
		{"compress",PT_COMPRESS,1},
		{"page",PT_PAGETOREAD,1},
		{"jpegquality",PT_JPEGQUALITY,1},
		{"jpegsampling",PT_JPEGSAMPLING,1},
		{"webpquality",PT_WEBPQUALITY,1},
		{"zipcmprlevel",PT_ZIPCMPRLEVEL,1},
		{"pngcmprlevel",PT_ZIPCMPRLEVEL,1},
		{"randseed",PT_RANDSEED,1},
		{"infmt",PT_INFMT,1},
		{"outfmt",PT_OUTFMT,1},
		{"edge",PT_EDGE_POLICY,1},
		{"edgex",PT_EDGE_POLICY_X,1},
		{"edgey",PT_EDGE_POLICY_Y,1},
		{"density",PT_DENSITY_POLICY,1},
		{"gsf",PT_GRAYSCALEFORMULA,1},
		{"grayscaleformula",PT_GRAYSCALEFORMULA,1},
		{"noopt",PT_NOOPT,1},
		{"encoding",PT_ENCODING,1},
		{"interlace",PT_INTERLACE,0},
		{"bestfit",PT_BESTFIT,0},
		{"nobestfit",PT_NOBESTFIT,0},
		{"noresize",PT_NORESIZE,0},
		{"grayscale",PT_GRAYSCALE,0},
		{"condgrayscale",PT_CONDGRAYSCALE,0},
		{"nogamma",PT_NOGAMMA,0},
		{"intclamp",PT_INTCLAMP,0},
		{"nocslabel",PT_NOCSLABEL,0},
		{"usebkgdlabel",PT_USEBKGDLABEL,0},
		{"includescreen",PT_INCLUDESCREEN,0},
		{"noincludescreen",PT_NOINCLUDESCREEN,0},
		{"jpegarith",PT_JPEGARITH,0},
		{"bmptrns",PT_BMPTRNS,0},
		{"quiet",PT_QUIET,0},
		{"nowarn",PT_NOWARN,0},
		{"noinfo",PT_NOINFO,0},
		{"version",PT_VERSION,0},
		{"help",PT_HELP,0},
		{NULL,PT_NONE,0}
	};
	enum iwcmd_param_types pt;
	int i;

	pt=PT_NONE;

	// Search for the option name.
	for(i=0;opt_info[i].name;i++) {
		if(!strcmp(n,opt_info[i].name)) {
			if(opt_info[i].has_param) {
				// Found option with a parameter. Record it and return.
				ps->param_type=opt_info[i].code;
				return 1;
			}
			// Found parameterless option.
			pt=opt_info[i].code;
			break;
		}
	}

	// Handle parameterless options.
	switch(pt) {
	case PT_BESTFIT:
		p->bestfit=1;
		break;
	case PT_NOBESTFIT:
		p->bestfit=0;
		break;
	case PT_NORESIZE:
		p->noresize_flag=1;
		break;
	case PT_GRAYSCALE:
		p->grayscale=1;
		break;
	case PT_CONDGRAYSCALE:
		p->condgrayscale=1;
		break;
	case PT_NOGAMMA:
		p->no_gamma=1;
		break;
	case PT_INTCLAMP:
		p->intclamp=1;
		break;
	case PT_NOCSLABEL:
		p->no_cslabel=1;
		break;
	case PT_USEBKGDLABEL:
		p->use_bkgd_label=1;
		break;
	case PT_INCLUDESCREEN:
		p->include_screen=1;
		break;
	case PT_NOINCLUDESCREEN:
		p->include_screen=0;
		break;
	case PT_INTERLACE:
		p->interlace=1;
		break;
	case PT_JPEGARITH:
		p->jpeg_arith_coding=1;
		break;
	case PT_BMPTRNS:
		p->bmp_trns=1;
		p->compression=IW_COMPRESSION_RLE;
		break;
	case PT_QUIET:
		p->nowarn=1;
		p->noinfo=1;
		break;
	case PT_NOWARN:
		p->nowarn=1;
		break;
	case PT_NOINFO:
		p->noinfo=1;
		break;
	case PT_VERSION:
		ps->printversion=1;
		break;
	case PT_HELP:
		ps->showhelp=1;
		break;
	default:
		iwcmd_error(p,"Unknown option \xe2\x80\x9c%s\xe2\x80\x9d\n",n);
		return 0;
	}

	return 1;
}

static void iwcmd_read_w_or_h(struct params_struct *p, const char *v,
   int *new_d, int *rel_flag, double *new_rel_d)
{
	if(v[0]=='x') {
		// This is a relative size like "x1.5".
		*rel_flag = 1;
		*new_rel_d = iwcmd_parse_dbl(&v[1]);
	}
	else {
		// This is a number of pixels.
		*new_d = iwcmd_parse_int(v);
		return;
	}
}

static int iwcmd_read_depth(struct params_struct *p, const char *v)
{
	if(strchr(v,',')) {
		iwcmd_parse_int_4(v,&p->depth_r,&p->depth_g,&p->depth_b,&p->depth_a);
	}
	else {
		p->depth=iwcmd_parse_int(v);
	}
	return 1;
}

static int process_option_arg(struct params_struct *p, struct parsestate_struct *ps, const char *v)
{
	int ret;

	switch(ps->param_type) {
	case PT_WIDTH:
		iwcmd_read_w_or_h(p,v,&p->dst_width_req,&p->rel_width_flag,&p->rel_width);
		break;
	case PT_HEIGHT:
		iwcmd_read_w_or_h(p,v,&p->dst_height_req,&p->rel_height_flag,&p->rel_height);
		break;
	case PT_DEPTH:
		ret=iwcmd_read_depth(p,v);
		if(ret<0) return 0;
		break;
	case PT_DEPTHGRAY:
		p->depth_k = iwcmd_parse_int(v);
		break;
	case PT_DEPTHALPHA:
		p->depth_a = iwcmd_parse_int(v);
		break;
	case PT_INPUTCS:
		ret=iwcmd_string_to_colorspace(p,&p->cs_in,v);
		if(ret<0) return 0;
		p->cs_in_set=1;
		break;
	case PT_CS:
		ret=iwcmd_string_to_colorspace(p,&p->cs_out,v);
		if(ret<0) return 0;
		p->cs_out_set=1;
		break;
	case PT_RESIZETYPE:
		ret=iwcmd_string_to_resizetype(p,v,&p->resize_alg_x);
		if(ret<0) return 0;
		p->resize_alg_y=p->resize_alg_x;
		break;
	case PT_RESIZETYPE_X:
		ret=iwcmd_string_to_resizetype(p,v,&p->resize_alg_x);
		if(ret<0) return 0;
		break;
	case PT_RESIZETYPE_Y:
		ret=iwcmd_string_to_resizetype(p,v,&p->resize_alg_y);
		if(ret<0) return 0;
		break;
	case PT_BLUR_FACTOR:
		ret=iwcmd_string_to_blurtype(p,v,&p->resize_blur_x);
		if(ret<0) return 0;
		p->resize_blur_y=p->resize_blur_x;
		break;
	case PT_BLUR_FACTOR_X:
		ret=iwcmd_string_to_blurtype(p,v,&p->resize_blur_x);
		if(ret<0) return 0;
		break;
	case PT_BLUR_FACTOR_Y:
		ret=iwcmd_string_to_blurtype(p,v,&p->resize_blur_y);
		if(ret<0) return 0;
		break;
	case PT_DITHER:
		p->dither_family_all=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_all);
		if(p->dither_family_all<0) return 0;
		break;
	case PT_DITHERCOLOR:
		p->dither_family_nonalpha=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_nonalpha);
		if(p->dither_family_nonalpha<0) return 0;
		break;
	case PT_DITHERALPHA:
		p->dither_family_alpha=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_alpha);
		if(p->dither_family_alpha<0) return 0;
		break;
	case PT_DITHERRED:
		p->dither_family_red=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_red);
		if(p->dither_family_red<0) return 0;
		break;
	case PT_DITHERGREEN:
		p->dither_family_green=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_green);
		if(p->dither_family_green<0) return 0;
		break;
	case PT_DITHERBLUE:
		p->dither_family_blue=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_blue);
		if(p->dither_family_blue<0) return 0;
		break;
	case PT_DITHERGRAY:
		p->dither_family_gray=iwcmd_string_to_dithertype(p,v,&p->dither_subtype_gray);
		if(p->dither_family_gray<0) return 0;
		break;
	case PT_CC:
		p->color_count_all=iwcmd_parse_int(v);
		break;
	case PT_CCCOLOR:
		p->color_count_nonalpha=iwcmd_parse_int(v);
		break;
	case PT_CCALPHA:
		p->color_count_alpha=iwcmd_parse_int(v);
		break;
	case PT_BKGD:
		p->apply_bkgd=1;
		parse_bkgd(p,v);
		break;
	case PT_CHECKERSIZE:
		p->bkgd_check_size=iwcmd_parse_int(v);
		break;
	case PT_CHECKERORG:
		iwcmd_parse_int_pair(v,&p->bkgd_check_origin_x,&p->bkgd_check_origin_y);
		break;
	case PT_CROP:
		p->crop_x = p->crop_y = 0;
		p->crop_w = p->crop_h = -1;
		iwcmd_parse_int_4(v,&p->crop_x,&p->crop_y,&p->crop_w,&p->crop_h);
		p->use_crop=1;
		break;
	case PT_CCRED:
		p->color_count_red=iwcmd_parse_int(v);
		break;
	case PT_CCGREEN:
		p->color_count_green=iwcmd_parse_int(v);
		break;
	case PT_CCBLUE:
		p->color_count_blue=iwcmd_parse_int(v);
		break;
	case PT_CCGRAY:
		p->color_count_gray=iwcmd_parse_int(v);
		break;
	case PT_OFFSET_R_H:
		p->offset_r_h=iwcmd_parse_dbl(v);
		break;
	case PT_OFFSET_G_H:
		p->offset_g_h=iwcmd_parse_dbl(v);
		break;
	case PT_OFFSET_B_H:
		p->offset_b_h=iwcmd_parse_dbl(v);
		break;
	case PT_OFFSET_R_V:
		p->offset_r_v=iwcmd_parse_dbl(v);
		break;
	case PT_OFFSET_G_V:
		p->offset_g_v=iwcmd_parse_dbl(v);
		break;
	case PT_OFFSET_B_V:
		p->offset_b_v=iwcmd_parse_dbl(v);
		break;
	case PT_OFFSET_RB_H:
		// Shortcut for shifting red and blue in opposite directions.
		p->offset_r_h=iwcmd_parse_dbl(v);
		p->offset_b_h= -p->offset_r_h;
		break;
	case PT_OFFSET_RB_V:
		// Shortcut for shifting red and blue vertically in opposite directions.
		p->offset_r_v=iwcmd_parse_dbl(v);
		p->offset_b_v= -p->offset_r_v;
		break;
	case PT_TRANSLATE:
		if(v[0]=='s') {
			iwcmd_parse_dbl_pair(&v[1],&p->translate_x,&p->translate_y);
			p->translate_src_flag=1;
		}
		else {
			iwcmd_parse_dbl_pair(v,&p->translate_x,&p->translate_y);
		}
		break;
	case PT_COMPRESS:
		p->compression=get_compression_from_name(p,v);
		if(p->compression<0) return 0;
		break;
	case PT_PAGETOREAD:
		p->page_to_read = iwcmd_parse_int(v);
		break;
	case PT_JPEGQUALITY:
		p->jpeg_quality=iwcmd_parse_int(v);
		break;
	case PT_JPEGSAMPLING:
		p->jpeg_samp_factor_h = 1;
		p->jpeg_samp_factor_v = 1;
		iwcmd_parse_int_pair(v,&p->jpeg_samp_factor_h,&p->jpeg_samp_factor_v);
		break;
	case PT_WEBPQUALITY:
		p->webp_quality=iwcmd_parse_dbl(v);
		break;
	case PT_ZIPCMPRLEVEL:
		p->zipcmprlevel=iwcmd_parse_int(v);
		p->zipcmprlevel_set=1;
		break;
	case PT_RANDSEED:
		if(v[0]=='r') {
			p->randomize = 1;
		}
		else {
			p->random_seed=iwcmd_parse_int(v);
		}
		break;
	case PT_INFMT:
		p->infmt=get_fmt_from_name(v);
		if(p->infmt==IW_FORMAT_UNKNOWN) {
			iwcmd_error(p,"Unknown format \xe2\x80\x9c%s\xe2\x80\x9d\n",v);
			return 0;
		}
		break;
	case PT_OUTFMT:
		p->outfmt=get_fmt_from_name(v);
		if(p->outfmt==IW_FORMAT_UNKNOWN) {
			iwcmd_error(p,"Unknown format \xe2\x80\x9c%s\xe2\x80\x9d\n",v);
			return 0;
		}
		break;
	case PT_EDGE_POLICY:
		p->edge_policy_x = process_edge_policy(p,v);
		if(p->edge_policy_x<0) return 0;
		p->edge_policy_y = p->edge_policy_x;
		break;
	case PT_EDGE_POLICY_X:
		p->edge_policy_x = process_edge_policy(p,v);
		if(p->edge_policy_x<0) return 0;
		break;
	case PT_EDGE_POLICY_Y:
		p->edge_policy_y = process_edge_policy(p,v);
		if(p->edge_policy_y<0) return 0;
		break;
	case PT_DENSITY_POLICY:
		if(!iwcmd_process_density(p,v)) {
			return 0;
		}
		break;
	case PT_GRAYSCALEFORMULA:
		if(!iwcmd_process_gsf(p,v)) {
			return 0;
		}
		p->grayscale=1;
		break;
	case PT_NOOPT:
		if(!iwcmd_process_noopt(p,v))
			return 0;
		break;
	case PT_ENCODING:
		// Already handled.
		break;

	case PT_NONE:
		// This is presumably the input or output filename.

		if(ps->untagged_param_count==0) {
			p->input_uri.uri = v;
		}
		else if(ps->untagged_param_count==1) {
			p->output_uri.uri = v;
		}
		ps->untagged_param_count++;
		break;

	default:
		iwcmd_error(p,"Internal error: unhandled param\n");
		return 0;
	}

	return 1;
}

static int read_encoding_option(struct params_struct *p, const char *v)
{
	if(!strcmp(v,"ascii")) {
		p->output_encoding = IWCMD_ENCODING_ASCII;
	}
	else if(!strcmp(v,"utf8")) {
#ifdef _UNICODE
		// In Windows, if we set the output mode to UTF-8, we have to print
		// using UTF-16, and let Windows do the conversion.
		p->output_encoding = IWCMD_ENCODING_UTF16;
		p->output_encoding_setmode = IWCMD_ENCODING_UTF8;
#else
		p->output_encoding = IWCMD_ENCODING_UTF8;
#endif
	}
	else if(!strcmp(v,"utf8raw")) {
		p->output_encoding = IWCMD_ENCODING_UTF8;
	}
#ifdef _UNICODE
	else if(!strcmp(v,"utf16")) {
		p->output_encoding = IWCMD_ENCODING_UTF16;
		p->output_encoding_setmode = IWCMD_ENCODING_UTF16;
	}
	else if(!strcmp(v,"utf16raw")) {
		p->output_encoding = IWCMD_ENCODING_UTF16;
	}
#endif
	else {
		return 0;
	}

	return 1;
}

// Figure out what output character encoding to use, and do other
// encoding-related setup as needed.
static void handle_encoding(struct params_struct *p, int argc, char* argv[])
{
	int i;

	// Pre-scan the arguments for an "encoding" option.
	// This has to be done before we process the other options, so that we
	// can correctly print messages while parsing the other options.
	for(i=1;i<argc-1;i++) {
		if(!strcmp(argv[i],"-encoding")) {
			read_encoding_option(p, argv[i+1]);
		}
	}

#if defined(_UNICODE)
	// If the user didn't set an encoding, and this is a Windows Unicode
	// build, use UTF-16.
	if(p->output_encoding==IWCMD_ENCODING_AUTO) {
		p->output_encoding = IWCMD_ENCODING_UTF16;
		p->output_encoding_setmode = IWCMD_ENCODING_UTF16;
	}

#elif !defined(IW_NO_LOCALE)
	// For non-Windows builds with "locale" features enabled.
	setlocale(LC_CTYPE,"");
	// If the user didn't set an encoding, try to detect if we should use
	// UTF-8.
	if(p->output_encoding==IWCMD_ENCODING_AUTO) {
		if(strcmp(nl_langinfo(CODESET), "UTF-8") == 0) {
			p->output_encoding = IWCMD_ENCODING_UTF8;
		}
	}

#endif

	// If we still haven't decided on an encoding, use ASCII.
	if(p->output_encoding==IWCMD_ENCODING_AUTO) {
		p->output_encoding=IWCMD_ENCODING_ASCII;
	}

#ifdef IW_WINDOWS
	// In Windows, set the output mode, if appropriate.
	if(p->output_encoding_setmode==IWCMD_ENCODING_UTF8) {
		_setmode(_fileno(stdout),_O_U8TEXT);
	}
	// TODO: Not sure how much of this works in non-Unicode builds. Need to
	// investigate that (or remove support for non-Unicode builds).
#ifdef _UNICODE
	if(p->output_encoding_setmode==IWCMD_ENCODING_UTF16) {
		_setmode(_fileno(stdout),_O_U16TEXT);
	}
#endif
#endif
}

// Our "schemes" consist of 1-32 lowercase letters, digits, and {+,-,.}.
static int uri_has_scheme(const char *s)
{
	int i;
	for(i=0;s[i];i++) {
		if(s[i]==':' && i>0) return 1;
		if(i>=32) return 0;
		if( (s[i]>='a' && s[i]<='z') || (s[i]>='0' && s[i]<='9') ||
			s[i]=='+' || s[i]=='-' || s[i]=='.')
		{
			;
		}
		else {
			return 0;
		}
	}
	return 0;
}

// Sets the other fields in u, based on u->uri.
static int parse_uri(struct params_struct *p, struct uri_struct *u)
{
	u->filename = u->uri; // By default, point filename to the start of uri.

	if(uri_has_scheme(u->uri)) {
		if(!strncmp("file:",u->uri,5)) {
			u->scheme = IWCMD_SCHEME_FILE;
			u->filename = &u->uri[5];
		}
		else if(!strncmp("clip:",u->uri,5)) {
			u->scheme = IWCMD_SCHEME_CLIPBOARD;
		}
		else {
			iwcmd_error(p,"Don\xe2\x80\x99t understand \xe2\x80\x9c%s\xe2\x80\x9d; try \xe2\x80\x9c"
				"file:%s\xe2\x80\x9d.\n",u->uri,u->uri);
			return 0;
		}
	}
	else {
		// No scheme. Default to "file".
		u->scheme = IWCMD_SCHEME_FILE;
	}
	return 1;
}

static int iwcmd_main(int argc, char* argv[])
{
	struct params_struct p;
	struct parsestate_struct ps;
	int ret;
	int i;
	const char *optname;

	memset(&ps,0,sizeof(struct parsestate_struct));
	ps.param_type=PT_NONE;
	ps.untagged_param_count=0;
	ps.printversion=0;
	ps.showhelp=0;

	memset(&p,0,sizeof(struct params_struct));
	p.dst_width_req = -1;
	p.dst_height_req = -1;
	p.edge_policy_x = -1;
	p.edge_policy_y = -1;
	p.density_policy = IWCMD_DENSITY_POLICY_AUTO;
	p.bkgd_check_size = 16;
	p.bestfit = 0;
	p.offset_r_h=0.0; p.offset_g_h=0.0; p.offset_b_h=0.0;
	p.offset_r_v=0.0; p.offset_g_v=0.0; p.offset_b_v=0.0;
	p.translate_x=0.0; p.translate_y=0.0;
	p.infmt=IW_FORMAT_UNKNOWN;
	p.outfmt=IW_FORMAT_UNKNOWN;
	p.output_encoding=IWCMD_ENCODING_AUTO;
	p.output_encoding_setmode=IWCMD_ENCODING_AUTO;
	p.resize_blur_x.blur = 1.0;
	p.resize_blur_y.blur = 1.0;
	p.webp_quality = -1.0;
	p.include_screen = -1;
	p.dither_family_all = p.dither_family_nonalpha = p.dither_family_alpha = -1;
	p.dither_family_red = p.dither_family_green = p.dither_family_blue = -1;
	p.dither_family_gray = -1;
	p.grayscale_formula = -1;

	handle_encoding(&p,argc,argv);

	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-' && ps.param_type==PT_NONE) {
			optname = &argv[i][1];
			// If the second char is also a '-', ignore it.
			if(argv[i][1]=='-')
				optname = &argv[i][2];
			if(!process_option_name(&p, &ps, optname)) {
				return 1;
			}
		}
		else {
			// Process a parameter of the previous option.

			if(!process_option_arg(&p, &ps, argv[i])) {
				return 1;
			}

			ps.param_type = PT_NONE;
		}
	}

	if(ps.showhelp) {
		usage_message(&p);
		return 0;
	}

	if(ps.printversion) {
		do_printversion(&p);
		return 0;
	}

	if(ps.untagged_param_count!=2 || ps.param_type!=PT_NONE) {
		usage_message(&p);
		return 1;
	}

	if(!parse_uri(&p,&p.input_uri)) {
		return 1;
	}
	if(!parse_uri(&p,&p.output_uri)) {
		return 1;
	}

	ret=run(&p);
	return ret?0:1;
}

#ifdef _UNICODE

int wmain(int argc, WCHAR* argvW[])
{
	int retval;
	int i;
	char **argvUTF8;

	argvUTF8 = (char**)malloc(argc*sizeof(char*));
	if(!argvUTF8) return 1;

	// Convert parameters to UTF-8
	for(i=0;i<argc;i++) {
		argvUTF8[i] = iwcmd_utf16_to_utf8_strdup(argvW[i]);
		if(!argvUTF8[i]) return 1;
	}

	retval = iwcmd_main(argc,argvUTF8);

	for(i=0;i<argc;i++) {
		free(argvUTF8[i]);
	}

	return retval;
}

#else

int main(int argc, char* argv[])
{
	return iwcmd_main(argc,argv);
}

#endif

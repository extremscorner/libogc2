#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "color.h"
#include "cache.h"
#include "video.h"
#include "system.h"

#include "console.h"
#include "consol.h"
#include "usbgecko.h"

#include <stdio.h>
#include <sys/iosupport.h>

static int __console_fstat_r(struct _reent *r,void *fd,struct stat *st);

//---------------------------------------------------------------------------------
static devoptab_t dotab_stdout = {
//---------------------------------------------------------------------------------
	"stdout",	// device name
	0,			// size of file structure
	NULL,		// device open
	NULL,		// device close
	__console_write,	// device write
	NULL,		// device read
	NULL,		// device seek
	__console_fstat_r,	// device fstat
	NULL,		// device stat
	NULL,		// device link
	NULL,		// device unlink
	NULL,		// device chdir
	NULL,		// device rename
	NULL,		// device mkdir
	0,			// dirStateSize
	NULL,		// device diropen_r
	NULL,		// device dirreset_r
	NULL,		// device dirnext_r
	NULL,		// device dirclose_r
	NULL,		// device statvfs_r
	NULL,		// device ftrunctate_r
	NULL,		// device fsync_r
	NULL,		// device data
	NULL,		// device chmod_r
	NULL,		// device fchmod_r
	NULL,		// device rmdir_r
	NULL,		// device lstat_r
	NULL,		// device utimes_r
	NULL,		// device fpathconf_r
	NULL,		// device pathconf_r
	NULL,		// device symlink_r
	NULL,		// device readlink_r
};

//color table
static const unsigned int color_table[] =
{
  0x10801080,		// 30 normal black
  0x316D31B8,		// 31 normal red
  0x515B5151,		// 32 normal green
  0x71487189,		// 33 normal yellow
  0x1DB81D77,		// 34 normal blue
  0x3DA53DAF,		// 35 normal magenta
  0x5D935D48,		// 36 normal cyan
  0xB580B580,		// 37 normal white
  0x7E807E80,		// 30 bright black
  0x515A51F0,		// 31 bright red
  0x91369122,		// 32 bright green
  0xD210D292,		// 33 bright yellow
  0x29F0296E,		// 34 bright blue
  0x6ACA6ADE,		// 35 bright magenta
  0xAAA6AA10,		// 36 bright cyan
  0xEB80EB80,		// 37 bright white
};

static u32 do_xfb_copy = FALSE;
static struct _console_data_s _console_data;
static struct _console_data_s *curr_con = NULL;
static void *_console_buffer = NULL;

static FILE *stdcon = NULL;

extern u8 console_font_8x16[];

void __console_vipostcb(u32 retraceCnt)
{
	u32 ycnt,xcnt, fb_stride;
	u32 *fb,*ptr;

	do_xfb_copy = TRUE;

	ptr = curr_con->destbuffer;
	fb = VIDEO_GetCurrentFramebuffer()+(curr_con->target_y*curr_con->tgt_stride) + curr_con->target_x*VI_DISPLAY_PIX_SZ;
	fb_stride = curr_con->tgt_stride/4 - (curr_con->con_xres/VI_DISPLAY_PIX_SZ);

	for(ycnt=curr_con->con_yres;ycnt>0;ycnt--)
	{
		for(xcnt=curr_con->con_xres;xcnt>0;xcnt-=VI_DISPLAY_PIX_SZ)
		{
			*fb++ = *ptr++;
		}
		fb += fb_stride;
	}

	do_xfb_copy = FALSE;
}


static void __console_drawc(int c)
{
	console_data_s *con;
	int ay;
	unsigned int *ptr;
	unsigned char *pbits;
	unsigned char bits;
	unsigned int color;
	unsigned int fgcolor, bgcolor;
	unsigned int nextline;

	if(do_xfb_copy==TRUE) return;
	if(!curr_con) return;
	con = curr_con;

	ptr = (unsigned int*)(con->destbuffer + ( con->con_stride *  con->cursor_row * FONT_YSIZE ) + ((con->cursor_col * FONT_XSIZE / 2) * 4));
	pbits = &con->font[c * FONT_YSIZE];
	nextline = con->con_stride/4 - 4;
	fgcolor = con->foreground;
	bgcolor = con->background;

	for (ay = 0; ay < FONT_YSIZE; ay++)
	{
		/* hard coded loop unrolling ! */
		/* this depends on FONT_XSIZE = 8*/
#if FONT_XSIZE == 8
		bits = *pbits++;

		/* bits 1 & 2 */
		if ( bits & 0x80)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x40)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* bits 3 & 4 */
		if ( bits & 0x20)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x10)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* bits 5 & 6 */
		if ( bits & 0x08)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x04)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* bits 7 & 8 */
		if ( bits & 0x02)
			color = fgcolor & 0xFFFF00FF;
		else
			color = bgcolor & 0xFFFF00FF;
		if (bits & 0x01)
			color |= fgcolor  & 0x0000FF00;
		else
			color |= bgcolor  & 0x0000FF00;
		*ptr++ = color;

		/* next line */
		ptr += nextline;
#else
#endif
	}
}
static void __console_clear_line( int line, int from, int to ) {
	console_data_s *con;
	unsigned int c;
	unsigned int *p;
	unsigned int x_pixels;
	unsigned int px_per_col = FONT_XSIZE/2;
	unsigned int line_height = FONT_YSIZE;
	unsigned int line_width;
	
	if( !(con = curr_con) ) return;
	// For some reason there are xres/2 pixels per screen width
  x_pixels = con->con_xres / 2;
	
	line_width = (to - from)*px_per_col;
	p = (unsigned int*)con->destbuffer;
	
	// Move pointer to the current line and column offset
	p += line*(FONT_YSIZE*x_pixels) + from*px_per_col;
	
	// Clears 1 line of pixels at a time, line_height times
  while( line_height-- ) {
  	c = line_width;
    while( c-- )
      *p++ = con->background;
    p -= line_width;
    p += x_pixels;
  }
}
static void __console_clear(void)
{
	console_data_s *con;
	unsigned int c;
	unsigned int *p;

	if( !(con = curr_con) ) return;

	c = (con->con_xres*con->con_yres)/2;
	p = (unsigned int*)con->destbuffer;
	
	while(c--)
		*p++ = con->background;

	con->cursor_row = 0;
	con->cursor_col = 0;
	con->saved_row = 0;
	con->saved_col = 0;
}
static void __console_clear_from_cursor(void) {
	console_data_s *con;
	int cur_row;
	
  if( !(con = curr_con) ) return;
	cur_row = con->cursor_row;
	
  __console_clear_line( cur_row, con->cursor_col, con->con_cols );
  
  while( cur_row++ < con->con_rows )
    __console_clear_line( cur_row, 0, con->con_cols );
  
}
static void __console_clear_to_cursor(void) {
	console_data_s *con;
	int cur_row;
	
  if( !(con = curr_con) ) return;
	cur_row = con->cursor_row;
	
  __console_clear_line( cur_row, 0, con->cursor_col );
  
  while( cur_row-- )
    __console_clear_line( cur_row, 0, con->con_cols );
}

void __console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;
	console_data_s *con = &_console_data;

	_CPU_ISR_Disable(level);

	con->destbuffer = framebuffer;
	con->con_xres = xres;
	con->con_yres = yres;
	con->con_cols = xres / FONT_XSIZE;
	con->con_rows = yres / FONT_YSIZE;
	con->con_stride = con->tgt_stride = stride;
	con->target_x = xstart;
	con->target_y = ystart;

	con->font = console_font_8x16;

	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;

	curr_con = con;

	__console_clear();

	_CPU_ISR_Restore(level);

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
}

void __console_init_ex(void *conbuffer,int tgt_xstart,int tgt_ystart,int tgt_stride,int con_xres,int con_yres,int con_stride)
{
	unsigned int level;
	console_data_s *con = &_console_data;

	_CPU_ISR_Disable(level);

	con->destbuffer = conbuffer;
	con->target_x = tgt_xstart;
	con->target_y = tgt_ystart;
	con->con_xres = con_xres;
	con->con_yres = con_yres;
	con->tgt_stride = tgt_stride;
	con->con_stride = con_stride;
	con->con_cols = con_xres / FONT_XSIZE;
	con->con_rows = con_yres / FONT_YSIZE;
	con->cursor_row = 0;
	con->cursor_col = 0;
	con->saved_row = 0;
	con->saved_col = 0;

	con->font = console_font_8x16;

	con->foreground = COLOR_WHITE;
	con->background = COLOR_BLACK;

	curr_con = con;

	__console_clear();

	VIDEO_SetPostRetraceCallback(__console_vipostcb);

	_CPU_ISR_Restore(level);

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
}

static int __console_parse_escsequence(const char *pchr)
{
	char chr;
	console_data_s *con;
	int i;
	int parameters[3];
	int para;

	if(!curr_con) return -1;
	con = curr_con;

	/* set default value */
	para = 0;
	parameters[0] = 0;
	parameters[1] = 0;
	parameters[2] = 0;

	/* scan parameters */
	i = 0;
	chr = *pchr;
	while( (para < 3) && (chr >= '0') && (chr <= '9') )
	{
		while( (chr >= '0') && (chr <= '9') )
		{
			/* parse parameter */
			parameters[para] *= 10;
			parameters[para] += chr - '0';
			pchr++;
			i++;
			chr = *pchr;
		}
		para++;

		if( *pchr == ';' )
		{
		  /* skip parameter delimiter */
		  pchr++;
			i++;
		}
		chr = *pchr;
	}

	/* get final character */
	chr = *pchr++;
	i++;
	switch(chr)
	{
		/////////////////////////////////////////
		// Cursor directional movement
		/////////////////////////////////////////
		case 'A':
		{
			curr_con->cursor_row -= parameters[0];
			if(curr_con->cursor_row < 0) curr_con->cursor_row = 0;
			break;
		}
		case 'B':
		{
			curr_con->cursor_row += parameters[0];
			if(curr_con->cursor_row >= curr_con->con_rows) curr_con->cursor_row = curr_con->con_rows - 1;
			break;
		}
		case 'C':
		{
			curr_con->cursor_col += parameters[0];
			if(curr_con->cursor_col >= curr_con->con_cols) curr_con->cursor_col = curr_con->con_cols - 1;
			break;
		}
		case 'D':
		{
			curr_con->cursor_col -= parameters[0];
			if(curr_con->cursor_col < 0) curr_con->cursor_col = 0;
			break;
		}
		/////////////////////////////////////////
		// Cursor position movement
		/////////////////////////////////////////
		case 'H':
		case 'f':
		{
			curr_con->cursor_col = parameters[1];
			curr_con->cursor_row = parameters[0];
			if(curr_con->cursor_row >= curr_con->con_rows) curr_con->cursor_row = curr_con->con_rows - 1;
			if(curr_con->cursor_col >= curr_con->con_cols) curr_con->cursor_col = curr_con->con_cols - 1;
			break;
		}
		/////////////////////////////////////////
		// Screen clear
		/////////////////////////////////////////
		case 'J':
		{
			if( parameters[0] == 0 )
        __console_clear_from_cursor();
			if( parameters[0] == 1 )
        __console_clear_to_cursor();
			if( parameters[0] == 2 )
        __console_clear();
        
			break;
		}
		/////////////////////////////////////////
		// Line clear
		/////////////////////////////////////////
		case 'K':
		{
			if( parameters[0] == 0 )
        __console_clear_line( curr_con->cursor_row, curr_con->cursor_col, curr_con->con_cols );
			if( parameters[0] == 1 )
        __console_clear_line( curr_con->cursor_row, 0, curr_con->cursor_col );
			if( parameters[0] == 2 )
        __console_clear_line( curr_con->cursor_row, 0, curr_con->con_cols);
        
			break;
		}
		/////////////////////////////////////////
		// Save cursor position
		/////////////////////////////////////////
		case 's':
		{
			con->saved_col = con->cursor_col;
			con->saved_row = con->cursor_row;
			break;
		}
		/////////////////////////////////////////
		// Load cursor position
		/////////////////////////////////////////
		case 'u':
			con->cursor_col = con->saved_col;
			con->cursor_row = con->saved_row;
			break;
		/////////////////////////////////////////
		// SGR Select Graphic Rendition
		/////////////////////////////////////////
		case 'm':
		{
			// handle 30-37,39 for foreground color changes
			if( (parameters[0] >= 30) && (parameters[0] <= 39) )
			{
				parameters[0] -= 30;

				//39 is the reset code
				if(parameters[0] == 9){
				    parameters[0] = 15;
				}
				else if(parameters[0] > 7){
					parameters[0] = 7;
				}

				if(parameters[1] == 1)
				{
					// Intensity: Bold makes color bright
					parameters[0] += 8;
				}
				con->foreground = color_table[parameters[0]];
			}
			// handle 40-47 for background color changes
			else if( (parameters[0] >= 40) && (parameters[0] <= 47) )
			{
				parameters[0] -= 40;

				if(parameters[1] == 1)
				{
					// Intensity: Bold makes color bright
					parameters[0] += 8;
				}
				con->background = color_table[parameters[0]];
			}
		  break;
		}
	}

	return(i);
}

ssize_t __console_write(struct _reent *r,void *fd,const char *ptr,size_t len)
{
	size_t i;
	const char *tmp = ptr;
	console_data_s *con;
	char chr;

	if(!curr_con) return -1;
	con = curr_con;
	if(!tmp || len<=0) return -1;

	i = 0;
	while(*tmp!='\0' && i<len)
	{
		chr = *tmp++;
		i++;
		if ( (chr == 0x1b) && (*tmp == '[') )
		{
			/* escape sequence found */
			int k;

			tmp++;
			i++;
			k = __console_parse_escsequence(tmp);
			tmp += k;
			i += k;
		}
		else
		{
			switch(chr)
			{
				case '\n':
					con->cursor_row++;
					con->cursor_col = 0;
					break;
				case '\r':
					con->cursor_col = 0;
					break;
				case '\b':
					con->cursor_col--;
					if(con->cursor_col < 0)
					{
						con->cursor_col = 0;
					}
					break;
				case '\f':
					con->cursor_row++;
					break;
				case '\t':
					if(con->cursor_col%TAB_SIZE) con->cursor_col += (con->cursor_col%TAB_SIZE);
					else con->cursor_col += TAB_SIZE;
					break;
				default:
					__console_drawc(chr);
					con->cursor_col++;

					if( con->cursor_col >= con->con_cols)
					{
						/* if right border reached wrap around */
						con->cursor_row++;
						con->cursor_col = 0;
					}
			}
		}

		if( con->cursor_row >= con->con_rows)
		{
			/* if bottom border reached scroll */
			memcpy(con->destbuffer,
				con->destbuffer+con->con_stride*(FONT_YSIZE*FONT_YFACTOR+FONT_YGAP),
				con->con_stride*con->con_yres-FONT_YSIZE);

			unsigned int cnt = (con->con_stride * (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP))/4;
			unsigned int *ptr = (unsigned int*)(con->destbuffer + con->con_stride * (con->con_yres - FONT_YSIZE));
			while(cnt--)
				*ptr++ = con->background;
			con->cursor_row--;
		}
	}

	return i;
}

static ssize_t __console_write_r(struct _reent *r,void *fd,const char *ptr,size_t len)
{
	ssize_t ret = __console_write(r,fd,ptr,len);

	if(stdcon && !ftrylockfile(stdcon)) {
		if(ret!=-1) _fwrite_unlocked_r(r,ptr,1,len,stdcon);
		else ret = _fwrite_unlocked_r(r,ptr,1,len,stdcon);
		funlockfile(stdcon);
	}
	return ret;
}

static int __console_fstat_r(struct _reent *r,void *fd,struct stat *st)
{
	memset(st, 0, sizeof(struct stat));
	st->st_mode = S_IFCHR;
	return 0;
}

void CON_Init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	__console_init(framebuffer,xstart,ystart,xres,yres,stride);
}

s32 CON_InitEx(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight)
{
	VIDEO_SetPostRetraceCallback(NULL);
	if(_console_buffer)
		free(_console_buffer);
	
	_console_buffer = malloc(conWidth*conHeight*VI_DISPLAY_PIX_SZ);
	if(!_console_buffer) return -1;

	__console_init_ex(_console_buffer,conXOrigin,conYOrigin,rmode->fbWidth*VI_DISPLAY_PIX_SZ,conWidth,conHeight,conWidth*VI_DISPLAY_PIX_SZ);

	return 0;
}

void CON_GetMetrics(int *cols, int *rows)
{
	if(curr_con) {
		*cols = curr_con->con_cols;
		*rows = curr_con->con_rows;
	}
}

void CON_GetPosition(int *col, int *row)
{
	if(curr_con) {
		*col = curr_con->cursor_col;
		*row = curr_con->cursor_row;
	}
}

static s32 __gecko_chan = -1;

static int __gecko_write(void *c,const char *buf,int n)
{
	s32 chan = *(s32*)c;

	if(!usb_isgeckoalive(chan)) {
		errno = ENXIO;
		return -1;
	}
	return usb_sendbuffer(chan,buf,n);
}

static int __gecko_write_safe(void *c,const char *buf,int n)
{
	s32 chan = *(s32*)c;

	if(!usb_isgeckoalive(chan)) {
		errno = ENXIO;
		return -1;
	}
	return usb_sendbuffer_safe(chan,buf,n);
}

static int __gecko_close(void *c)
{
	*(s32*)c = -1;
	return 0;
}

void CON_EnableGecko(s32 chan,bool safe)
{
	if(chan<0 && __gecko_chan==-1) return;
	if(chan>=0 && (__gecko_chan==chan || !usb_isgeckoalive(chan))) return;

	fflush(stdout);
	fflush(stderr);

	fclose(stdcon);
	stdcon = NULL;

	if(chan<0) return;
	__gecko_chan = chan;

	stdcon = funopen(&__gecko_chan, NULL, safe ? __gecko_write_safe : __gecko_write, NULL, __gecko_close);
	if(!stdcon) return;
	setlinebuf(stdcon);

	dotab_stdout.write_r = __console_write_r;

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
}

extern void __SYS_EnableBarnacle(s32 chn,u32 dev);

extern s32 InitializeUART(void);
extern s32 WriteUARTN(void *buf,u32 len);

static int __uart_write(void *c,const char *buf,int n)
{
	if(InitializeUART()!=0)
		return -1;

	if(WriteUARTN((void*)buf,n)!=0)
		return -1;

	return n;
}

void CON_EnableBarnacle(s32 chan,s32 dev)
{
	if(chan<0 && __gecko_chan>=0) return;

	fflush(stdout);
	fflush(stderr);

	fclose(stdcon);
	stdcon = NULL;

	if(chan<0) return;
	__SYS_EnableBarnacle(chan, dev);

	stdcon = fwopen(NULL, __uart_write);
	if(!stdcon) return;
	setlinebuf(stdcon);

	dotab_stdout.write_r = __console_write_r;

	devoptab_list[STD_OUT] = &dotab_stdout;
	devoptab_list[STD_ERR] = &dotab_stdout;
}

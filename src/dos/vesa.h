#ifndef __VESA20_H_INCLUDED
#define __VESA20_H_INCLUDED

#if defined __WATCOMC__
#define VBEinit640x480x8() NULL;
#define VBEshutdown()
#else
uint8_t *VBEinit640x480x8(void);
void     VBEshutdown(void);
#endif

#endif

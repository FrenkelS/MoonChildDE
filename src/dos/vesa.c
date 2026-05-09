// This code is based on https://github.com/tobijk/vesa-dos-djgpp


/*****************************************************************************
*               VBE 2.0 detection and routines for DJGPP C.                  *
*****************************************************************************/

#include <dpmi.h>
#include <go32.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nearptr.h>

#include "vesa.h"


typedef struct {
    char            vbe_signature[4];     // 'VESA' 4 byte signature
    int16_t         vbe_version;          // VBE version number
    char           *oem_string_ptr;       // Pointer to OEM string
    uint32_t        capabilities;         // Capabilities of video card
    uint16_t       *video_mode_ptr;       // Pointer to supported modes
    int16_t         total_memory;         // Number of 64kb memory blocks
    // added for VBE 2.0
    int16_t         oem_software_rev;     // OEM Software revision number
    char           *oem_vendor_name_ptr;  // Pointer to Vendor Name string
    char           *oem_product_name_ptr; // Pointer to Product Name string
    char           *oem_product_rev_ptr;  // Pointer to Product Revision str
    int16_t         vbe_af_version;
    uint16_t       *accelerated_video_mode_ptr;
    uint8_t         reserved[216];        // Pad to 256 byte block size
    char            oem_data[256];        // Scratch pad for OEM data
} __attribute__ ((packed)) VBEInfo;

typedef char assertSizeOfVBEInfo[sizeof(VBEInfo) == 512 ? 1 : -1];


typedef struct {
    // Mandatory information for all VBE revisions:
    uint16_t  mode_attributes;      // mode attributes
    uint8_t   win_a_attributes;     // window A attributes
    uint8_t   win_b_attributes;     // window B attributes
    uint16_t  win_granularity;      // window granularity
    uint16_t  win_size;             // window size
    uint16_t  win_a_segment;        // window A start segment
    uint16_t  win_b_segment;        // window B start segment
    uint32_t  win_func_ptr;         // pointer to window function
    uint16_t  bytes_per_scan_line;  // bytes per scan line

    // Mandatory information for VBE 1.2 and above:
    uint16_t  x_resolution;          // horizontal resolution in pixels or chars
    uint16_t  y_resolution;          // vertical resolution in pixels or chars
    uint8_t   x_char_size;           // character cell width in pixels
    uint8_t   y_char_size;           // character cell height in pixels
    uint8_t   number_of_planes;      // number of memory planes
    uint8_t   bits_per_pixel;        // bits per pixel
    uint8_t   number_of_banks;       // number of banks
    uint8_t   memory_model;          // memory model type
    uint8_t   bank_size;             // bank size in KB
    uint8_t   number_of_image_pages; // number of images
    uint8_t   reserved;              // reserved for page function

    // Direct Color fields (required for direct/6 and YUV/7 memory models)
    uint8_t   red_mask_size;          // size of direct color red mask in bits
    uint8_t   red_field_position;     // bit position of lsb of red mask
    uint8_t   green_mask_size;        // size of direct color green mask in bits
    uint8_t   green_field_position;   // bit position of lsb of green mask
    uint8_t   blue_mask_size;         // size of direct color blue mask in bits
    uint8_t   blue_field_position;    // bit position of lsb of blue mask
    uint8_t   rsvd_mask_size;         // size of direct color reserved mask in bits
    uint8_t   rsvd_field_position;    // bit position of lsb of reserved mask
    uint8_t   direct_color_mode_info; // direct color mode attributes

    // Mandatory information for VBE 2.0 and above:
    uint32_t   phys_base_ptr;          // physical address for flat frame buffer
    uint32_t   off_screen_mem_offset;  // pointer to start of off screen memory
    uint16_t   off_screen_mem_size;    // amount of off screen memory in 1k units
    uint8_t    reserved_buf[206];
} __attribute__ ((packed)) VBEModeInfo;

typedef char assertSizeOfVBEModeInfo[sizeof(VBEModeInfo) == 256 ? 1 : -1];


static uint32_t  lfb_linear_address;
static uint16_t  lfb_selector;
static int       total_video_memory;


/*****************************************************************************
*                                                                            *
* VBEInit640x480x8 silently enters the desired mode and returns a pointer to *
* the linear frame buffer                                                    *
*                                                                            *
*****************************************************************************/
uint8_t *VBEinit640x480x8(void)
{
    VBEInfo     vbe_info;
    __dpmi_regs r;

    // we want VBE 2.0+ info
    strncpy(vbe_info.vbe_signature, "VBE2", 4);

    // get SuperVGA information
    r.x.ax = 0x4F00;
    r.x.es = (__tb >> 4) & 0xFFFF;
    r.x.di = __tb & 0x0F;
    dosmemput(&vbe_info, sizeof(vbe_info), __tb);
    __dpmi_int(0x10, &r);
    if (r.x.ax != 0x004F)
        return NULL;
    dosmemget(__tb, sizeof(vbe_info), &vbe_info);

    if (strncmp(vbe_info.vbe_signature, "VESA", 4) != 0)
        return NULL;  // VESA ?

    // only available in VBE 2.0+
    if (vbe_info.vbe_version < 0x0200)
        return NULL;

    uint32_t video_mode_offset = ((((uint32_t)vbe_info.video_mode_ptr >> 16) & 0xFFFF) << 4)
                                 + ((uint32_t)vbe_info.video_mode_ptr & 0xFFFF);
    total_video_memory = vbe_info.total_memory * 64 * 1024;

    uint16_t vbe_mode;
    uint32_t lfb_ptr;
    for (int counter = 0; ; counter++) {
        dosmemget(video_mode_offset + (counter * sizeof(vbe_mode)),
                  sizeof(vbe_mode), &vbe_mode);
        if (vbe_mode == 0xFFFF)
            return NULL;

        // get SuperVGA mode information
        r.x.ax = 0x4F01;
        r.x.cx = vbe_mode;
        r.x.es = (__tb >> 4) & 0xFFFF;
        r.x.di = __tb & 0x0F;
        __dpmi_int(0x10, &r);
        if (r.x.ax != 0x004F)
            return NULL;

        VBEModeInfo vbe_mode_info;
        dosmemget(__tb, sizeof(vbe_mode_info), &vbe_mode_info);

        if (vbe_mode_info.x_resolution == 640
         && vbe_mode_info.y_resolution == 480
         && vbe_mode_info.bits_per_pixel == 8) {
            lfb_ptr = vbe_mode_info.phys_base_ptr;
            break;
         }
    }

    // set SuperVGA video mode
    r.x.ax = 0x4F02;
    r.x.bx = vbe_mode | 0x4000;
    __dpmi_int(0x10, &r);
    if (r.x.ax != 0x004F)
        return NULL;

    __dpmi_meminfo m;
    m.size    = total_video_memory;
    m.address = lfb_ptr;
    __dpmi_physical_address_mapping(&m);
    __dpmi_lock_linear_region(&m);

    lfb_linear_address = m.address;
    lfb_selector       = __dpmi_allocate_ldt_descriptors(1);

    __dpmi_set_segment_base_address(lfb_selector, lfb_linear_address);
    __dpmi_set_segment_limit(lfb_selector, total_video_memory - 1);

    return (uint8_t*)(lfb_linear_address + __djgpp_conventional_base);
}


/*****************************************************************************
*                                                                            *
*                VBEshutdown cleans up descriptors, memory, etc.             *
*                                                                            *
*****************************************************************************/
void VBEshutdown(void)
{
    __dpmi_meminfo m;
    m.size    = total_video_memory;
    m.address = lfb_linear_address;
    __dpmi_unlock_linear_region(&m);
    __dpmi_free_physical_address_mapping(&m);
    __dpmi_free_ldt_descriptor(lfb_selector);
}

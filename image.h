#ifndef __SWORD_IMG_H_
#define __SWORD_IMG_H_

#include <unistd.h>

typedef struct
{
	int width;
	int height;
	int bpp;
	int type;
	int r_mask;
	int g_mask;
	int b_mask;
	int stride;
	int pages_max;
	int pages_cur;
	void* handle;
	void* handle_mp;
}image_handle;

#define IMAGE_ERR_SUCCESS 	(0)
#define IMAGE_ERR_UNKNOWN 	(1) //image type unknown
#define IMAGE_ERR_LOAD 		(2) //load image failed
#define IMAGE_ERR_PAGE 		(3) //get page failed

//call in beginning
void image_init();

//release
void image_exit();


//load an image, return handle to phandle
// 0 means success
// 1 means unknown type error
int image_load(const char* file, image_handle* phandle);

//unload an image
void image_unload(image_handle* handle);


//get raw bits of image 
void* image_getbits(image_handle* handle);


void image_copy(image_handle* handle, void* dstMemory, int src_x, int src_y, 
		int dst_x, int dst_y, int dst_stride, int width, int height);


void image_copy_vflip(image_handle* handle, void* dstMemory, int src_x, int src_y, 
		int dst_x, int dst_y, int dst_stride, int width, int height);

//convert bits to an image_handle structure
//return a new handle
void image_frombits(void* memory_bits, image_handle* ret_handle, int width, int height, int stride);


//rescale to new size, return a new handle
void image_rescale(image_handle* handle_src, image_handle* handle_dst, int new_width, int new_height);

//get one page from multi-page image (such as GIF)
//return a normal image handle
void image_mp_lock_page(image_handle* handle, int page);

#endif

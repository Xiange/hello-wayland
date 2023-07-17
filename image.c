#include <FreeImage.h>
#include <stdio.h>
#include <string.h>
#include "image.h"

void image_init()
{
	FreeImage_Initialise(TRUE);
}

void image_exit()
{
	FreeImage_DeInitialise();
}

static void image_getinfo(image_handle* phandle)
{

	FIBITMAP * dib=phandle->handle;
	int width=FreeImage_GetWidth(dib);
	int height=FreeImage_GetHeight(dib);

	int bpp=FreeImage_GetBPP(dib);
	int rmask=FreeImage_GetRedMask(dib);
	int bmask=FreeImage_GetBlueMask(dib);
	int gmask=FreeImage_GetGreenMask(dib);
	int size=FreeImage_GetMemorySize(dib);
	int line=FreeImage_GetLine(dib);
		fprintf(stderr, "Width=%d, Heigh=%d\n", width, height);
		fprintf(stderr, "BPP=%d, rmask=%x, gmask=%x, bmask=%x\n", bpp, rmask, gmask, bmask);
		fprintf(stderr, "line=%d, size=%d\n", line, size);

	int bHasbg=FreeImage_HasBackgroundColor(dib);
	int bTransparent=FreeImage_IsTransparent(dib);
	int colortype=FreeImage_GetColorType(dib);
		fprintf(stderr, "HasBackground=%d, transparent=%d, color=%d\n", bHasbg, bTransparent, colortype);

	phandle->width=width;
	phandle->height=height;
	phandle->bpp=bpp;
	phandle->type=-1;
	phandle->r_mask=rmask;
	phandle->g_mask=gmask;
	phandle->b_mask=bmask;
	phandle->stride=line;
	phandle->pages_max=0;
	phandle->pages_cur=0;
}



int image_load(const char* file, image_handle* phandle)
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	FIBITMAP * dib=NULL;
	FIMULTIBITMAP * dim=NULL;
	int page_count=0;


	fif=FreeImage_GetFileType(file, 0);

	//check type
	if(fif == FIF_UNKNOWN) fif=FreeImage_GetFIFFromFilename(file);
	if(fif == FIF_UNKNOWN) 
	{
		fprintf(stderr, "Load image %s failed, unknown type\n", file);
		return IMAGE_ERR_UNKNOWN;
	}

	fprintf(stderr, "image %s , type %d\n", file, (int)fif);

	//check if it's Multi-page
	if(fif==FIF_GIF)
	{
		//
		dim=FreeImage_OpenMultiBitmap(fif, file, FALSE, TRUE, TRUE, GIF_PLAYBACK);
		if(dim==NULL)
		{
			fprintf(stderr, "Load GIF image %s failed, type=%d\n", file, (int)fif);
			return IMAGE_ERR_LOAD;
		}
		page_count=FreeImage_GetPageCount(dim);

		fprintf(stderr, "Load GIF image %s OK, type=%d, page=%d\n", file, (int)fif, page_count);

		dib=FreeImage_LockPage(dim, 0);
		if(dib==NULL)
		{
			//load failed
			fprintf(stderr, "Log GIF %s failed, type=%d\n", file, (int)fif);
			return IMAGE_ERR_PAGE;
		}
	}
	else
	{
		//load image
		dib=FreeImage_Load(fif, file, 0);
		if(dib==NULL)
		{
			//load failed
			fprintf(stderr, "Load image %s failed, type=%d\n", file, (int)fif);
			return IMAGE_ERR_PAGE;
		}
	}

	phandle->type=(int)fif;

	//get info
	phandle->handle=dib;
	phandle->handle_mp=dim;
	image_getinfo(phandle);

	phandle->pages_cur=0;
	phandle->pages_max=page_count;

	return IMAGE_ERR_SUCCESS;
}

void image_unload(image_handle* handle)
{
	FIBITMAP * dib=(FIBITMAP*)handle->handle;
	FIMULTIBITMAP * dim=(FIMULTIBITMAP*)handle->handle_mp;

	if(handle->pages_max!=0)
	{
		//multi-pages
		if(dib) FreeImage_UnlockPage(dim, dib, FALSE);
		if(dim) FreeImage_CloseMultiBitmap(dim, 0);
	}
	else
	{
		//unload single page
		if(dib) FreeImage_Unload(dib);
	}

	memset(handle, 0, sizeof(image_handle));
}



//rescal image and put image to handle_dst
void image_rescale(image_handle* handle_src, image_handle* handle_dst, int new_width, int new_height)
{
	FIBITMAP * dib_src=(FIBITMAP*)handle_src->handle;
	FIBITMAP * dib_dst;

	dib_dst=FreeImage_Rescale(dib_src, new_width, new_height, FILTER_CATMULLROM);
	handle_dst->handle=dib_dst;
	handle_dst->handle_mp=NULL;
	image_getinfo(handle_dst);
}


//copy part of image to memory
void image_copy(image_handle* handle, void* dstMemory, int src_x, int src_y, 
		int dst_x, int dst_y, int dst_stride, int width, int height)
{
	FIBITMAP * dib=(FIBITMAP*)handle->handle;
	int i, ws, hs;

	ws=width;
	hs=height;

	if(src_x+width > handle->width)
	{
		//width exceed
		ws=handle->width-src_x;
	}
	if(src_y+height > handle->height)
	{
		//height exceed
		hs=handle->height-src_y;
	}

	dstMemory+=(dst_y*dst_stride);
	for(i=0; i<hs; i++)
	{
		uint32_t* pData=(uint32_t*)FreeImage_GetScanLine(dib, src_y+i);
		memcpy(dstMemory+dst_x*4, (const void*)(pData+src_x), ws*4);
		dstMemory+=dst_stride;
//		if(i==0) 
//		{
//			fprintf(stderr, "Dump: %04X %04X %04X %04X\n", pData[0], pData[1], pData[2], pData[3]);
//		}
	}
}

//copy part of image to memory in v-flip mode
void image_copy_vflip(image_handle* handle, void* dstMemory, int src_x, int src_y, 
		int dst_x, int dst_y, int dst_stride, int width, int height)
{
	FIBITMAP * dib=(FIBITMAP*)handle->handle;
	int i, ws, hs;

	ws=width;
	hs=height;

	if(src_x+width > handle->width)
	{
		//width exceed
		ws=handle->width-src_x;
	}
	if(src_y+height > handle->height)
	{
		//height exceed
		hs=handle->height-src_y;
	}

	dstMemory+=((dst_y+hs-1)*dst_stride);
	for(i=0; i<hs; i++)
	{
		uint32_t* pData=(uint32_t*)FreeImage_GetScanLine(dib, src_y+i);
		memcpy(dstMemory+dst_x*4, (const void*)(pData+src_x), ws*4);
		dstMemory-=dst_stride;
//		if(i==0) 
//		{
//			fprintf(stderr, "Dump: %04X %04X %04X %04X\n", pData[0], pData[1], pData[2], pData[3]);
//		}
	}
}



void* image_getbits(image_handle* handle)
{
	FIBITMAP * dib=(FIBITMAP*)handle->handle;
	return FreeImage_GetBits(dib);
}

//copy part of image to memory (must 32bit bpp)
void image_frombits(void* memory_bits, image_handle* pRet, int width, int height, int stride)
{
	FIBITMAP * dib;

	dib=FreeImage_ConvertFromRawBits(memory_bits, width, height, stride, 32, 0xff0000, 0xff00, 0xff, FALSE);
	pRet->handle=dib;
	pRet->handle_mp=NULL;

	image_getinfo(pRet);
}

//return a normal image
void image_mp_lock_page(image_handle* handle, int page)
{
	FIMULTIBITMAP * dim=(FIMULTIBITMAP*)handle->handle_mp;
	FIBITMAP * dib=(FIBITMAP*)handle->handle;
	if(dim)
	{
		if(page >= handle->pages_max) page=0;
		if(dib) FreeImage_UnlockPage(dim, dib, FALSE);
	 	handle->handle=FreeImage_LockPage(dim, page);
		handle->pages_cur=page;
	}
}




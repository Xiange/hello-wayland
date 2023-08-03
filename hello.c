#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/input-event-codes.h>
#include "xdg-shell-client-protocol.h"
#include "shm.h"
#include "image.h"
#include "cairo/cairo.h"
#include "config.h"

void* LoadImageFile(const char* file, int* pWidth, int* pHeight);

static void draw_cairo(int x, int y);
static struct wl_buffer *draw_frame(int width, int height, int time_in_ms); 

//Seat callbacks
static void seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities);

static void seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
    fprintf(stderr, "Seat_Name: %s\n", name);
}



static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name=seat_name,
};



struct hello_context
{
	int 					running;
	int 					width, height;
	int 					width_shm, height_shm;
	float 					offset;
	int 					shm_fd;
	int 					mouse_set;
	int 					mouse_erase;
	unsigned int 			last_frame;
	uint32_t *				pData32;
	uint32_t *				pDataMouse;
	struct wl_shm* 			shm;
	struct xdg_wm_base* 	wm_base;
	struct wl_compositor*	compositor;
	struct wl_subcompositor* subcompositor;
	struct wl_surface*		surface;
	struct wl_surface*		surface_mouse;
	struct xdg_surface *	xdg_surface;
	struct xdg_toplevel*	xdg_toplevel; 
	struct wl_subsurface*	xdg_sub; 
	struct wl_seat* 		seat;
	struct wl_shm_pool *	pool;
	struct wl_buffer *		buffer;
	struct wl_buffer *		bufferMouse;
	cairo_surface_t*		cairo_surface;
	cairo_t*				cr;
	int 					transparent;
	image_handle 			img_handle;
	image_handle 			gif_handle;
	int 					gif_frame_time;
};


struct hello_context g_context={0};

//static void ck_pix_format(void *data, struct wl_shm *wl_shm, uint32_t format)
//{
//    fprintf(stderr, "shm_format: %08X\n", format);
//}
//
//
//struct wl_shm_listener cb_shm_listner={ck_pix_format};

static const struct wl_callback_listener wl_surface_frame_listener;



//surface conf. callback
static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) 
{
	int width, height;
	xdg_surface_ack_configure(xdg_surface, serial);

	struct wl_buffer *buffer = NULL;

	//create buffer
	width=g_context.width;
	height=g_context.height;
	if(width==0) width=640;
	if(height==0) height=480;
    fprintf(stderr, "surface: hanle configure, width=%d, height=%d\n", width, height);

	buffer = draw_frame(width, height, 0);
	if (buffer == NULL) {
    	fprintf(stderr, "Create buffer failed\n");
		return;
	}
	//attach subbufer
	wl_surface_attach(g_context.surface_mouse, g_context.bufferMouse, 0, 0);
	wl_surface_commit(g_context.surface_mouse);

	//attach buffer
	wl_surface_attach(g_context.surface, buffer, 0, 0);
	wl_surface_commit(g_context.surface);
};

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

	
//toplevel callback
static void toplevel_config(void *data,
			  struct xdg_toplevel *xdg_toplevel,
			  int32_t width,
			  int32_t height,
			  struct wl_array *states)
{
    fprintf(stderr, "xdg_toplevel_config: width=%d, height=%d\n", width, height);
	g_context.width=width;
	g_context.height=height;
}

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) 
{
	
    fprintf(stderr, "xdg_toplevel: hanle close.\n");
	g_context.running=0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = toplevel_config,
	.close = xdg_toplevel_handle_close,
};




//registry callbacks
static void on_global_added(void *data,
                            struct wl_registry *wl_registry,
                            uint32_t name,
                            const char *interface,
                            uint32_t version)
{
    fprintf(stderr, "Global added: %-3d, %s (ver %d)\n", name,  interface, version );
	if(strcmp(interface, wl_shm_interface.name)==0) 
	{
		g_context.shm=wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
		//wl_shm_add_listener(g_context.pshm, &cb_shm_listner, NULL);
	}
	else if(strcmp(interface, xdg_wm_base_interface.name)==0) 
	{
		g_context.wm_base=wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, version);
	}
	else if(strcmp(interface, wl_compositor_interface.name)==0) 
	{
		g_context.compositor=wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
	}
	else if(strcmp(interface, wl_seat_interface.name)==0) 
	{
		g_context.seat=wl_registry_bind(wl_registry, name, &wl_seat_interface, version);
		wl_seat_add_listener(g_context.seat, &seat_listener, NULL);
	}
	else if(strcmp(interface, wl_subcompositor_interface.name)==0) 
	{
		g_context.subcompositor=wl_registry_bind(wl_registry, name, &wl_subcompositor_interface, version);
	}
}

static void on_global_removed(void *data,
                            struct wl_registry *wl_registry,
                            uint32_t name)
{
    fprintf(stderr, "Global Removed: (name %d)\n", name);
}

struct wl_registry_listener s_registryListener=
{
	on_global_added,
	on_global_removed
};



int main()
{
	int iRet=0;

	//load png image
	image_init();
	
	iRet=image_load(hello_datadir "/test.png", &g_context.img_handle);
	if(iRet)
	{
    	fprintf(stderr, "PNG loaded failed, code=%d\n", iRet);
	}

	iRet=image_load(hello_datadir "./t2.gif", &g_context.gif_handle);
	if(iRet)
	{
    	fprintf(stderr, "GIF loaded failed, code=%d\n", iRet);
	}
	

	//get wayland display
    struct wl_display *display = wl_display_connect(0);

    if (!display)
    {
        fprintf(stderr, "Unable to connect to wayland compositor\n");
        return -1;
    }

	//get registry
	struct wl_registry *reg = wl_display_get_registry(display);
	if (!reg)
	{
		fprintf(stderr, "Failed to get registry!\n");
	}
	fprintf(stderr, "Got registry OK!\n");

	//poll
	wl_registry_add_listener(reg, &s_registryListener, NULL);
	wl_display_roundtrip(display);

	//check shm,compositor,and wm_base
	if(g_context.shm==NULL || g_context.compositor==NULL || g_context.wm_base==NULL ||
			g_context.subcompositor==NULL)
	{
		fprintf(stderr, "Interface bind failed.\n");
		goto ENDMAIN;
	}

	//create one surface from compositor
	g_context.running=1;
	g_context.shm_fd=-1;
	g_context.transparent=255;
	g_context.surface = wl_compositor_create_surface(g_context.compositor);
	g_context.surface_mouse = wl_compositor_create_surface(g_context.compositor);

	if(g_context.surface==NULL || g_context.surface_mouse==NULL)
	{
		fprintf(stderr, "create surface failed\n");
		iRet=2;
		goto ENDMAIN;
	}


	//create toplevel surface with xdg_wm_base
	g_context.xdg_surface = xdg_wm_base_get_xdg_surface(g_context.wm_base, g_context.surface);
	g_context.xdg_toplevel = xdg_surface_get_toplevel(g_context.xdg_surface);

	//create sub_surface, set position ,z-order, and sync.
	g_context.xdg_sub=wl_subcompositor_get_subsurface(g_context.subcompositor, 
			g_context.surface_mouse, g_context.surface);

	if(g_context.xdg_sub==NULL)
	{
		fprintf(stderr, "create sub surface failed, code=%ld\n", (long)g_context.xdg_sub);
		iRet=3;
		goto ENDMAIN;
	}

	//position,order,sync.
	wl_subsurface_set_position(g_context.xdg_sub, 0, 0);
	wl_subsurface_place_above(g_context.xdg_sub, g_context.surface);
	wl_subsurface_set_sync(g_context.xdg_sub);

	//add callbacks
	xdg_surface_add_listener(g_context.xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel_add_listener(g_context.xdg_toplevel, &xdg_toplevel_listener, NULL);

	//set title
	xdg_toplevel_set_title(g_context.xdg_toplevel, "Hello, world!");

	//show it?
	wl_surface_commit(g_context.surface);

	struct wl_callback *cb = wl_surface_frame(g_context.surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, &g_context);


	while (wl_display_dispatch(display) != -1 && g_context.running) 
	{
		// This space intentionally left blank
	}


ENDMAIN:

	if(g_context.cr) cairo_destroy(g_context.cr);
	if(g_context.cairo_surface) cairo_surface_destroy(g_context.cairo_surface);

	if(g_context.xdg_toplevel) xdg_toplevel_destroy(g_context.xdg_toplevel);
	if(g_context.xdg_surface) xdg_surface_destroy(g_context.xdg_surface);
	if(g_context.xdg_sub) wl_subsurface_destroy(g_context.xdg_sub);
	if(g_context.surface) wl_surface_destroy(g_context.surface);
	if(g_context.surface_mouse) wl_surface_destroy(g_context.surface_mouse);


	if(g_context.pData32) munmap(g_context.pData32, g_context.width_shm*g_context.height_shm*4*2);
	if(g_context.buffer) wl_buffer_destroy(g_context.buffer);
	if(g_context.buffer) wl_buffer_destroy(g_context.bufferMouse);
    if(g_context.pool) wl_shm_pool_destroy(g_context.pool);
    if(g_context.shm_fd!=-1) close(g_context.shm_fd);

    wl_display_disconnect(display);

	image_unload(&g_context.img_handle);
	image_unload(&g_context.gif_handle);
	image_exit();
    return iRet;
}


static int create_buffer(int width, int height) 
{
	int stride = width * 4;
	int offset = stride * height;
	int size = offset * 2; 		//2 buffer
	int bChange=1;

	if(g_context.shm_fd==-1)
	{
		//new create
		int fd = create_shm_file(size);
		if (fd < 0) {
			fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
			return 1;
		}
		else {
			fprintf(stderr, "creating shm file OK, fd=%d, size=%d\n", fd, size);
		}
		g_context.shm_fd=fd;
	}
	else if(g_context.width_shm!=width || g_context.height_shm!=height)
	{
		//resize fd
		if (ftruncate(g_context.shm_fd, size) < 0) {
			fprintf(stderr, "resize fd %d to %d failed\n", g_context.shm_fd, size);
			return 2;
		}
		fprintf(stderr, "shm file %d resized to %d\n", g_context.shm_fd, size);
	}
	else
	{
		bChange=0;
	}

	if(bChange)
	{
		//remap

		//release old cairo 
		if(g_context.cr) cairo_destroy(g_context.cr);
		if(g_context.cairo_surface) cairo_surface_destroy(g_context.cairo_surface);

		if(g_context.pData32)
		{
			munmap(g_context.pData32, g_context.width_shm*g_context.height_shm*4*2);
			g_context.pData32=NULL;
			g_context.pDataMouse=NULL;
		}

		g_context.width_shm=width;
		g_context.height_shm=height;

		uint32_t *data = mmap(NULL, size,
				PROT_READ | PROT_WRITE, MAP_SHARED, g_context.shm_fd, 0);

		if (data == MAP_FAILED) 
		{
			fprintf(stderr, "mmap fd %d failed, size=%d\n", g_context.shm_fd, size);
			close(g_context.shm_fd);
			g_context.shm_fd=-1;
			return 2;
		}
		g_context.pData32=data;
		g_context.pDataMouse=data+width*height;

		if(g_context.bufferMouse) wl_buffer_destroy(g_context.bufferMouse);
		if(g_context.buffer) wl_buffer_destroy(g_context.buffer);
		if(g_context.pool) wl_shm_pool_destroy(g_context.pool);
		g_context.pool = wl_shm_create_pool(g_context.shm, g_context.shm_fd, size);

   		g_context.buffer = wl_shm_pool_create_buffer(g_context.pool, 0, 
			width, height, stride, WL_SHM_FORMAT_ARGB8888);

   		g_context.bufferMouse = wl_shm_pool_create_buffer(g_context.pool, offset, 
			width, height, stride, WL_SHM_FORMAT_ARGB8888);

		//clear mouse buffer
		//memset(g_context.pDataMouse, 0, g_context.width_shm*g_context.height_shm*4);

		//rescale to fit screen
//		void* new_hdl=image_rescale(g_context.img_handle, width, height);
//		void* bits_new=image_getbits(new_hdl);
//		memcpy(g_context.pDataMouse, bits_new, g_context.width_shm*g_context.height_shm*4);
//		image_unload(new_hdl);

		image_copy(&g_context.img_handle, g_context.pDataMouse, 0, 0, 0, 0, stride, width, height);


		//g_context.img_handle_mouse=image_frombits(g_context.pDataMouse, width, height, stride);
		//if(handle_new) handle_bits_new=image_getbits(handle_new);
		//fprintf(stderr, "image new is at %p, rawbits is at %p\n", handle_new, handle_bits_new); 

		//scale png file to fit screen



		//release old

		//create cairo surface
		g_context.cairo_surface=cairo_image_surface_create_for_data(
				(unsigned char*)g_context.pDataMouse,
				CAIRO_FORMAT_ARGB32,
				width, height, stride);
		g_context.cr=cairo_create(g_context.cairo_surface);

		//draw
		draw_cairo(20, 50);

	}
	return 0;
}


static struct wl_buffer *draw_frame(int width, int height, int time_in_ms) 
{

	int stride = width * 4;

	//first create buffer in need
	if(create_buffer(width, height)!=0)
	{
		fprintf(stderr, "create buffer failed\n");
		return NULL;
	}

	int len=24;


	 /* Draw checkerboxed background */
	int offset=(int)g_context.offset % len;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (((x+offset) + (y+offset) / len * len) % (len*2) < len)
                g_context.pData32[y * width + x] = 0 | (g_context.transparent<<24);
            else
                g_context.pData32[y * width + x] = 0x666666 | (g_context.transparent<<24);
        }
    }

	// Draw GIF
	image_copy_vflip(&g_context.gif_handle, g_context.pData32,  0, 0, 300, 50, stride,
			g_context.gif_handle.width, g_context.gif_handle.height);

	//next page
	if(time_in_ms - g_context.gif_frame_time > 120)
	{
		image_mp_lock_page(&g_context.gif_handle, g_context.gif_handle.pages_cur+1);
		g_context.gif_frame_time=time_in_ms;
	}

    return g_context.buffer;

}

static void draw_cairo(int x, int y)
{
	cairo_select_font_face(g_context.cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(g_context.cr, 50.0);
	cairo_set_source_rgba(g_context.cr, 1.0, 0.0, 0.1, 0.9);
	cairo_move_to(g_context.cr, x, y); 
	cairo_show_text(g_context.cr, "Hello, world!");

	cairo_set_source_rgba(g_context.cr, 0.1, 0.0, 1.0, 0.9);
	cairo_move_to(g_context.cr, x, y+100); 
	cairo_show_text(g_context.cr, "你好，世界!");
}

//pointer callbacks


static void pointer_enter(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      wl_fixed_t surface_x,
		      wl_fixed_t surface_y)
{
		fprintf(stderr, "pointer_enter, %d, %d, serial=%d\n", surface_x, surface_y, serial);
}

static void pointer_leave(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface)
{
		fprintf(stderr, "pointer_leave, serial=%d\n", serial);
};

static void pointer_motion(void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t time,
		       wl_fixed_t surface_x,
		       wl_fixed_t surface_y)
{
		int x=surface_x/256;
		int y=surface_y/256;

		uint32_t* pData=g_context.pDataMouse+(x+y*g_context.width_shm);


		//fprintf(stderr, "pointer_motion, %d, %d, time=%d\n", surface_x/256, surface_y/256, time);

		int width=10;

		if((x>=0 && x<g_context.width_shm) && ( y>=0 && y<g_context.height_shm-width))
		{
			int xt,yt;
			if(g_context.mouse_set)
			{
				for(yt=0; yt<width; yt++)
				{
					for(xt=0; xt<width; xt++)
					{
						pData[xt]=(0xC0<<24 | 0xFF<<16);
					}
					pData+=g_context.width_shm;
				}
			}
			else if(g_context.mouse_erase)
			{
				for(yt=0; yt<width; yt++)
				{
					for(xt=0; xt<width; xt++)
					{
						pData[xt]=0;
					}
					pData+=g_context.width_shm;
				}
			}
		}


		wl_surface_damage_buffer(g_context.surface_mouse, x, y, width, width);

}
	
static void pointer_axis(void *data,
		     struct wl_pointer *wl_pointer,
		     uint32_t time,
		     uint32_t axis,
		     wl_fixed_t value)
{
		if(value>0 && g_context.transparent<255) g_context.transparent+=5;
		if(value<0 && g_context.transparent>0) g_context.transparent-=5;
		fprintf(stderr, "pointer_axis, axis=%d, value=%d, time=%d, transparent=%d\n", axis, value, time,
				g_context.transparent);
}


	
static void pointer_handle_button(void *data, struct wl_pointer *pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state) 
{
	//struct wl_seat *seat = data;

	fprintf(stderr, "pointer_button, btn=%d, stat=%d, serial=%d, time=%d\n", button, state,
			serial, time);
	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		//xdg_toplevel_move(g_context.xdg_toplevel, seat, serial);
		g_context.mouse_set=1;
	}

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_RELEASED) {
		//xdg_toplevel_move(g_context.xdg_toplevel, seat, serial);
		g_context.mouse_set=0;
	}

	if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		//xdg_toplevel_move(g_context.xdg_toplevel, seat, serial);
		g_context.mouse_erase=1;
	}

	if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_RELEASED) {
		//xdg_toplevel_move(g_context.xdg_toplevel, seat, serial);
		g_context.mouse_erase=0;
	}
}


static void pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
	//fprintf(stderr, "pointer_frame\n");
}

static void pointer_axis_source(void *data,
			    struct wl_pointer *wl_pointer,
			    uint32_t axis_source)
{
	fprintf(stderr, "pointer_axis_source=%d\n", axis_source);
}

static void pointer_axis_stop(void *data,
			  struct wl_pointer *wl_pointer,
			  uint32_t time,
			  uint32_t axis)
{
	fprintf(stderr, "pointer_axis_stop,axis=%d, time=%d\n", axis, time);
}

static void pointer_axis_discrete(void *data,
			      struct wl_pointer *wl_pointer,
			      uint32_t axis,
			      int32_t discrete)
{
	fprintf(stderr, "pointer_axis_discrete,axis=%d, discrete=%d\n", axis, discrete);
}

static void pointer_axis_value120(void *data,
			      struct wl_pointer *wl_pointer,
			      uint32_t axis,
			      int32_t value120)
{
	fprintf(stderr, "pointer_axis_v120,axis=%d, v120=%d\n", axis, value120);
}
	
static void pointer_axis_relative_direction(void *data,
					struct wl_pointer *wl_pointer,
					uint32_t axis,
					uint32_t direction)
{
	fprintf(stderr, "pointer_axis_dir,axis=%d, dir=%d\n", axis, direction);
}



static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_handle_button,
	.axis = pointer_axis,
	.frame=pointer_frame,
	.axis_source=pointer_axis_source,
	.axis_stop=pointer_axis_stop,
	.axis_discrete=pointer_axis_discrete,
	.axis_value120=pointer_axis_value120,
	.axis_relative_direction=pointer_axis_relative_direction,
};




static void seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) 
{
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		struct wl_pointer *pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(pointer, &pointer_listener, seat);
	}
}

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	/* Destroy this callback */
	wl_callback_destroy(cb);

	/* Request another frame */
	cb = wl_surface_frame(g_context.surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, data); 

	/* Update scroll amount at 24 pixels per second */
	if (g_context.last_frame != 0) {
		int elapsed = time - g_context.last_frame;
		g_context.offset += elapsed / 1000.0 * 24;
	}

	/* Submit a frame for this event */
	struct wl_buffer *buffer = draw_frame(g_context.width_shm, g_context.height_shm, time);

	wl_surface_attach(g_context.surface_mouse, g_context.bufferMouse, 0, 0);
	wl_surface_attach(g_context.surface, buffer, 0, 0);
	wl_surface_damage_buffer(g_context.surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(g_context.surface_mouse);
	wl_surface_commit(g_context.surface);
    //fprintf(stderr, "frame done, time=%d\n", time);

	g_context.last_frame = time;
}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};





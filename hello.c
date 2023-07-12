#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "xdg-shell-client-protocol.h"
#include "shm.h"


static struct wl_buffer *create_buffer(int width, int height); 

struct hello_context
{
	int 					running;
	int 					width, height;
	struct wl_shm* 			shm;
	struct xdg_wm_base* 	wm_base;
	struct wl_compositor*	compositor;
	struct wl_surface*		surface;
	struct xdg_surface *	xdg_surface;
	struct xdg_toplevel*	xdg_toplevel; 
};


struct hello_context g_context={0};

//static void ck_pix_format(void *data, struct wl_shm *wl_shm, uint32_t format)
//{
//    fprintf(stderr, "shm_format: %08X\n", format);
//}
//
//
//struct wl_shm_listener cb_shm_listner={ck_pix_format};


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

	buffer = create_buffer(width, height);
	if (buffer == NULL) {
    	fprintf(stderr, "Create buffer failed\n");
		return;
	}
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
	if(g_context.shm==NULL || g_context.compositor==NULL || g_context.wm_base==NULL)
	{
		fprintf(stderr, "Interface bind failed.\n");
		goto ENDMAIN;
	}

	//create one surface from compositor
	g_context.running=1;
	g_context.surface = wl_compositor_create_surface(g_context.compositor);

	//create toplevel surface with xdg_wm_base
	g_context.xdg_surface = xdg_wm_base_get_xdg_surface(g_context.wm_base, g_context.surface);
	g_context.xdg_toplevel = xdg_surface_get_toplevel(g_context.xdg_surface);


	//add callbacks
	xdg_surface_add_listener(g_context.xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel_add_listener(g_context.xdg_toplevel, &xdg_toplevel_listener, NULL);

	//set title
	xdg_toplevel_set_title(g_context.xdg_toplevel, "Hello, world!");

	//show it?
	wl_surface_commit(g_context.surface);
	//wl_display_roundtrip(display);


	while (wl_display_dispatch(display) != -1 && g_context.running) 
	{
		// This space intentionally left blank
	}


ENDMAIN:
	if(g_context.xdg_toplevel) xdg_toplevel_destroy(g_context.xdg_toplevel);
	if(g_context.xdg_surface) xdg_surface_destroy(g_context.xdg_surface);
	if(g_context.surface) wl_surface_destroy(g_context.surface);

    wl_display_disconnect(display);
    return iRet;
}

static void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    /* Sent by the compositor when it's no longer using this buffer */
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

static struct wl_buffer *create_buffer(int width, int height) 
{
	int stride = width * 4;
	int size = stride * height;

	int fd = create_shm_file(size);
	if (fd < 0) {
		fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
		return NULL;
	}

	 uint32_t *data = mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
		fprintf(stderr, "mmap fd %d failed, size=%d\n", fd, size);
        close(fd);
        return NULL;
    }


	struct wl_shm_pool *pool = wl_shm_create_pool(g_context.shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

	 /* Draw checkerboxed background */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if ((x + y / 8 * 8) % 16 < 8)
                data[y * width + x] = 0xFF666666;
            else
                data[y * width + x] = 0xFFEEEEEE;
        }
    }

	munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}



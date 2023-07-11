#include <wayland-client.h>
#include <stdio.h>
#include <unistd.h>

static void on_global_added(void *data,
                            struct wl_registry *wl_registry,
                            uint32_t name,
                            const char *interface,
                            uint32_t version)
{
    (void)data;
    (void)wl_registry;
    fprintf(stderr, "Global added: %s, v %d (name %d)\n", interface, version, name);
}

static void on_global_removed(void *data,
                            struct wl_registry *wl_registry,
                            uint32_t name)
{
    (void)data;
    (void)wl_registry;
    fprintf(stderr, "Global Removed: (name %d)\n", name);
}

struct wl_registry_listener s_registryListener;

int main()
{
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
	s_registryListener.global=on_global_added;
	s_registryListener.global_remove=on_global_removed;
	wl_registry_add_listener(reg, &s_registryListener, NULL);


	fprintf(stderr, "wait 3 seconds...\n");
	usleep(3000000);




    wl_display_disconnect(display);

    return 0;
}


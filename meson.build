project('helloworld', 'c', version: '0.10' )

src_files = [
    'hello.c',
    'shm.c',
    'image.c',
    'xdg-shell-protocol.c'
]

wayland_protos = dependency('wayland-protocols', version: '>=1.24')
wayland_client = dependency('wayland-client')
wayland_scanner = dependency('wayland-scanner')
cairo = dependency('cairo')

#get protocol dir
wl_proto_dir=wayland_protos.get_variable('pkgdatadir')
wl_proto_xdg=wl_proto_dir.join('/stable/xdg-shell/xdg-shell.xml')
scanner=wayland_scanner.get_variable('wayland_scanner')

custom_target(
    'xdg-shell-protocol.c',
    input: wl_proto_xdg
    output: 'xdg-shell-protocol.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)




hello_deps = [
    wayland_protos,
    wayland_client,
	cairo
]

executable(
    'helloworld',
    src_files,
	dependencies: hello_deps,
	install: true
)


	

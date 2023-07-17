project('helloworld', 'c', version: '0.10')

wayland_scanner = dependency('wayland-scanner')
wayland_protos = dependency('wayland-protocols', version: '>=1.24')
wayland_client = dependency('wayland-client')

#get protocol dir
wl_proto_dir=wayland_protos.get_variable('pkgdatadir')
wl_proto_xdg=wl_proto_dir + '/stable/xdg-shell/xdg-shell.xml'
scanner=wayland_scanner.get_variable('wayland_scanner')


xdg_c=custom_target('xdg-shell-protocol.c',
    input: wl_proto_xdg,
    output: 'xdg-shell-protocol.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

xdg_h=custom_target('xdg-shell-client-protocol.h',
    input: wl_proto_xdg,
    output: 'xdg-shell-client-protocol.h',
    command: [scanner, 'client-header', '@INPUT@', '@OUTPUT@']
)

src_files = [
    'hello.c',
    'shm.c',
    'image.c',
    'shm.h',
    'image.h',
    xdg_c,
    xdg_h
]




#wayland relative packages
cairo = dependency('cairo')

#check FreeImage
cc=meson.get_compiler('c')
freeimage=cc.find_library('freeimage', has_headers : 'FreeImage.h')


hello_deps = [
    wayland_protos,
    wayland_client,
	cairo,
    freeimage
]


#image install dir
prefix = get_option('prefix')
datadir = get_option('datadir')
data_install_dir= prefix + '/' + datadir + '/helloworld'
message('Data dir @: ' +  data_install_dir)

version = '"@0@"'.format(meson.project_version())

#config.h
conf_data = configuration_data()
conf_data.set('hello_version', version)
conf_data.set('hello_datadir', '"@0@"'.format(data_install_dir)) 


configure_file(output : 'config.h',
               configuration : conf_data)



executable(
    'helloworld',
    src_files,
	dependencies: hello_deps,
	install: true
)

#data files
data_files = [
    'test.png',
    't2.gif'
]

install_data(data_files,
  install_dir : data_install_dir
)

 
	

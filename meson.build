project('xglk', 'c', version : '0.4.11')

pkg = import('pkgconfig')

glk_dep = dependency('glk-0.6.1', static : true)
gi_dispatch_dep = dependency('gi_dispatch-0.6.1', static : true)
gi_blorb_dep = dependency('gi_blorb', static : true)
x11_dep = dependency('x11')
png_dep = dependency('libpng')
jpg_dep = dependency('libjpeg')
sdl_dep = dependency('sdl')
sdl_mixer_dep = dependency('SDL_mixer')

glksources = ['main.c',
    'xglk.c',
    'xglk_vars.c',
    'xglk_prefs.c',
    'xglk_loop.c',
    'xglk_init.c',
    'xglk_scrap.c',
    'xglk_msg.c',
    'xglk_key.c',
    'xglk_weggie.c',
    'xglk_pict.c',
    'xglk_res.c',
    'xg_event.c',
    'xg_window.c',
    'xg_stream.c',
    'xg_fileref.c',
    'xg_style.c',
    'xg_misc.c',
    'xg_gestalt.c',
    'xg_win_textbuf.c',
    'xg_win_textgrid.c',
    'xg_win_graphics.c',
    'xg_schan.c']

xglk_lib = static_library('xglk', sources : glksources, dependencies : [x11_dep, png_dep, jpg_dep, sdl_dep, sdl_mixer_dep, glk_dep, gi_blorb_dep, gi_dispatch_dep], install : true, install_dir : get_option('libdir') + '/xglk')
install_headers('glkstart.h', subdir : 'xglk')

pkg.generate(
    description : 'An X Window implementation of the Glk API',
    name : 'xglk',
    libraries : xglk_lib,
    subdirs : 'xglk',
    requires : ['x11 >= ' + x11_dep.version(), 'libpng >= ' + png_dep.version(), 'libjpeg >= ' + jpg_dep.version(), 'sdl >= ' + sdl_dep.version(), 'SDL_mixer >= ' + sdl_mixer_dep.version(), 'glk-0.6.1', 'gi_dispatch-0.6.1', 'gi_blorb = ' + gi_blorb_dep.version()],
    version : '0.4.11')

 

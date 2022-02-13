mandir = join_paths(get_option('prefix'), get_option('mandir'))
man1dir = join_paths(mandir, 'man1')

conf = configuration_data()
conf.set('version', meson.project_version())
conf.set('version_date', version_date)

manpage = configure_file(
     input: files('tio.1.in'),
     output: 'tio.1',
     configuration: conf)

install_man(
     manpage,
     install_dir: man1dir)

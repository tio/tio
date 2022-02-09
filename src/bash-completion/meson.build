conf = configuration_data()
conf.set('baudrates', baudrates)

completion_file = configure_file( input: files('tio.in'),
                                  output: 'tio',
                                  configuration: conf,
                                )

bashcompletiondir = get_option('bashcompletiondir')
if bashcompletiondir == ''
  bash_completion_dep = dependency('bash-completion', required: false)
  if bash_completion_dep.found()
    bashcompletiondir = join_paths(get_option('datadir'), 'bash-completion', 'completions')
  endif
endif

if (bashcompletiondir != 'no') and (bashcompletiondir != '')
  install_data(completion_file, install_dir: bashcompletiondir)
endif

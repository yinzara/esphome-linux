project('esphome-linux', 'c',
  version: '0.0.2',
  license: 'MIT',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    'buildtype=release',
  ],
)

# Dependencies - automatically uses pkg-config
thread_dep = dependency('threads')
dl_dep = meson.get_compiler('c').find_library('dl', required: false)

# Conditional dependencies for Bluetooth Proxy
if get_option('enable_bluetooth_proxy')
  dbus_dep = dependency('dbus-1', version: '>=1.6')
  glib_dep = dependency('glib-2.0', version: '>=2.40')
endif

# Include directories
inc = include_directories('src/include')

# Core source files
core_sources = files(
  'src/main.c',
  'src/esphome_api.c',
  'src/esphome_proto.c',
  'src/esphome_plugin.c',
)

# Plugin sources (optional, can be empty)
plugin_sources = []

# Check for plugins directory
if get_option('enable_plugins')
  plugins_dir = meson.current_source_dir() / 'plugins'
  if run_command('test', '-d', plugins_dir, check: false).returncode() == 0
    # Scan for plugin subdirectories
    foreach plugin_dir : run_command('find', plugins_dir, '-mindepth', '1', '-maxdepth', '1', '-type', 'd', check: false).stdout().strip().split('\n')
      if plugin_dir != ''
        plugin_name = plugin_dir.split('/')[-1]

        # Check if this plugin should be enabled
        should_enable = true
        if plugin_name == 'bluetooth_proxy' and not get_option('enable_bluetooth_proxy')
          should_enable = false
          message('Skipping plugin: ' + plugin_name + ' (disabled)')
        endif

        if should_enable
          message('Found plugin: ' + plugin_name)

          # Add plugin source files
          # Convert absolute path to relative path for subdir()
          plugin_rel_path = 'plugins' / plugin_name
          plugin_meson = plugin_dir / 'meson.build'
          if run_command('test', '-f', plugin_meson, check: false).returncode() == 0
            subdir(plugin_rel_path)
          else
            # Simple plugin - just add C files
            plugin_c_files = run_command('find', plugin_dir, '-name', '*.c', check: false).stdout().strip().split('\n')
            foreach src : plugin_c_files
              if src != ''
                plugin_sources += files(src)
              endif
            endforeach
          endif
        endif
      endif
    endforeach
  endif
endif

# Combine all sources
all_sources = core_sources + plugin_sources

# Compiler flags
add_project_arguments(
  '-D_GNU_SOURCE',
  language: 'c'
)

# Build dependencies list
deps = [thread_dep]
if get_option('enable_bluetooth_proxy')
  deps += [dbus_dep, glib_dep]
endif
if dl_dep.found()
  deps += dl_dep
endif

# Executable
executable('esphome-linux',
  all_sources,
  include_directories: inc,
  dependencies: deps,
  install: true,
)

# Summary
summary_dict = {
  'prefix': get_option('prefix'),
  'bindir': get_option('bindir'),
  'Bluetooth Proxy': get_option('enable_bluetooth_proxy'),
}
if get_option('enable_bluetooth_proxy')
  summary_dict += {
    'D-Bus': dbus_dep.version(),
    'GLib': glib_dep.version(),
  }
endif
summary(summary_dict, section: 'Configuration')

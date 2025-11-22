# Bluetooth Proxy Plugin meson.build
# Uses libblepp for BLE scanning instead of D-Bus/BlueZ

cpp_compiler = meson.get_compiler('cpp')

# Try to find libblepp via pkg-config first
libblepp_dep = dependency('libblepp', required: false)

if libblepp_dep.found()
  # Found via pkg-config, but need to also link dependencies that pkg-config might not include
  # Try to find Nimble library (required by libblepp)
  libnimble = cpp_compiler.find_library('nimble', required: false)

  # Try to find Bluetooth library (required by libblepp for BlueZ support)
  libbluetooth = cpp_compiler.find_library('bluetooth', required: false)

  # Combine all dependencies
  libblepp_deps = [libblepp_dep]
  if libnimble.found()
    libblepp_deps += libnimble
  endif
  if libbluetooth.found()
    libblepp_deps += libbluetooth
  endif

  libblepp_dep = declare_dependency(
    dependencies: libblepp_deps
  )
  message('Using libblepp via pkg-config (with additional dependencies)')
elif not libblepp_dep.found()
  # Try to find the library manually
  # In cross-compilation (Buildroot), it will be in sysroot/usr/lib
  # In native builds, check the subdirectories of this build

  # For cross-compilation, meson automatically searches in sysroot
  # Just need to specify library name without explicit dirs
  libblepp_lib = cpp_compiler.find_library('ble++', required: false)

  if libblepp_lib.found()
    # Found in sysroot or system paths
    # Also need to link Nimble and Bluetooth libraries if libblepp was built with them
    libblepp_deps = [libblepp_lib]

    # Try to find Nimble library (required by libblepp for Nimble support)
    libnimble = cpp_compiler.find_library('nimble', required: false)
    if libnimble.found()
        libblepp_deps += libnimble
    endif

    # Try to find Bluetooth library (required by libblepp for BlueZ support)
    libbluetooth = cpp_compiler.find_library('bluetooth', required: false)
    if libbluetooth.found()
        libblepp_deps += libbluetooth
    endif

    libblepp_dep = declare_dependency(
      dependencies: libblepp_deps
    )
    message('Using libblepp from system/sysroot')
  else
    # Fall back to local development build
    libblepp_base = join_paths(meson.source_root(), 'libble')
    message('libble root:' + libblepp_base)
    libblepp_lib = cpp_compiler.find_library('ble++',
      dirs: [
          join_paths(libblepp_base, 'out/usr/lib/aarch64-linux-gnu'),
          join_paths(libblepp_base, 'out/usr/lib/amd64-linux-gnu'),
          join_paths(libblepp_base, 'out/usr/lib/mips-linux-gnu')
      ],
      required: true
    )
    nimble_base = join_paths(meson.source_root(), 'nimble')
    nimble_lib = cpp_compiler.find_library('nimble',
                                             dirs: [
                                                 join_paths(nimble_base, 'out/lib')
                                             ],
                                             required: true
    )
    bluez_base = join_paths(meson.source_root(), 'bluez')
    bluez_lib = cpp_compiler.find_library('bluetooth',
                                             dirs: [
                                                 join_paths(bluez_base, 'out/lib')
                                             ],
                                             required: true
    )

    libblepp_dep = declare_dependency(
      include_directories: include_directories('../../libble/out/usr/include'),
      dependencies: [libblepp_lib, nimble_lib, bluez_lib]
    )

    message('Using libblepp from: ' + libblepp_base)
    message('Using nimble from: ' + nimble_base)
    message('Using bluez from: ' + bluez_base)
  endif
endif

# Plugin sources (mixed C and C++)
plugin_sources += files(
  'bluetooth_proxy_plugin.c',
  'ble_scanner.cpp',  # Rewritten in C++
)

# Add libblepp dependency to the main project deps
deps += libblepp_dep

message('Bluetooth Proxy plugin: using libblepp for BLE scanning')

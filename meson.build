project(
    'libogc2',
    ['c'],
    meson_version: '>=1.1',
)

external_project = import('unstable-external_project')

opt_level = get_option('optimization')
platform = get_option('platform')
libraries = get_option('libraries')

if libraries.length() == 0
    error('Must specify libraries to build.')
endif

lib_args = []
foreach lib: libraries
    lib_args += '--with-lib=' + lib
endforeach

p = external_project.add_project(
    'configure',
    configure_options: [
        '--prefix=@PREFIX@',
        '--bindir=@BINDIR@',
        '--libdir=@LIBDIR@',
        '--incdir=@INCLUDEDIR@',
        '--opt-level=' + opt_level,
        '--with-platform=' + platform,
    ] + lib_args,
    cross_configure_options: [],
)

deps = {}
foreach lib: libraries
    dep = p.dependency(':' + platform + '/lib' + lib + '.a')
    deps += {lib: dep}
endforeach

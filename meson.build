project('libjpeg-turbo', 'c', version : '2.1.0', license : 'bsd-like')

pkg = import('pkgconfig')
python = import('python').find_installation()

cc = meson.get_compiler('c')
host_system = host_machine.system()

cdata = configuration_data()
cdata.set('CMAKE_PROJECT_NAME', meson.project_name())
cdata.set('VERSION', meson.project_version())

jpeg_lib_version = 80
so_major = jpeg_lib_version / 10
so_age = 2
so_minor = 2
so_version = '@0@.@1@.@2@'.format(so_major, so_age, so_minor)
vs_defs = 'win/jpeg8.def'

version_arr = meson.project_version().split('.')
version_major = version_arr[0]
version_minor = version_arr[1]
version_revision = version_arr[2]

# Add padding to build an integer: 2.1.0 -> 2001000
if version_major.to_int() < 10
  version_major += '00'
elif version_major.to_int() < 100
  version_major += '0'
endif

if version_minor.to_int() < 10
  version_minor += '00'
elif version_minor.to_int() < 100
  version_minor += '0'
endif

cdata.set('JPEG_LIB_VERSION', jpeg_lib_version)
cdata.set('LIBJPEG_TURBO_VERSION_NUMBER', version_major + version_minor + version_revision)

cdata.set('BITS_IN_JSAMPLE', '8')

cdata.set('C_ARITH_CODING_SUPPORTED', true)
cdata.set('D_ARITH_CODING_SUPPORTED', true)
cdata.set('MEM_SRCDST_SUPPORTED', true)

check_headers = ['local.h', 'stddef.h', 'stdlib.h', 'sys/types.h']
if cc.get_id() == 'msvc'
  check_headers += 'intrin.h'
endif

foreach header : check_headers
  have_header = cc.has_header(header)
  cdata.set('HAVE_' + header.underscorify().to_upper(), have_header)
  cdata.set('NEED_' + header.underscorify().to_upper(), have_header)
endforeach

has_memset = cc.has_header_symbol('string.h', 'memset')
has_memcpy = cc.has_header_symbol('string.h', 'memcpy')
cdata.set('NEED_BSD_STRINGS', not has_memset or not has_memcpy)

size_t = cc.sizeof('size_t')
unsigned_long = cc.sizeof('unsigned long')
cdata.set('SIZE_T', size_t)
cdata.set('HAVE_UNSIGNED_CHAR', cc.sizeof('unsigned char') != -1)
cdata.set('HAVE_UNSIGNED_SHORT', cc.sizeof('unsigned short') != -1)
if size_t == unsigned_long
  code = 'int main(int argc, char **argv) { unsigned long a = argc;  return __builtin_ctzl(a); }'
  cdata.set('HAVE_BUILTIN_CTZL', cc.compiles(code))
endif

code = 'int main(void) { typedef struct undefined_structure *undef_struct_ptr;  undef_struct_ptr ptr = 0;  return ptr != 0; }'
cdata.set('INCOMPLETE_TYPES_BROKEN', not cc.compiles(code))

if meson.can_run_host_binaries()
  code = '''
      #include <stdio.h>
      #include <stdlib.h>
      int is_shifting_signed (long arg) {
        long res = arg >> 4;
        if (res == -0x7F7E80CL)
          return 1; /* right shift is signed */
        /* see if unsigned-shift hack will fix it. */
        /* we can't just test exact value since it depends on width of long... */
        res |= (~0L) << (32-4);
        if (res == -0x7F7E80CL)
          return 0; /* right shift is unsigned */
        printf(\"Right shift isn't acting as I expect it to.\\\\n\");
        printf(\"I fear the JPEG software will not work at all.\\\\n\\\\n\");
        return 0; /* try it with unsigned anyway */
      }
      int main (void) {
        exit(is_shifting_signed(-0x7F7E80B1L));
      }
  '''
  cdata.set('RIGHT_SHIFT_IS_UNSIGNED', cc.run(code).returncode() == 0)
else
  cdata.set('RIGHT_SHIFT_IS_UNSIGNED', false)
endif

p = run_command(python, '-c', 'import datetime; print(datetime.datetime.now().strftime("%Y%m%d"))')
cdata.set('BUILD', p.stdout().strip())

if cc.get_id() == 'msvc'
  cdata.set('THREAD_LOCAL', '__declspec(thread)')
else
  cdata.set('THREAD_LOCAL', '__thread')
endif

if get_option('force_inline')
  if cc.get_id() == 'msvc'
    cdata.set('INLINE', '__forceinline')
  else
    cdata.set('INLINE', 'inline __attribute__((always_inline))')
  endif
else
  cdata.set('INLINE', 'inline')
endif

if host_system == 'windows'
  add_project_arguments('-DDLLDEFINE', language: 'c')
endif

# TODO: Add simd support
cdata.set('WITH_SIMD', false)
simd = static_library('simd', 'jsimd_none.c')

jconfig_h_in = host_system == 'windows' ? 'win/jconfig.h.in' : 'jconfig.h.in'
jconfig_h = configure_file(input: jconfig_h_in,
  output: 'jconfig.h',
  format: 'cmake@',
  configuration: cdata)

configure_file(input: 'jconfigint.h.in',
  output: 'jconfigint.h',
  format: 'cmake@',
  configuration: cdata)

install_headers('jmorecfg.h', 'jerror.h', 'jpeglib.h', jconfig_h)

sources = [
  'jcapimin.c', 'jcapistd.c', 'jccoefct.c', 'jccolor.c', 'jcdctmgr.c', 'jchuff.c',
  'jcicc.c', 'jcinit.c', 'jcmainct.c', 'jcmarker.c', 'jcmaster.c', 'jcomapi.c', 'jcparam.c',
  'jcphuff.c', 'jcprepct.c', 'jcsample.c', 'jctrans.c', 'jdapimin.c', 'jdapistd.c', 'jdatadst.c',
  'jdatasrc.c', 'jdcoefct.c', 'jdcolor.c', 'jddctmgr.c', 'jdhuff.c', 'jdicc.c', 'jdinput.c',
  'jdmainct.c', 'jdmarker.c', 'jdmaster.c', 'jdmerge.c', 'jdphuff.c', 'jdpostct.c', 'jdsample.c',
  'jdtrans.c', 'jerror.c', 'jfdctflt.c', 'jfdctfst.c', 'jfdctint.c', 'jidctflt.c', 'jidctfst.c',
  'jidctint.c', 'jidctred.c', 'jquant1.c', 'jquant2.c', 'jutils.c', 'jmemmgr.c', 'jmemnobs.c',
  'jaricom.c', 'jcarith.c', 'jdarith.c',
]

jpeg = library('jpeg', sources,
  link_with: simd,
  soversion: so_version,
  vs_module_defs: vs_defs,
  install: true,
)

pkg.generate(jpeg, name: 'libjpeg')

jpeg_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: jpeg
)

meson.override_dependency('libjpeg', jpeg_dep)

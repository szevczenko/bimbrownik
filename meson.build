project('Unity example', 'c',
  license: 'MIT',
  default_options: [
    'c_std=c99',
    'warning_level=3'
  ],
  meson_version: '>= 0.49.0',
  subproject_dir: 'lib'
)

add_project_arguments('-DMG_ARCH=MG_ARCH_CUSTOM', language : 'c')
add_project_arguments('-D_MSC_VER=1900', language : 'c')

unity_proj = subproject('Unity')
unity_dep = unity_proj.get_variable('unity_dep')
unity_gen_runner = unity_proj.get_variable('gen_test_runner')

src_common = files([
  'test' / 'mocks' / 'dev_config.c',
])

inc_common = include_directories('test' / 'mocks' / 'include')

inc_mongoose = include_directories('hq_components' / 'mongoose_drv' / 'mongoose')

src_mongoose = files([
  'hq_components' / 'mongoose_drv' / 'mongoose' / 'mongoose.c',
])

src_hawkbit_parser = files([
  'hq_components' / 'mongoose_drv' / 'hawkbit_parser.c',
])

src1_test = files([
  'test' / 'hawkbit_parser_test.c',
])

inc_hawkbit_parser = include_directories('components' / 'utils')

test1 = executable('test_hawkbit_parser',
  sources: [
    src_common,
    src_hawkbit_parser,
    src_mongoose,
    src1_test,
    unity_gen_runner.process('test' / 'hawkbit_parser_test.c')
  ],
  include_directories: [ inc_common, inc_hawkbit_parser, inc_mongoose ],
  dependencies: [ unity_dep ],
)

test('test1', test1,
  should_fail: false)

# test2 = executable('test2',
#   sources: [
#     src2,
#     unity_gen_runner.process('test' / 'TestProductionCode2.c')
#   ],
#   include_directories: [ inc ],
#   dependencies: [ unity_dependency ],
# )

# test('test2', test2)

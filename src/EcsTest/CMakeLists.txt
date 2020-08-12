fips_begin_app(EcsTest windowed)
    fips_files(
        Main.cc EcsEngine.h 
    )

    oryol_shader(shaders.glsl)
    fips_deps(Core Gfx Assets Input)
fips_end_app()

fips_begin_app(test_EcsTest cmdline)
    fips_vs_warning_level(3)
    fips_files(
        Test.cc EcsEngine.h 
    )
    fips_deps(Core)
fips_end_app()

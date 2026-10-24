{
    depfiles_format = "gcc",
    depfiles = "ShlexTest.o: test/ShlexTest.cpp Shlex.hpp\
",
    files = {
        "test/ShlexTest.cpp"
    },
    values = {
        "/usr/sbin/g++",
        {
            "-m64",
            "-std=c++17",
            "-I.",
            "-Wall",
            "-Wextra"
        }
    }
}
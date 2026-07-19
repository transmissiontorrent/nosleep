add_executable(woke_example example.cpp)
target_link_libraries(woke_example PRIVATE woke)
target_compile_features(woke_example PRIVATE cxx_std_17)

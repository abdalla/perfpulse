function(perfpulse_set_compiler_options target)
    target_compile_definitions(${target} PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN NOMINMAX
        _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A00000C)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc /MP
            $<$<CONFIG:Release>:/O2 /GL /Gy>)
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
    endif()
endfunction()

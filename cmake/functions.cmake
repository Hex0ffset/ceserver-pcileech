function(copy_runtime_dlls TARGET)
    get_property(already_applied TARGET "${TARGET}" PROPERTY _copy_runtime_dlls_applied)

    if (CMAKE_IMPORT_LIBRARY_SUFFIX AND NOT already_applied)
        add_custom_command(
                TARGET "${TARGET}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Copying runtime DLLs for ${TARGET} from $<TARGET_RUNTIME_DLLS:${TARGET}> to $<TARGET_FILE_DIR:${TARGET}>"
                COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_RUNTIME_DLLS:${TARGET}>" "$<TARGET_FILE_DIR:${TARGET}>"
                COMMAND_EXPAND_LISTS
        )

        set_property(TARGET "${TARGET}" PROPERTY _copy_runtime_dlls_applied 1)
    endif ()
endfunction()
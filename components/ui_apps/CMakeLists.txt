idf_component_register(SRCS "src/ui_launcher.c"
                            "src/ui_maze.c"
                            "src/ui_sports.c"
                            "src/ui_board_settings.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl lv_ui t4s3_hal
                       WHOLE_ARCHIVE)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=show_home_view" "-Wl,--wrap=ui_home_create")

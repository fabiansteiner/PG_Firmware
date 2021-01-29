# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

#FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
#file(GLOB app_sources CONFIGURE_DEPENDS "*.h" "*.c")
#idf_component_register(SRCS ${app_sources})


idf_component_register(SRCS "main.c"
                        "PLC.c"
                        "UserIO.c"
                        "variablepool.c"
                        "web_server.c"
                        "calculateVolumetricContent.c"
                        "watering.c"
                        "FAT_storage.c"
                        "countDown.c"
                        "wifi.c"
                    INCLUDE_DIRS ".")


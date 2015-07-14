add_library(mod_skeleton SHARED mod_skeleton.c)
target_link_libraries(mod_skeleton wsngw)
set_target_properties(mod_skeleton PROPERTIES PREFIX "")

add_library(mod_ni SHARED mod_ni.c)
target_link_libraries(mod_ni wsngw)
set_target_properties(mod_ni PROPERTIES PREFIX "")

add_library(mod_gwman SHARED mod_gwmanage.c)
target_link_libraries(mod_gwman wsngw)
set_target_properties(mod_gwman PROPERTIES PREFIX "")

add_library(mod_devman SHARED mod_devmanage.c)
target_link_libraries(mod_devman wsngw)
set_target_properties(mod_devman PROPERTIES PREFIX "")

add_library(mod_wsnadaptor SHARED mod_wsnadaptor.c atcmddriver.c wsnconfig.c)
target_link_libraries(mod_wsnadaptor wsngw)
set_target_properties(mod_wsnadaptor PROPERTIES PREFIX "")

add_library(mod_applocate SHARED mod_applocate.c)
target_link_libraries(mod_applocate wsngw)
set_target_properties(mod_applocate PROPERTIES PREFIX "")

add_library(mod_rpc SHARED mod_rpc.c)
target_link_libraries(mod_rpc wsngw)
set_target_properties(mod_rpc PROPERTIES PREFIX "")

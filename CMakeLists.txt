# Project body
message(">>>>>>>>>>>>>>> ThingJS standart interfaces <<<<<<<<<<<<<<<<<<<")

idf_component_register(
        SRC_DIRS "./" "./implementation" "./drivers"
        INCLUDE_DIRS  "./" "./implementation" "./drivers"
        PRIV_REQUIRES thingjs_boards thingjs_extern thingjs_core esp-tls mqtt
)

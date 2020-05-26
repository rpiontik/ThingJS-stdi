# Project body
message(">>>>>>>>>>>>>>> ThingJS standart interfaces <<<<<<<<<<<<<<<<<<<")

idf_component_register(
        SRC_DIRS "./" "./implementation"
        INCLUDE_DIRS  "./" "./implementation"
        PRIV_REQUIRES thingjs_boards thingjs_extern thingjs_core esp-tls
)

set(srcs "receiver.c" "../../shared/ieee802154.c")
set(includes "." "../../shared/")

idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${includes})
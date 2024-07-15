
zephyr_sources_ifdef(CONFIG_BT_SIROCCO sirocco.c malloc.c timing.c scan.c conn.c adv.c alert.c)

add_subdirectory(detection)
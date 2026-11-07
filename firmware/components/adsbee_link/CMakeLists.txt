idf_component_register(SRCS "adsbee_link.c" "line_splitter.c"
        INCLUDE_DIRS "include"
        PRIV_REQUIRES esp_driver_uart board)

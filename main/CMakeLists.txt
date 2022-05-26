idf_component_register(SRCS "garden_main.c" "application.cpp" "moisture_service.cpp"  "watering_service.cpp" "clock_service.cpp" "ds3231.cpp" "i2cdev.cpp"
                    INCLUDE_DIRS ".")

component_compile_options(-std=gnu++17)

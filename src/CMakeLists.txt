include_directories(include)

add_library(
  csv_scanner_ext_library OBJECT
  csv_file_storage.cpp
  csv_scanner_extension.cpp
  scan_csv.cpp)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:csv_scanner_ext_library>
    PARENT_SCOPE)

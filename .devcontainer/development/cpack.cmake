# CPack's External generator requires a CMake callback to invoke the full
# Cosmo packaging flow instead of creating an unprocessed TGZ archive.
execute_process(
    COMMAND sh "${CMAKE_CURRENT_LIST_DIR}/package.sh"
        "${CMAKE_CURRENT_LIST_DIR}/../.." "${CPACK_PACKAGE_DIRECTORY}"
    COMMAND_ERROR_IS_FATAL ANY)

set(CPACK_EXTERNAL_BUILT_PACKAGES "${CPACK_PACKAGE_DIRECTORY}/omnirefl.tar.gz")

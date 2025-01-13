# header-only library

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO fnc12/sqlite_orm
    REF "d0c72834e5bb261c95a21c206d8d9ea1ac9872ae"
    SHA512 ad4a9783d3095c380d6b067f687654f54c6535bff9811fddb817b5b748768adde9388bde373256a20e2641be77a992ffc9e2767563e02e318b04f94ed9e5e79b
    HEAD_REF master
    PATCHES 
        fix-dependency.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        test BUILD_TESTING
        example BUILD_EXAMPLES
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DSQLITE_ORM_ENABLE_CXX_17=OM
        -DSQLITE_ORM_ENABLE_CXX_20=OM
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME SqliteOrm CONFIG_PATH lib/cmake/SqliteOrm)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

# cmake/cpack_deb.cmake
# =============================================================================
# CPack DEB Configuration for Percona XtraBackup 8.0
# Replaces: storage/innobase/xtrabackup/utils/debian/
#
# LIMITATION: Cannot produce percona-xtrabackup-dbg-80 package.
#   dh_strip --dbg-package has no CPack equivalent.
#   For -dbg package, keep using dpkg-buildpackage.
# =============================================================================

IF(NOT DEFINED XB_VERSION_MAJOR)
  SET(XB_VERSION_MAJOR ${MAJOR_VERSION})
ENDIF()
IF(NOT DEFINED XB_VERSION_MINOR)
  SET(XB_VERSION_MINOR ${MINOR_VERSION})
ENDIF()
IF(NOT DEFINED XB_VERSION_PATCH)
  SET(XB_VERSION_PATCH ${PATCH_VERSION})
ENDIF()
IF(NOT DEFINED XB_VERSION_EXTRA)
  SET(XB_VERSION_EXTRA "")
ENDIF()

SET(XB_VERSION_SUFFIX "${XB_VERSION_MAJOR}${XB_VERSION_MINOR}")

IF(NOT DEFINED DEB_RELEASE)
  SET(DEB_RELEASE "1")
ENDIF()

# ---------------------------------------------------------------------------
# Component-based packaging: Runtime (main), Test (test suite)
# No -dbg component (CPack limitation)
# ---------------------------------------------------------------------------
SET(CPACK_DEB_COMPONENT_INSTALL ON)

# ---------------------------------------------------------------------------
# Common DEB settings
# ---------------------------------------------------------------------------
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER
  "Percona Development Team <opensource-dev@percona.com>")
SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE
  "http://www.percona.com/software/percona-xtrabackup")
SET(CPACK_DEBIAN_PACKAGE_SECTION "database")
SET(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")

# ---------------------------------------------------------------------------
# Main package: percona-xtrabackup-80
# ---------------------------------------------------------------------------
SET(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME
  "percona-xtrabackup-${XB_VERSION_SUFFIX}")

SET(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS
  "libdbd-mysql-perl, libcurl4-openssl-dev, rsync, zstd, lz4")
SET(CPACK_DEBIAN_RUNTIME_PACKAGE_SHLIBDEPS ON)
SET(CPACK_DEBIAN_RUNTIME_PACKAGE_GENERATE_SHLIBS ON)

SET(CPACK_DEBIAN_RUNTIME_PACKAGE_PROVIDES "xtrabackup")
SET(CPACK_DEBIAN_RUNTIME_PACKAGE_CONFLICTS
  "percona-xtrabackup-21, percona-xtrabackup-22, percona-xtrabackup, percona-xtrabackup-24, percona-xtrabackup-81, percona-xtrabackup-82, percona-xtrabackup-83, percona-xtrabackup-84, percona-xtrabackup-pro-80, percona-xtrabackup-pro-84")
SET(CPACK_DEBIAN_RUNTIME_PACKAGE_BREAKS "xtrabackup (<< 2.0.0~)")
SET(CPACK_DEBIAN_RUNTIME_PACKAGE_REPLACES "xtrabackup (<< 2.0.0~)")

SET(CPACK_DEBIAN_RUNTIME_PACKAGE_DESCRIPTION
  "Open source backup tool for InnoDB and XtraDB
 Percona XtraBackup is an open-source hot backup utility for MySQL that
 doesn't lock your database during the backup. It can back up data from
 InnoDB, XtraDB and MyISAM tables on MySQL/Percona Server/MariaDB
 servers, and has many advanced features.")

SET(CPACK_DEBIAN_RUNTIME_FILE_NAME
  "percona-xtrabackup-${XB_VERSION_SUFFIX}_${CPACK_PACKAGE_VERSION}-${DEB_RELEASE}_\${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

SET(_deb_postinst_in "${CMAKE_SOURCE_DIR}/cmake/deb_postinst.in")
IF(EXISTS "${_deb_postinst_in}")
  CONFIGURE_FILE("${_deb_postinst_in}"
    "${CMAKE_BINARY_DIR}/postinst" @ONLY)
  SET(CPACK_DEBIAN_RUNTIME_PACKAGE_CONTROL_EXTRA
    "${CMAKE_BINARY_DIR}/postinst")
ENDIF()

# ---------------------------------------------------------------------------
# Test sub-package: percona-xtrabackup-test-80
# ---------------------------------------------------------------------------
SET(CPACK_DEBIAN_TEST_PACKAGE_NAME
  "percona-xtrabackup-test-${XB_VERSION_SUFFIX}")
SET(CPACK_DEBIAN_TEST_PACKAGE_DEPENDS
  "mysql-client, percona-xtrabackup-${XB_VERSION_SUFFIX} (= ${CPACK_PACKAGE_VERSION}-${DEB_RELEASE})")
SET(CPACK_DEBIAN_TEST_PACKAGE_CONFLICTS
  "percona-xtrabackup-test-pro-80, percona-xtrabackup-test-pro-84")
SET(CPACK_DEBIAN_TEST_PACKAGE_DESCRIPTION
  "Test suite for Percona XtraBackup
 Test suite for Percona XtraBackup. Install this package if you intend
 to run XtraBackup's test suite.")
SET(CPACK_DEBIAN_TEST_FILE_NAME
  "percona-xtrabackup-test-${XB_VERSION_SUFFIX}_${CPACK_PACKAGE_VERSION}-${DEB_RELEASE}_\${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
SET(CPACK_DEBIAN_TEST_PACKAGE_SHLIBDEPS OFF)

MESSAGE(STATUS "CPack DEB: percona-xtrabackup-${XB_VERSION_SUFFIX} ${CPACK_PACKAGE_VERSION}-${DEB_RELEASE}")
MESSAGE(STATUS "CPack DEB: percona-xtrabackup-test-${XB_VERSION_SUFFIX}")
MESSAGE(STATUS "CPack DEB: NOTE - dbg package NOT produced (use dpkg-buildpackage if needed)")

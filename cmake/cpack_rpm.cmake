# cmake/cpack_rpm.cmake

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

IF(NOT DEFINED RPM_RELEASE)
  SET(RPM_RELEASE "1")
ENDIF()
IF(NOT DEFINED XB_RPM_VERSION_EXTRA)
  IF("${XB_VERSION_EXTRA}" STREQUAL "" OR "${XB_VERSION_EXTRA}" STREQUAL "%{nil}")
    SET(XB_RPM_VERSION_EXTRA "${RPM_RELEASE}")
  ELSE()
    STRING(REGEX REPLACE "^-" "" _extra_clean "${XB_VERSION_EXTRA}")
    SET(XB_RPM_VERSION_EXTRA "${_extra_clean}.${RPM_RELEASE}")
  ENDIF()
ENDIF()

# ---------------------------------------------------------------------------
# Component-based packaging: Runtime (main), Test (test suite)
# ---------------------------------------------------------------------------
SET(CPACK_RPM_COMPONENT_INSTALL ON)

# ---------------------------------------------------------------------------
# Common RPM settings
# ---------------------------------------------------------------------------
SET(CPACK_RPM_PACKAGE_LICENSE "GPLv2")
SET(CPACK_RPM_PACKAGE_URL "http://www.percona.com/software/percona-xtrabackup")
SET(CPACK_RPM_PACKAGE_VENDOR "Percona LLC")
SET(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
SET(CPACK_RPM_PACKAGE_RELEASE "${XB_RPM_VERSION_EXTRA}%{?dist}")

SET(CPACK_RPM_SPEC_MORE_DEFINE
  "%define __strip /bin/true
%define debug_package %{nil}
%define __brp_mangle_shebangs /usr/bin/true")

SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
  /usr
  /usr/bin
  /usr/include
  /usr/lib
  /usr/lib/private
  /usr/lib64
  /usr/share
  /usr/share/doc
  /usr/share/man
  /usr/share/man/man1
)

SET(_rpm_changelog_path "${CMAKE_SOURCE_DIR}/cmake/rpm_changelog")
IF(EXISTS "${_rpm_changelog_path}")
  SET(CPACK_RPM_CHANGELOG_FILE "${_rpm_changelog_path}")
ELSE()
  FILE(WRITE "${CMAKE_BINARY_DIR}/rpm_changelog"
    "* Fri Aug 31 2018 Evgeniy Patlan <evgeniy.patlan@percona.com>\n- Packaging for 8.0\n")
  SET(CPACK_RPM_CHANGELOG_FILE "${CMAKE_BINARY_DIR}/rpm_changelog")
ENDIF()

# ---------------------------------------------------------------------------
# Main package: percona-xtrabackup-80
# ---------------------------------------------------------------------------
SET(CPACK_RPM_RUNTIME_PACKAGE_NAME
  "percona-xtrabackup-${XB_VERSION_SUFFIX}")
SET(CPACK_RPM_RUNTIME_PACKAGE_SUMMARY
  "XtraBackup online backup for MySQL / InnoDB")
SET(CPACK_RPM_RUNTIME_PACKAGE_DESCRIPTION
  "Percona XtraBackup is an OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines")
SET(CPACK_RPM_RUNTIME_FILE_NAME
  "percona-xtrabackup-${XB_VERSION_SUFFIX}-${CPACK_PACKAGE_VERSION}-${XB_RPM_VERSION_EXTRA}%{?dist}.${CMAKE_SYSTEM_PROCESSOR}.rpm")
SET(CPACK_RPM_RUNTIME_PACKAGE_REQUIRES
  "perl(DBD::mysql), rsync, zstd, perl(Digest::MD5), lz4")
SET(CPACK_RPM_RUNTIME_PACKAGE_CONFLICTS
  "percona-xtrabackup-21, percona-xtrabackup-22, percona-xtrabackup, percona-xtrabackup-24")
SET(CPACK_RPM_RUNTIME_PACKAGE_AUTOREQ ON)
SET(CPACK_RPM_RUNTIME_PACKAGE_AUTOPROV ON)

SET(_rpm_post_install_in "${CMAKE_SOURCE_DIR}/cmake/rpm_post_install.sh.in")
IF(EXISTS "${_rpm_post_install_in}")
  CONFIGURE_FILE("${_rpm_post_install_in}"
    "${CMAKE_BINARY_DIR}/rpm_post_install.sh" @ONLY)
  SET(CPACK_RPM_RUNTIME_POST_INSTALL_SCRIPT_FILE
    "${CMAKE_BINARY_DIR}/rpm_post_install.sh")
ENDIF()

# ---------------------------------------------------------------------------
# Test sub-package: percona-xtrabackup-test-80
# ---------------------------------------------------------------------------
SET(CPACK_RPM_TEST_PACKAGE_NAME
  "percona-xtrabackup-test-${XB_VERSION_SUFFIX}")
SET(CPACK_RPM_TEST_PACKAGE_SUMMARY
  "Test suite for Percona XtraBackup")
SET(CPACK_RPM_TEST_PACKAGE_DESCRIPTION
  "This package contains the test suite for Percona XtraBackup ${CPACK_PACKAGE_VERSION}${XB_VERSION_EXTRA}")
SET(CPACK_RPM_TEST_FILE_NAME
  "percona-xtrabackup-test-${XB_VERSION_SUFFIX}-${CPACK_PACKAGE_VERSION}-${XB_RPM_VERSION_EXTRA}%{?dist}.${CMAKE_SYSTEM_PROCESSOR}.rpm")
SET(CPACK_RPM_TEST_PACKAGE_REQUIRES
  "percona-xtrabackup-${XB_VERSION_SUFFIX} = ${CPACK_PACKAGE_VERSION}-${XB_RPM_VERSION_EXTRA}, /usr/bin/mysql")
SET(CPACK_RPM_TEST_PACKAGE_AUTOREQ OFF)
SET(CPACK_RPM_TEST_PACKAGE_AUTOPROV OFF)

MESSAGE(STATUS "CPack RPM: percona-xtrabackup-${XB_VERSION_SUFFIX} ${CPACK_PACKAGE_VERSION}-${XB_RPM_VERSION_EXTRA}")
MESSAGE(STATUS "CPack RPM: percona-xtrabackup-test-${XB_VERSION_SUFFIX}")

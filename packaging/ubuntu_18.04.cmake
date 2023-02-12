# Package generator for Ubuntu 18.04 LTS
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR ${IMPPG_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${IMPPG_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${IMPPG_VERSION_SUBMINOR})
set(CPACK_PACKAGE_FILE_NAME "imppg-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-Ubuntu_18.04")
set(CPACK_PACKAGE_CONTACT "Filip Szczerek <ga.software@yahoo.com>")
set(CPACK_PACKAGE_VENDOR "Filip Szczerek <ga.software@yahoo.com>")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Filip Szczerek <ga.software@yahoo.com>")
set(CPACK_PACKAGE_DESCRIPTION "Image Post-Processor")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Image Post-Processor")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libglew2.0, libfreeimage3, libcfitsio5, libwxgtk3.0-gtk3-0v5, wx3.0-i18n, liblua5.3-0")
include(CPack)

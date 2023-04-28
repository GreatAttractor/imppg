#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

// Temporary workaround for FreeImage 3.19.0 and newer libtiff on Fedora 38.
// FreeImage bug report: https://bugzilla.redhat.com/show_bug.cgi?id=2189506
//
// Putting it here as well (apart from image.cpp) to avoid `common_tests` linker error. I do not yet understand why
// the one in `image.cpp` is not sufficient (it is for linking `imppg` itself, and there `-lfreeimage also occurs after
// `libimage.a` in the linker invocation).
//
#if USE_FREEIMAGE && !defined(_TIFFDataSize)
  #include <tiff.h>
extern "C" {
int
_TIFFDataSize(TIFFDataType type)
{
    switch (type)
    {
    case TIFF_BYTE:
    case TIFF_SBYTE:
    case TIFF_ASCII:
    case TIFF_UNDEFINED:
        return 1;
    case TIFF_SHORT:
    case TIFF_SSHORT:
        return 2;
    case TIFF_LONG:
    case TIFF_SLONG:
    case TIFF_FLOAT:
    case TIFF_IFD:
    case TIFF_RATIONAL:
    case TIFF_SRATIONAL:
        return 4;
    case TIFF_DOUBLE:
    case TIFF_LONG8:
    case TIFF_SLONG8:
    case TIFF_IFD8:
        return 8;
    default:
        return 0;
    }
}
}
#endif

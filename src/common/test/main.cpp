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
extern "C" {

typedef enum {
	TIFF_NOTYPE = 0,
	TIFF_BYTE = 1,
	TIFF_ASCII = 2,
	TIFF_SHORT = 3,
	TIFF_LONG = 4,
	TIFF_RATIONAL = 5,
	TIFF_SBYTE = 6,
	TIFF_UNDEFINED = 7,
	TIFF_SSHORT = 8,
	TIFF_SLONG = 9,
	TIFF_SRATIONAL = 10,
	TIFF_FLOAT = 11,
	TIFF_DOUBLE = 12,
	TIFF_IFD = 13,
	TIFF_LONG8 = 16,
	TIFF_SLONG8 = 17,
	TIFF_IFD8 = 18
} TIFFDataType;

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

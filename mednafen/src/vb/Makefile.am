AUTOMAKE_OPTIONS = subdir-objects
DEFS = -DLOCALEDIR=\"$(datadir)/locale\" @DEFS@ @MATH_OPTIMIZER_FLAGS@ -fno-strict-aliasing
DEFAULT_INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/src/hw_cpu

noinst_LIBRARIES	=	libvb.a
libvb_a_SOURCES 	= 	vb.cpp debug.cpp timer.cpp input.cpp vip.cpp vsu.cpp

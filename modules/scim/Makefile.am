## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2007 Peng Wu
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

AM_CPPFLAGS             = -I$(top_srcdir)/src/include \
			  -I$(top_srcdir)/src/storage \
			  -I$(top_srcdir)/src/lookup \
			  -DGETTEXT_PACKAGE=\"@GETTEXT_PACKAGE@\" \
                          -DSCIM_ICONDIR=\"@SCIM_ICONDIR@\" \
                          -DNOVEL_PINYIN_LOCALEDIR=\"$(datadir)/locale\" \
                          -DNOVEL_PINYIN_DATADIR=\"@NOVEL_PINYIN_DATADIR@\" \
                          -DNOVEL_PINYIN_ICON_FILE=\"@SCIM_ICONDIR@/novel-pinyin.png\" \
			  @GLIB2_CPPFLAGS@

noinst_PROGRAMS          = 

noinst_HEADERS 		 = novel_special_table.h \
			   novel_pinyin_imengine_config_keys.h \
			   novel_pinyin_private.h \
			   novel_pinyin_global.h \
			   novel_native_lookup_table.h \
			   novel_pinyin_imengine.h

if SCIM_BUILD_SETUP
SETUP_MODULE = novel-pinyin-imengine-setup.la
endif

setupmoduledir           = @SCIM_MODULEDIR@/SetupUI
setupmodule_LTLIBRARIES  = $(SETUP_MODULE)

novel_pinyin_imengine_setup_la_SOURCES = novel_pinyin_imengine_setup.cpp

novel_pinyin_imengine_setup_la_CFLAGS  = @SCIM_GTKUTILS_CFLAGS@ @SCIM_CFLAGS@

novel_pinyin_imengine_setup_la_CXXFLAGS= @SCIM_GTKUTILS_CFLAGS@ @SCIM_CFLAGS@

novel_pinyin_imengine_setup_la_LDFLAGS = -avoid-version \
					-rpath $(setupmoduledir) \
					-module \
					@LIBTOOL_EXPORT_OPTIONS@ \
					@SCIM_LIBS@ \
					@SCIM_GTKUTILS_LIBS@

moduledir               = @SCIM_MODULEDIR@/IMEngine
module_LTLIBRARIES      = novel_pinyin.la

noinst_LTLIBRARIES        = libnovel_pinyin.la

libnovel_pinyin_la_SOURCES = novel_special_table.cpp \
                                novel_pinyin_global.cpp \
                                novel_native_lookup_table.cpp 

libnovel_pinyin_la_CFLAGS   = @SCIM_CFLAGS@
libnovel_pinyin_la_CXXFLAGS = @SCIM_CFLAGS@


novel_pinyin_la_SOURCES    = novel_pinyin_imengine.cpp

novel_pinyin_la_CFLAGS        = @SCIM_CFLAGS@
novel_pinyin_la_CXXFLAGS      = @SCIM_CFLAGS@

novel_pinyin_la_LDFLAGS       = -avoid-version \
				-rpath $(moduledir) \
				-module \
				@LIBTOOL_EXPORT_OPTIONS@ \
				@SCIM_LIBS@ \
				@GLIB2_LDFLAGS@ 

novel_pinyin_la_LIBADD        = libnovel_pinyin.la \
				../../src/storage/libstorage.la \
                                ../../src/lookup/liblookup.la
				


PREFIX?=/usr/local
INCLUDEDIR?=$(PREFIX)/include/android
PKGCONFIGDIR?=$(PREFIX)/lib/pkgconfig
all:
	@echo "Use '$(MAKE) install' to install"

install:
	mkdir -p $(DESTDIR)/$(INCLUDEDIR)
	mkdir -p $(DESTDIR)/$(PKGCONFIGDIR)
	cp android-config.h android-version.h $(DESTDIR)/$(INCLUDEDIR)
	cp android-headers.pc $(DESTDIR)/$(PKGCONFIGDIR)
	sed -i -e s:prefix=/usr:prefix=$(PREFIX):g $(DESTDIR)/$(PKGCONFIGDIR)/android-headers.pc
	cp -r hardware $(DESTDIR)/$(INCLUDEDIR)
	cp -r hardware_legacy $(DESTDIR)/$(INCLUDEDIR)
	cp -r cutils $(DESTDIR)/$(INCLUDEDIR)
	cp -r system $(DESTDIR)/$(INCLUDEDIR)
	cp -r android $(DESTDIR)/$(INCLUDEDIR)
	cp -r linux $(DESTDIR)/$(INCLUDEDIR)
	cp -r sync $(DESTDIR)/$(INCLUDEDIR)
	cp -r libnfc-nxp $(DESTDIR)/$(INCLUDEDIR)
	cp -r private $(DESTDIR)/$(INCLUDEDIR)
	cp -r log $(DESTDIR)/$(INCLUDEDIR)

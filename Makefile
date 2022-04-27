PREFIX?=/usr/local
INCLUDEDIR?=$(PREFIX)/include/android
all:
	@echo "Use '$(MAKE) install' to install"

install:
	mkdir -p $(DESTDIR)/$(INCLUDEDIR)
	cp -r * $(DESTDIR)/$(INCLUDEDIR)
	rm -f $(DESTDIR)/$(INCLUDEDIR)/Makefile
	rm -f $(DESTDIR)/$(INCLUDEDIR)/android-headers.pc.in
	mkdir -p $(DESTDIR)/$(PREFIX)/lib/pkgconfig
	cp android-headers.pc.in $(DESTDIR)/$(PKGCONFIGDIR)/android-headers.pc
	sed -i -e s:prefix=/usr:prefix=$(PREFIX):g $(DESTDIR)/$(PKGCONFIGDIR)/android-headers.pc
    

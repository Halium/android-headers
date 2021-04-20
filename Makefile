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
	sed -e 's;@prefix@;$(PREFIX)/;g; s;@includedir@;$(INCLUDEDIR);g' android-headers.pc.in > $(DESTDIR)/$(PREFIX)/lib/pkgconfig/android-headers.pc

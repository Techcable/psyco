
version     = 1.5.2
checkedout  = ../../dist
precompiled = ../../tag/compiled
basename    = psyco-$(version)
install-txt = \
    echo "This is the binary distribution of Psyco version $(version)."; \
    echo "It has been compiled for Python $* on $(shell date).";         \
    echo "";                                                             \
    echo "See the Psyco Guide in doc/ for installation instructions.";   \
    echo "Quick start: copy the 'psyco' subdirectory into the";          \
    echo "'site-packages' directory of Python."
install-src-txt = \
    echo "This is the source distribution of Psyco version $(version)."; \
    echo "See the Psyco Guide in doc/ for installation instructions.";   \
    echo "Quick start: python setup.py install"

linux   = $(basename)-linux.i386
src     = $(basename)-src
win     = $(basename)-win

all-src-files= $(shell cat               $(checkedout)/MANIFEST)
test-files   = $(shell grep ^test/       $(checkedout)/MANIFEST)
psyco-files  = $(shell grep ^py-support/ $(checkedout)/MANIFEST)
doc-files    = $(shell cd $(checkedout) && make -C doc && echo doc/psycoguide/*)
dist-files   = COPYING.txt $(psyco-files) $(test-files) $(doc-files)

all: clean versioncheck docoutput \
     $(src).tar.gz            \
     $(win)-2.4.zip           \
     $(linux)-2.3.tar.gz      \
     $(linux)-2.4.tar.gz      \
     $(linux)-2.5.tar.gz


FORCE:
.DELETE_ON_ERROR:


clean: FORCE
	-rm -fr $(basename)*

libcheck: check-for-libc-2.2

check-for-libc-2.2: FORCE
	[ -e /lib/libc-2.2.* ]

versioncheck: FORCE
	cd $(checkedout)/c && python versionchecker.py

docoutput: FORCE
	make -C $(checkedout)/doc

precompress-%: FORCE
	find $*.dir -name ".cvsignore" -exec rm {} ';'

%.tar.gz: %.dir precompress-%
	cd $< && tar zcf ../$@ $(basename)
	-rm -r $<

%.zip: %.dir precompress-%
	-rm -f $@
	cd $< && zip -r -9 ../$@ $(basename)
	-rm -r $<


$(src).dir: FORCE
	mkdir -p $@/$(basename)
	($(install-src-txt)) > $@/$(basename)/INSTALL.txt
	(cd $(checkedout) && tar c $(all-src-files)) | (cd $@/$(basename) && tar x)

$(linux)-%.dir: FORCE
#	cd $(checkedout)/c && make python=$(python-$*) mode=O
	mkdir -p $@/$(basename)
	($(install-txt)) > $@/$(basename)/INSTALL.txt
	(cd $(checkedout) && tar c $(dist-files)) | (cd $@/$(basename) && tar x)
	mv $@/$(basename)/py-support $@/$(basename)/psyco
	cd $(checkedout) && python$* setup.py build_ext -f -i
	mv $(checkedout)/py-support/_psyco.so $@/$(basename)/psyco/

$(win)-%.dir: FORCE
	mkdir -p $@/$(basename)
	($(install-txt)) > $@/$(basename)/INSTALL.txt
	(cd $(checkedout) && tar c $(dist-files)) | (cd $@/$(basename) && tar x)
	mv $@/$(basename)/py-support $@/$(basename)/psyco
	cp $(precompiled)/$(win)-$*.pyd $@/$(basename)/psyco/_psyco.pyd

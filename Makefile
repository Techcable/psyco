include Makefile.in

sources = $(notdir $(wildcard content/*.rst))

doc = psycoguide
doccompiled = psyco.ps.gz structure.ps.gz theory_psyco.ps.gz theory_psyco.pdf
docpsgz = $(addsuffix .ps.gz,$(doc)) $(doccompiled)

slides = $(sort $(notdir $(wildcard content/slides/*.*)))

images = pipe-corner.jpg   \
         pipe-h.jpg        \
         pipe-title.jpg    \
         pipe-title-v.jpg  \
         pipe-title-end.jpg\
         pipe-hdr.jpg      \
         pipe-hdr-bg.jpg   \
         pipe-bot.jpg

pages = $(addsuffix .html,$(basename $(sources)))
slide-pages = $(addprefix slides/,$(addsuffix .html,$(basename $(slides))))
slide-images = $(addprefix slides/,$(slides))

source-files = $(addprefix content/,$(sources))
image-files = $(addprefix show/,$(images))
main-site-files = $(pages) $(images) $(doc)
site-files = $(main-site-files) $(slide-images) $(slide-pages)
all-files = $(site-files)  \
            psyco-slides.zip    accu2004-psyco.tgz  \
            psyco-pepm-a.ps.gz  psyco-pepm-l.ps.gz  \
            $(docpsgz)
site-output-files = $(addprefix htdocs/,$(main-site-files))
all-output-files = htdocs $(addprefix htdocs/,$(all-files))
all-htdocs-files = $(all-output-files) htdocs/psyco-site.tar.gz htdocs/psyco-site.zip

all: FORCE $(all-htdocs-files)

clean: FORCE
	-rm -fr htdocs/*

sf: $(all-htdocs-files)
	cat /home/arigo/octogone/locked/sourceforge
	rsync -r --rsh=ssh --delete -z htdocs arigo,psyco@web.sourceforge.net:/home/groups/p/ps/psyco
	touch $@

htdocs:
	mkdir htdocs


htdocs/psyco-site.tar.gz: $(site-output-files)
	-rm -f $@
	tar zcf $@ $^

htdocs/psyco-site.zip: $(site-output-files)
	-rm -f $@
	zip -r -9 $@ $^

last-updated: $(source-files) $(image-files)
	date +'%a, %-d %b %Y' > $@


htdocs/index.html: content/index.rst last-updated show/psycowriter.py
	export LASTUPDATED="`cat last-updated`"; \
	$(python) $(tools)/rst.py -o $@ -t show/psycowriter.py -k "'title':1" $<

htdocs/%.html: content/%.rst show/psycowriter.py
	$(python) $(tools)/rst.py -o $@ -t show/psycowriter.py $<

$(addprefix htdocs/,$(doccompiled)): htdocs/%:
	svn cat http://codespeak.net/svn/user/arigo/papers/psyco/compiled/$* > $@

$(addprefix htdocs/,$(doc) $(addsuffix .ps.gz,$(doc))): htdocs/%: $(checkedout)/doc/%
	-rm -fr $@
	cp -r $< $@

htdocs/slides/%: content/slides/%
	-mkdir -p htdocs/slides
	cp $< $@

htdocs/slides/%.html: show/slides.py
	$(python) show/slides.py $* $(slides) | \
	  $(python) $(tools)/rst.py -k "'stylesheet':None" -o $@ -

htdocs/%.jpg: show/%.jpg
	cp $< $@

htdocs/psyco-slides.zip: content/psyco-slides.zip
	cp $< $@

htdocs/psyco-pepm-%.ps.gz: content/psyco-pepm-%.ps.gz
	cp $< $@

htdocs/accu2004-psyco.tgz:
	svn export http://codespeak.net/svn/user/arigo/talks/accu2004-psyco
	tar zcf $@ accu2004-psyco
	rm -r accu2004-psyco

ifdef checkedout
$(checkedout)/doc/%: FORCE
	$(MAKE) -C $(checkedout)/doc $*
endif

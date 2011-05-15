import os, sys

slides = sys.argv[2:]
pages = [os.path.splitext(slide)[0] + '.html' for slide in slides]
page1 = sys.argv[1] + '.html'

i = pages.index(page1)

links = ['']
if i>0:
    links.append(' ')
    links.append('`<<< Previous page @%s`' % pages[i-1])
if i<len(slides)-1:
    links.append(' ')
    links.append('`Next page >>> @%s`' % pages[i+1])
links.append(' ')
links.append('`Psyco home page@../index.html`')
links.append(' ')
links.append('')

sep = '+'.join(['-'*len(s) for s in links])
lnk = '|'.join(links)

print sep
print lnk
print sep
print
print '.. image:: %s' % slides[i]

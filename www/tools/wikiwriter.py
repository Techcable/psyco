#! /usr/bin/env python

"""
:Authors: Armin Rigo
:Contact: arigo@ulb.ac.be
:Revision: $Revision: 1.1 $
:Date: $Date: 2002/08/07 12:43:18 $
:Copyright: This module has been placed in the public domain.
"""

__docformat__ = 'reStructuredText'


import htmlentitydefs
from docutils import readers, nodes
from docutils.writers import html4css1


class HTMLTranslatorBase(html4css1.HTMLTranslator):
    TEXT_TO_HTML = { }
    for c in range(256):
        TEXT_TO_HTML[chr(c)] = chr(c)
    TEXT_TO_HTML_NOOP = TEXT_TO_HTML.copy()
    for entity, character in htmlentitydefs.entitydefs.items():
        TEXT_TO_HTML[character] = "&" + entity + ";"
    del entity, character, c
    TEXT_TO_HTML_NBSP = TEXT_TO_HTML.copy()
    TEXT_TO_HTML_NBSP[" "] = "&nbsp;"

    def __init__(self, doctree, stylesheet=None):
        html4css1.HTMLTranslator.__init__(self, doctree)
        if stylesheet is None:
            css = []
        else:
            css = ['<LINK REL="StyleSheet" HREF="%s"'
                   ' TYPE="text/css">\n' % stylesheet]
        self.head[-1:] = css

    def encode(self, text):
        """Encode special characters in `text` & return."""
        return ''.join(map(self.TEXT_TO_HTML.get, str(text)))

    def visit_interpreted(self, node):
        self.context.append(self.TEXT_TO_HTML)
        self.TEXT_TO_HTML = self.TEXT_TO_HTML_NOOP

    def depart_interpreted(self, node):
        self.TEXT_TO_HTML = self.context.pop()


class HTMLTranslator(HTMLTranslatorBase):

    def visit_title(self, node):
        """Only 6 section levels are supported by HTML."""
        self.context.append(self.TEXT_TO_HTML)
        if isinstance(node.parent, nodes.topic):
            self.body.append(
                  self.starttag(node, 'P', '', CLASS='topic-title'))
            self.context.append('</P>\n')
        elif self.sectionlevel == 0:
            self.head.append('<TITLE>%s</TITLE>\n'
                             % self.encode(node.astext()))
            self.body.append('\n')
            self.body.append(self.starttag(node, 'H1', '', CLASS='title'))
            self.body.append('<P CLASS="bigtitle">&nbsp;&nbsp;&nbsp;&nbsp;')
            self.TEXT_TO_HTML = self.TEXT_TO_HTML_NBSP
            self.context.append('&nbsp;'*39 + '</P></H1>\n')
        else:
            self.body.append(
                  self.starttag(node, 'H%s' % self.sectionlevel, ''))
            context = ''
            if node.hasattr('refid'):
                self.body.append('<A HREF="#%s">' % node['refid'])
                context = '</A>'
            self.context.append('%s</H%s>\n' % (context, self.sectionlevel))

    def depart_title(self, node):
        self.body.append(self.context.pop())
        self.TEXT_TO_HTML = self.context.pop()


class Writer(html4css1.Writer):
    translator = HTMLTranslator
    translator_kw = {'stylesheet': 'default.css'}

    def translate(self):
        visitor = self.translator(self.document, **self.translator_kw)
        self.document.walkabout(visitor)
        self.output = visitor.astext()

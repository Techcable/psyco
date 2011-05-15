#! /usr/bin/env python

"""
:Authors: Armin Rigo
:Contact: arigo@ulb.ac.be
:Revision: $Revision: 1.1 $
:Date: $Date: 2002/08/07 12:43:18 $
:Copyright: This module has been placed in the public domain.

File Reader for the reStructuredText markup syntax using interpreted text
`` `like this` `` for relative external references.
"""

__docformat__ = 'reStructuredText'


import sys
from docutils import readers, nodes
from docutils.transforms import frontmatter, references
from docutils.parsers.rst import Parser
from docutils.transforms import TransformError, Transform


class RelativeLinks(Transform):

    def transform(self):
        self.doctree.walk(self)

    def unknown_visit(self, node):
        pass

    def visit_interpreted(self, node):
        node.__class__ = nodes.reference
        node['refuri'] = node.astext()
        node.resolved = 1
        #for i in range(len(node.parent)):
        #    if node.parent[i] is node:
        #        node.parent[i] =


class Test(Transform):

    def transform(self):
        self.uris = []
        self.doctree.walk(self)
        print self.uris

    def unknown_visit(self, node):
        pass

    def visit_reference(self, node):
        if node.has_key('refuri'):
            self.uris.append(node['refuri'])


class Reader(readers.Reader):

    document = None
    """A single document tree."""

    transforms = (Test, references.Substitutions,
                  Test, frontmatter.DocTitle,
                  Test, frontmatter.DocInfo,
                  Test, references.Footnotes,
                  Test, RelativeLinks,
                  Test, references.Hyperlinks,
                  Test)

    def scan(self):
        self.input = self.scanfile(self.source)

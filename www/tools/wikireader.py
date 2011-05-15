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


from cStringIO import StringIO
from docutils import readers
from docutils.transforms import frontmatter, references
#from docutils.parsers.rst import Parser, directives
#from docutils.transforms import TransformError, Transform
import wiki, wikitransform#, wikidirectives


class Reader(readers.Reader):

    document = None
    """A single document tree."""

    transforms = (wikitransform.Interpret,
                  references.Substitutions,
                  wikitransform.DocTitle,
                  wikitransform.DocInfo,
                  references.Footnotes,
                  #wikitransform.RelativeLinks,
                  references.Hyperlinks,)
                  #wikitransform.BuildLocalLinks,)

    def scan(self):
        self.input = self.scanfile(self.source)

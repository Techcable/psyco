#! /usr/bin/env python

"""
:Authors: Armin Rigo
:Contact: arigo@ulb.ac.be
:Revision: $Revision: 1.1 $
:Date: $Date: 2002/08/07 12:43:18 $
:Copyright: This module has been placed in the public domain.
"""

__docformat__ = 'reStructuredText'


#import re
#from docutils import nodes
#from docutils.transforms import frontmatter, references
import os
from docutils.transforms import TransformError, Transform, frontmatter
import wiki


class Interpret(Transform):

    #re_singleword = re.compile(r"\w+$")

    def transform(self):
        self.doctree.walk(self)

    def unknown_visit(self, node):
        pass

##    def visit_interpreted(self, node):
##        moduleobj = self.doctree['source'].moduleobj
##        content = node.astext().strip()
##        if self.re_singleword.match(content):
##            try:
##                obj = getattr(moduleobj, content)
##            except AttributeError:
##                node.parent.replace(node, wiki.linknode(content))
##                return
##        else:
##            locals = self.doctree['source'].kw
##            obj = eval(content, moduleobj.__dict__, locals)
##        node.parent.replace(node, wiki.literalnode(obj))

    def visit_interpreted(self, node):
        content = node.astext().strip()
        #if self.re_singleword.match(content):
        if content.startswith('$'):
            content = os.getenv(content[1:])
            node.parent.replace(node, wiki.literalnode(content))
        elif content.startswith('<!--'):
            pass
        else:
            if '@' in content:
                content, linkurl = content.split('@')
            else:
                linkurl = None
                base, ext = os.path.splitext(content)
                if not ext:
                    linkurl = content + '.html'
            node.parent.replace(node, wiki.linknode(content, linkurl))
        #else:
        #    locals = self.doctree['source'].kw
        #    obj = eval(content, moduleobj.__dict__, locals)
        #node.parent.replace(node, wiki.literalnode(obj))


class DocInfo(frontmatter.DocInfo):

    def extract_bibliographic(self, field_list):
        nodelist, remainder =  \
                  frontmatter.DocInfo.extract_bibliographic(self, field_list)
        self.doctree.non_bibliographic_fields = remainder
        return nodelist, remainder


DocTitle = frontmatter.DocTitle
##class DocTitle(frontmatter.DocTitle):

##    def promote_document_title(self):
##        result = frontmatter.DocTitle.promote_document_title(self)
##        if not result:
##            # insert a default title
##            title = D.modulename(self.doctree['source'].moduleobj)
##            if title:
##                self.doctree['name'] = title
##                titlenode = nodes.title(title, title)
##                self.doctree.insert(0, titlenode)
##        return result

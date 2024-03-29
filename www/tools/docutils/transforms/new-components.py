#! /usr/bin/env python
"""
:Authors: David Goodger, Ueli Schlaepfer
:Contact: goodger@users.sourceforge.net
:Revision: $Revision: 1.1 $
:Date: $Date: 2002/08/07 12:43:18 $
:Copyright: This module has been placed in the public domain.

Transforms related to document components.

- `Contents`: Used to build a table of contents.
"""

__docformat__ = 'reStructuredText'


import re
from docutils import nodes, utils
from docutils.transforms import TransformError, Transform


class Contents(Transform):

    """
    This transform generates a table of contents from the entire document tree
    or from a single branch.  It locates "section" elements and builds them
    into a nested bullet list, which is placed within a "topic".  A title is
    either explicitly specified, taken from the appropriate language module,
    or omitted (local table of contents).  The depth may be specified.
    Two-way references between the table of contents and section titles are
    generated (requires Writer support).

    This transform requires a startnode, which which contains generation
    options and provides the location for the generated table of contents (the
    startnode is replaced by the table of contents "topic").
    """

    def transform(self):
        topic = nodes.topic(CLASS='contents')
        title = self.startnode.details['title']
        if self.startnode.details.has_key('local'):
            startnode = self.startnode.parent
            # @@@ generate an error if the startnode (directive) not at
            # section/document top-level? Drag it up until it is?
            while not isinstance(startnode, nodes.Structural):
                startnode = startnode.parent
            if not title:
                title = []
        else:
            startnode = self.doctree
            if not title:
                title = nodes.title('', self.language.labels['contents'])
        self.sectionsbydepth = [[]]
        self.level = 0
        self.visit_section(startnode)
        for c in startnode.getchildren():
            c.walkabout(self)
        self.depart_section(startnode)
        ((startnode, contents),), = self.sectionsbydepth
        if len(contents):
            topic += title
            topic += contents
            self.startnode.parent.replace(self.startnode, topic)
        else:
            self.startnode.parent.remove(self.startnode)

    def unknown_visit(self, node):
        self.sectionsbydepth.append(self.sectionsbydepth[-1])

    def unknown_departure(self, node):
        self.sectionsbydepth.pop()

    def visit_section(self, node):
        self.level += 1
        self.sectionsbydepth.append([])

    def depart_section(self, node):
        sections = self.sectionsbydepth.pop()
        entries = []
        for section, subjects in sections:
            title = section[0]
##            children = section.getchildren()
##            for i in range(len(children)):
##                if isinstance(children[i], nodes.title):
##                    title = children[i]
##                    break
##            else:
##                literalblock = nodes.literal_block('', '')
##                title = self.doctree.reporter.severe(
##                    'Titleless section', '', literalblock)
            reference = nodes.reference('', '', refid=section['id'],
                                        *title.getchildren())
            entry = nodes.paragraph('', '', reference)
            item = nodes.list_item('', entry)
            itemid = self.doctree.set_id(item)
            title['refid'] = itemid
            if (not self.startnode.details.has_key('depth')) \
                  or self.level < self.startnode.details['depth']:
                item += subjects
            entries.append(item)
        if entries:
            entries = nodes.bullet_list('', *entries)
        self.level -= 1
        self.sectionsbydepth[-1].append((node, entries))

##    def build_contents(self, node, level=0):
##        level += 1
##        sections = []
##        i = len(node) - 1
##        while i >= 0 and isinstance(node[i], nodes.section):
##            sections.append(node[i])
##            i -= 1
##        sections.reverse()
##        entries = []
##        for section in sections:
##            title = section[0]
##            reference = nodes.reference('', '', refid=section['id'],
##                                        *title.getchildren())
##            entry = nodes.paragraph('', '', reference)
##            item = nodes.list_item('', entry)
##            itemid = self.doctree.set_id(item)
##            title['refid'] = itemid
##            if (not self.startnode.details.has_key('depth')) \
##                  or level < self.startnode.details['depth']:
##                subsects = self.build_contents(section, level)
##                item += subsects
##            entries.append(item)
##        if entries:
##            entries = nodes.bullet_list('', *entries)
##        return entries

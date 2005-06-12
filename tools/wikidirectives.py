#! /usr/bin/env python

"""
:Authors: Armin Rigo
:Contact: arigo@ulb.ac.be
:Revision: $Revision: 1.1 $
:Date: $Date: 2002/08/07 12:43:18 $
:Copyright: This module has been placed in the public domain.
"""

from __future__ import nested_scopes

__docformat__ = 'reStructuredText'



    DO NOT USE


from cStringIO import StringIO
import docutils, re, types
from docutils import readers, nodes, statemachine
from docutils.transforms import frontmatter, references
from docutils.parsers.rst import Parser, directives
from docutils.transforms import TransformError, Transform


statemac = statemachine  # name conflict

##class TitleFinder:
##    def unknown_visit(self, node):
##        pass
##    def visit_title(self, node):
##        raise node

##def finddocumenttitle(doctree, node):
##    try:
##        tf = TitleFinder()
##        tf.doctree = doctree
##        node.walk(tf)
##    except nodes.title, titlenode:
##        return titlenode.astext()
##    return "untitled"

re_singleword = re.compile(r"\w+$")


def directive_python(match, typename, data, state, statemachine, attributes):
    lineno = statemachine.abslineno()
    lineoffset = statemachine.lineoffset
    datablock, indent, offset, blankfinish = \
          statemachine.getfirstknownindented(match.end(), uptoblank=1)
    sourceinfo = statemachine.memo.document['source']
    moduleobj = sourceinfo.moduleobj

    datablock.append('')
    datablock = '\n'.join(datablock)
    obj = datablock.strip()
    if re_singleword.match(obj):
        generated = getattr(moduleobj, obj)()
        if not isinstance(generated, types.ListType):
            generated = [generated]
    else:
        generated = []
        def p_function(*objs):
            for obj in objs:
                if isinstance(obj, nodes.Node):
                    generated.append(obj)
                else:
                    obj = str(obj)
                    if generated and isinstance(generated[-1], types.StringType):
                        generated[-1] += obj
                    else:
                        generated.append(obj)
        locals = sourceinfo.kw
        locals['p'] = p_function
        exec datablock in moduleobj.__dict__, locals
        del locals['p']

    result = []
    for obj in generated:
        if isinstance(obj, nodes.Node):
            result.append(obj)
        else:
            inputlines = docutils.statemachine.string2lines(
                obj, convertwhitespace=1)
            dummynode = nodes.section()
            backup = statemachine.memo.titlestyles[:]
            try:
                statemachine.memo.titlestyles[:] =  \
                                      [None]*statemachine.memo.sectionlevel
                state.nestedparse(inputlines, 0, node=dummynode, matchtitles=1)
            finally:
                statemachine.memo.titlestyles[:] = backup
            result += dummynode.getchildren()
    return result, blankfinish

#statemachine.memo.document.set_id(sectionnode, sectionnode)
    
##    backup = {}
##    for att, emptyvalue in remove_memo_att.items():
##        backup[att] = getattr(statemachine.memo, att)
##        setattr(statemachine.memo, att, emptyvalue)
##    try:
##        state.nestedparse(inputlines, 0, node=sectionnode, matchtitles=1)
##    finally:
##        for att, previousvalue in backup.items():
##            setattr(statemachine.memo, att, previousvalue)
    #sectionnode['name'] = finddocumenttitle(statemachine.memo.document, sectionnode)
    #statemachine.memo.document.note_implicit_target(sectionnode, sectionnode)

directives._directives['python'] = directive_python

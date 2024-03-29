#! /usr/bin/env python

"""
:Author: David Goodger
:Contact: goodger@users.sourceforge.net
:Revision: $Revision: 1.1 $
:Date: $Date: 2002/08/07 12:43:18 $
:Copyright: This module has been placed in the public domain.

Simple HyperText Markup Language document tree Writer.

The output uses the HTML 4.01 Transitional DTD (*almost* strict) and
contains a minimum of formatting information. A cascading style sheet
"default.css" is required for proper viewing with a browser.
"""

__docformat__ = 'reStructuredText'


import time
from types import ListType
from docutils import writers, nodes, languages


class Writer(writers.Writer):

    output = None
    """Final translated form of `document`."""

    def translate(self):
        visitor = HTMLTranslator(self.document)
        self.document.walkabout(visitor)
        self.output = visitor.astext()

    def record(self):
        self.recordfile(self.output, self.destination)


class HTMLTranslator(nodes.NodeVisitor):

    def __init__(self, doctree):
        nodes.NodeVisitor.__init__(self, doctree)
        self.language = languages.getlanguage(doctree.languagecode)
        self.head = ['<!DOCTYPE HTML PUBLIC'
                     ' "-//W3C//DTD HTML 4.01 Transitional//EN"\n'
                     ' "http://www.w3.org/TR/html4/loose.dtd">\n',
                     '<HTML LANG="%s">\n<HEAD>\n' % doctree.languagecode,
                     '<LINK REL="StyleSheet" HREF="default.css"'
                     ' TYPE="text/css">\n']
        self.body = ['</HEAD>\n<BODY>\n']
        self.foot = ['</BODY>\n</HTML>\n']
        self.sectionlevel = 0
        self.context = []
        self.topic_class = ''

    def astext(self):
        return ''.join(self.head + self.body + self.foot)

    def encode(self, text):
        """Encode special characters in `text` & return."""
        text = text.replace("&", "&amp;")
        text = text.replace("<", "&lt;")
        text = text.replace('"', "&quot;")
        text = text.replace(">", "&gt;")
        return text

    def starttag(self, node, tagname, suffix='\n', **attributes):
        atts = {}
        for (name, value) in attributes.items():
            atts[name.lower()] = value
        for att in ('class',):          # append to node attribute
            if node.has_key(att):
                if atts.has_key(att):
                    atts[att] = node[att] + ' ' + atts[att]
        for att in ('id',):             # node attribute overrides
            if node.has_key(att):
                atts[att] = node[att]
        attlist = atts.items()
        attlist.sort()
        parts = [tagname.upper()]
        for name, value in attlist:
            if value is None:           # boolean attribute
                parts.append(name.upper())
            elif isinstance(value, ListType):
                values = [str(v) for v in value]
                parts.append('%s="%s"' % (name.upper(),
                                          self.encode(' '.join(values))))
            else:
                parts.append('%s="%s"' % (name.upper(),
                                          self.encode(str(value))))
        idname = atts.get('id') or ''
        if idname:
            idname = '<A NAME="%s"></A>' % self.encode(idname)
        return '%s<%s>%s' % (idname, ' '.join(parts), suffix)

    def visit_Text(self, node):
        self.body.append(self.encode(node.astext()))

    def depart_Text(self, node):
        pass

    def visit_admonition(self, node, name):
        self.body.append(self.starttag(node, 'div', CLASS=name))
        self.body.append('<P CLASS="admonition-title">' + self.language.labels[name] + '</P>\n')

    def depart_admonition(self):
        self.body.append('</DIV>\n')

    def visit_attention(self, node):
        self.visit_admonition(node, 'attention')

    def depart_attention(self, node):
        self.depart_admonition()

    def visit_author(self, node):
        self.visit_docinfo_item(node, 'author')

    def depart_author(self, node):
        self.depart_docinfo_item()

    def visit_authors(self, node):
        pass

    def depart_authors(self, node):
        pass

    def visit_block_quote(self, node):
        self.body.append(self.starttag(node, 'blockquote'))

    def depart_block_quote(self, node):
        self.body.append('</BLOCKQUOTE>\n')

    def visit_bullet_list(self, node):
        if self.topic_class == 'contents':
            self.body.append(self.starttag(node, 'ul', compact=None))
        else:
            self.body.append(self.starttag(node, 'ul'))

    def depart_bullet_list(self, node):
        self.body.append('</UL>\n')

    def visit_caption(self, node):
        self.body.append(self.starttag(node, 'p', '', CLASS='caption'))

    def depart_caption(self, node):
        self.body.append('</P>\n')

    def visit_caution(self, node):
        self.visit_admonition(node, 'caution')

    def depart_caution(self, node):
        self.depart_admonition()

    def visit_citation(self, node):
        self.body.append(self.starttag(node, 'table', CLASS='citation',
                                       frame="void", rules="none"))
        self.body.append('<COL CLASS="label">\n'
                         '<COL>\n'
                         '<TBODY VALIGN="top">\n'
                         '<TR><TD>\n')

    def depart_citation(self, node):
        self.body.append('</TD></TR>\n'
                         '</TBODY>\n</TABLE>\n')

    def visit_citation_reference(self, node):
        href = ''
        if node.has_key('refid'):
            href = '#' + node['refid']
        elif node.has_key('refname'):
            href = '#' + self.doctree.nameids[node['refname']]
        self.body.append(self.starttag(node, 'a', '[', href=href, #node['refid'],
                                       CLASS='citation-reference'))

    def depart_citation_reference(self, node):
        self.body.append(']</A>')

    def visit_classifier(self, node):
        self.body.append(' <SPAN CLASS="classifier-delimiter">:</SPAN> ')
        self.body.append(self.starttag(node, 'span', '', CLASS='classifier'))

    def depart_classifier(self, node):
        self.body.append('</SPAN>')

    def visit_colspec(self, node):
        atts = {}
        #if node.has_key('colwidth'):
        #    atts['width'] = str(node['colwidth']) + '*'
        self.body.append(self.starttag(node, 'col', **atts))

    def depart_colspec(self, node):
        pass

    def visit_comment(self, node):
        self.body.append('<!-- ')

    def depart_comment(self, node):
        self.body.append(' -->\n')

    def visit_contact(self, node):
        self.visit_docinfo_item(node, 'contact')

    def depart_contact(self, node):
        self.depart_docinfo_item()

    def visit_copyright(self, node):
        self.visit_docinfo_item(node, 'copyright')

    def depart_copyright(self, node):
        self.depart_docinfo_item()

    def visit_danger(self, node):
        self.visit_admonition(node, 'danger')

    def depart_danger(self, node):
        self.depart_admonition()

    def visit_date(self, node):
        self.visit_docinfo_item(node, 'date')

    def depart_date(self, node):
        self.depart_docinfo_item()

    def visit_definition(self, node):
        self.body.append('</DT>\n')
        self.body.append(self.starttag(node, 'dd'))

    def depart_definition(self, node):
        self.body.append('</DD>\n')

    def visit_definition_list(self, node):
        self.body.append(self.starttag(node, 'dl'))

    def depart_definition_list(self, node):
        self.body.append('</DL>\n')

    def visit_definition_list_item(self, node):
        pass

    def depart_definition_list_item(self, node):
        pass

    def visit_description(self, node):
        self.body.append('<TD>\n')

    def depart_description(self, node):
        self.body.append('</TD>')

    def visit_docinfo(self, node):
        self.body.append(self.starttag(node, 'table', CLASS='docinfo',
                                       frame="void", rules="none"))
        self.body.append('<COL CLASS="docinfo-name">\n'
                         '<COL CLASS="docinfo-content">\n'
                         '<TBODY VALIGN="top">\n')

    def depart_docinfo(self, node):
        self.body.append('</TBODY>\n</TABLE>\n')

    def visit_docinfo_item(self, node, name):
        self.head.append('<META NAME="%s" CONTENT="%s">\n'
                         % (name, self.encode(node.astext())))
        self.body.append(self.starttag(node, 'tr', ''))
        self.body.append('<TD>\n'
                         '<P CLASS="docinfo-name">%s:</P>\n'
                         '</TD><TD>\n'
                         '<P>' % self.language.labels[name])

    def depart_docinfo_item(self):
        self.body.append('</P>\n</TD></TR>')

    def visit_doctest_block(self, node):
        self.body.append(self.starttag(node, 'pre', CLASS='doctest-block'))

    def depart_doctest_block(self, node):
        self.body.append('</PRE>\n')

    def visit_document(self, node):
        self.body.append(self.starttag(node, 'div', CLASS='document'))

    def depart_document(self, node):
        self.body.append('</DIV>\n')
        #self.body.append(
        #      '<P CLASS="credits">HTML generated from <CODE>%s</CODE> on %s '
        #      'by <A HREF="http://docutils.sourceforge.net/">Docutils</A>.'
        #      '</P>\n' % (node['source'], time.strftime('%Y-%m-%d')))

    def visit_emphasis(self, node):
        self.body.append('<EM>')

    def depart_emphasis(self, node):
        self.body.append('</EM>')

    def visit_entry(self, node):
        if isinstance(node.parent.parent, nodes.thead):
            tagname = 'th'
        else:
            tagname = 'td'
        atts = {}
        if node.has_key('morerows'):
            atts['rowspan'] = node['morerows'] + 1
        if node.has_key('morecols'):
            atts['colspan'] = node['morecols'] + 1
        self.body.append(self.starttag(node, tagname, **atts))
        self.context.append('</%s>' % tagname.upper())
        if len(node) == 0:              # empty cell
            self.body.append('&nbsp;')

    def depart_entry(self, node):
        self.body.append(self.context.pop())

    def visit_enumerated_list(self, node):
        """
        The 'start' attribute does not conform to HTML 4.01's strict.dtd, but
        CSS1 doesn't help. CSS2 isn't widely enough supported yet to be
        usable.
        """
        atts = {}
        if node.has_key('start'):
            atts['start'] = node['start']
        if node.has_key('enumtype'):
            atts['class'] = node['enumtype']
        # @@@ To do: prefix, suffix. How? Change prefix/suffix to a
        # single "format" attribute? Use CSS2?
        self.body.append(self.starttag(node, 'ol', **atts))

    def depart_enumerated_list(self, node):
        self.body.append('</OL>\n')

    def visit_error(self, node):
        self.visit_admonition(node, 'error')

    def depart_error(self, node):
        self.depart_admonition()

    def visit_field(self, node):
        self.body.append(self.starttag(node, 'tr', CLASS='field'))

    def depart_field(self, node):
        self.body.append('</TR>\n')

    def visit_field_argument(self, node):
        self.body.append(' ')
        self.body.append(self.starttag(node, 'span', '',
                                       CLASS='field-argument'))

    def depart_field_argument(self, node):
        self.body.append('</SPAN>')

    def visit_field_body(self, node):
        self.body.append(':\n</TD><TD>')
        self.body.append(self.starttag(node, 'div', CLASS='field-body'))

    def depart_field_body(self, node):
        self.body.append('</DIV></TD>\n')

    def visit_field_list(self, node):
        self.body.append(self.starttag(node, 'table', frame='void',
                                       rules='none'))
        self.body.append('<COL CLASS="field-name">\n'
                         '<COL CLASS="field-body">\n'
                         '<TBODY VALIGN="top">\n')

    def depart_field_list(self, node):
        self.body.append('</TBODY>\n</TABLE>\n')

    def visit_field_name(self, node):
        self.body.append(self.starttag(node, 'TD', '', CLASS='field-name'))

    def depart_field_name(self, node):
        """
        Leave the end tag to `self.visit_field_body()`, in case there are any
        field_arguments.
        """
        pass

    def visit_figure(self, node):
        self.body.append(self.starttag(node, 'div', CLASS='figure'))

    def depart_figure(self, node):
        self.body.append('</DIV>\n')

    def visit_footnote(self, node):
        self.body.append(self.starttag(node, 'table', CLASS='footnote',
                                       frame="void", rules="none"))
        self.body.append('<COL CLASS="label">\n'
                         '<COL>\n'
                         '<TBODY VALIGN="top">\n'
                         '<TR><TD>\n')

    def depart_footnote(self, node):
        self.body.append('</TD></TR>\n'
                         '</TBODY>\n</TABLE>\n')

    def visit_footnote_reference(self, node):
        href = ''
        if node.has_key('refid'):
            href = '#' + node['refid']
        elif node.has_key('refname'):
            href = '#' + self.doctree.nameids[node['refname']]
        self.body.append(self.starttag(node, 'a', '', href=href, #node['refid'],
                                       CLASS='footnote-reference'))

    def depart_footnote_reference(self, node):
        self.body.append('</A>')

    def visit_hint(self, node):
        self.visit_admonition(node, 'hint')

    def depart_hint(self, node):
        self.depart_admonition()

    def visit_image(self, node):
        atts = node.attributes.copy()
        atts['src'] = atts['uri']
        del atts['uri']
        if not atts.has_key('alt'):
            atts['alt'] = atts['src']
        self.body.append(self.starttag(node, 'img', '', **atts))

    def depart_image(self, node):
        pass

    def visit_important(self, node):
        self.visit_admonition(node, 'important')

    def depart_important(self, node):
        self.depart_admonition()

    def visit_interpreted(self, node):
        self.body.append('<SPAN class="interpreted">')

    def depart_interpreted(self, node):
        self.body.append('</SPAN>')

    def visit_label(self, node):
        self.body.append(self.starttag(node, 'p', '[', CLASS='label'))

    def depart_label(self, node):
        self.body.append(']</P>\n'
                         '</TD><TD>\n')

    def visit_legend(self, node):
        self.body.append(self.starttag(node, 'div', CLASS='legend'))

    def depart_legend(self, node):
        self.body.append('</DIV>\n')

    def visit_list_item(self, node):
        self.body.append(self.starttag(node, 'li'))

    def depart_list_item(self, node):
        self.body.append('</LI>\n')

    def visit_literal(self, node):
        self.body.append('<CODE>')

    def depart_literal(self, node):
        self.body.append('</CODE>')

    def visit_literal_block(self, node):
        self.body.append(self.starttag(node, 'pre', CLASS='literal-block'))

    def depart_literal_block(self, node):
        self.body.append('</PRE>\n')

    def visit_meta(self, node):
        self.head.append(self.starttag(node, 'meta', **node.attributes))

    def depart_meta(self, node):
        pass

    def visit_note(self, node):
        self.visit_admonition(node, 'note')

    def depart_note(self, node):
        self.depart_admonition()

    def visit_option(self, node):
        if self.context[-1]:
            self.body.append(', ')

    def depart_option(self, node):
        self.context[-1] += 1

    def visit_option_argument(self, node):
        self.body.append(node.get('delimiter', ' '))
        self.body.append(self.starttag(node, 'span', '',
                                       CLASS='option-argument'))

    def depart_option_argument(self, node):
        self.body.append('</SPAN>')

    def visit_option_group(self, node):
        atts = {}
        if len(node.astext()) > 14:
            atts['colspan'] = 2
            self.context.append('</TR>\n<TR><TD>&nbsp;</TD>')
        else:
            self.context.append('')
        self.body.append(self.starttag(node, 'td', **atts))
        self.body.append('<P><CODE>')
        self.context.append(0)

    def depart_option_group(self, node):
        self.context.pop()
        self.body.append('</CODE></P>\n</TD>')
        self.body.append(self.context.pop())

    def visit_option_list(self, node):
        self.body.append(
              self.starttag(node, 'table', CLASS='option-list',
                            frame="void", rules="none", cellspacing=12))
        self.body.append('<COL CLASS="option">\n'
                         '<COL CLASS="description">\n'
                         '<TBODY VALIGN="top">\n')

    def depart_option_list(self, node):
        self.body.append('</TBODY>\n</TABLE>\n')

    def visit_option_list_item(self, node):
        self.body.append(self.starttag(node, 'tr', ''))

    def depart_option_list_item(self, node):
        self.body.append('</TR>\n')

    def visit_option_string(self, node):
        self.body.append(self.starttag(node, 'span', '', CLASS='option'))

    def depart_option_string(self, node):
        self.body.append('</SPAN>')

    def visit_organization(self, node):
        self.visit_docinfo_item(node, 'organization')

    def depart_organization(self, node):
        self.depart_docinfo_item()

    def visit_paragraph(self, node):
        if not self.topic_class == 'contents':
            self.body.append(self.starttag(node, 'p', ''))

    def depart_paragraph(self, node):
        if self.topic_class == 'contents':
            self.body.append('\n')
        else:
            self.body.append('</P>\n')

    def visit_problematic(self, node):
        if node.hasattr('refid'):
            self.body.append('<A HREF="#%s">' % node['refid'])
            self.context.append('</A>')
        else:
            self.context.append('')
        self.body.append(self.starttag(node, 'span', '', CLASS='problematic'))

    def depart_problematic(self, node):
        self.body.append('</SPAN>')
        self.body.append(self.context.pop())

    def visit_raw(self, node):
        if node.has_key('format') and node['format'] == 'html':
            self.body.append(node.astext())
        raise nodes.SkipNode

    def visit_reference(self, node):
        if node.has_key('refuri'):
            href = node['refuri']
        elif node.has_key('refid'):
        #else:
            href = '#' + node['refid']
        elif node.has_key('refname'):
            # @@@ Check for non-existent mappings. Here or in a transform?
            href = '#' + self.doctree.nameids[node['refname']]
        self.body.append(self.starttag(node, 'a', '', href=href,
                                       CLASS='reference'))

    def depart_reference(self, node):
        self.body.append('</A>')

    def visit_revision(self, node):
        self.visit_docinfo_item(node, 'revision')

    def depart_revision(self, node):
        self.depart_docinfo_item()

    def visit_row(self, node):
        self.body.append(self.starttag(node, 'tr', ''))

    def depart_row(self, node):
        self.body.append('</TR>\n')

    def visit_section(self, node):
        self.sectionlevel += 1
        self.body.append(self.starttag(node, 'div', CLASS='section'))

    def depart_section(self, node):
        self.sectionlevel -= 1
        self.body.append('</DIV>\n')

    def visit_status(self, node):
        self.visit_docinfo_item(node, 'status')

    def depart_status(self, node):
        self.depart_docinfo_item()

    def visit_strong(self, node):
        self.body.append('<STRONG>')

    def depart_strong(self, node):
        self.body.append('</STRONG>')

    def visit_substitution_definition(self, node):
        raise nodes.SkipChildren

    def depart_substitution_definition(self, node):
        pass

    def visit_substitution_reference(self, node):
        self.unimplemented_visit(node)

    def visit_subtitle(self, node):
        self.body.append(self.starttag(node, 'H2', '', CLASS='subtitle'))

    def depart_subtitle(self, node):
        self.body.append('</H2>\n')

    def visit_system_message(self, node):
        if node['level'] < self.doctree.reporter['writer'].warninglevel:
            raise nodes.SkipNode
        self.body.append(self.starttag(node, 'div', CLASS='system-message'))
        self.body.append('<P CLASS="system-message-title">')
        if node.hasattr('backrefs'):
            backrefs = node['backrefs']
            if len(backrefs) == 1:
                self.body.append('<A HREF="#%s">%s</A> '
                                 '(level %s system message)</P>\n'
                                 % (backrefs[0], node['type'], node['level']))
            else:
                i = 1
                backlinks = []
                for backref in backrefs:
                    backlinks.append('<A HREF="#%s">%s</A>' % (backref, i))
                    i += 1
                self.body.append('%s (%s; level %s system message)</P>\n'
                                 % (node['type'], '|'.join(backlinks),
                                    node['level']))
        else:
            self.body.append('%s (level %s system message)</P>\n'
                             % (node['type'], node['level']))

    def depart_system_message(self, node):
        self.body.append('</DIV>\n')

    def visit_table(self, node):
        self.body.append(
              self.starttag(node, 'table', frame='border', rules='all'))

    def depart_table(self, node):
        self.body.append('</TABLE>\n')

    def visit_target(self, node):
        if not (node.has_key('refuri') or node.has_key('refid')
                or node.has_key('refname')):
            self.body.append(self.starttag(node, 'a', '', CLASS='target'))
            self.context.append('</A>')
        else:
            self.context.append('')

    def depart_target(self, node):
        self.body.append(self.context.pop())

    def visit_tbody(self, node):
        self.body.append(self.context.pop()) # '</COLGROUP>\n' or ''
        self.body.append(self.starttag(node, 'tbody', valign='top'))

    def depart_tbody(self, node):
        self.body.append('</TBODY>\n')

    def visit_term(self, node):
        self.body.append(self.starttag(node, 'dt', ''))

    def depart_term(self, node):
        """
        Leave the end tag to `self.visit_definition()`, in case there's a
        classifier.
        """
        pass

    def visit_tgroup(self, node):
        self.body.append(self.starttag(node, 'colgroup'))
        self.context.append('</COLGROUP>\n')

    def depart_tgroup(self, node):
        pass

    def visit_thead(self, node):
        self.body.append(self.context.pop()) # '</COLGROUP>\n'
        self.context.append('')
        self.body.append(self.starttag(node, 'thead', valign='bottom'))

    def depart_thead(self, node):
        self.body.append('</THEAD>\n')

    def visit_tip(self, node):
        self.visit_admonition(node, 'tip')

    def depart_tip(self, node):
        self.depart_admonition()

    def visit_title(self, node):
        """Only 6 section levels are supported by HTML."""
        if isinstance(node.parent, nodes.topic):
            self.body.append(
                  self.starttag(node, 'P', '', CLASS='topic-title'))
            self.context.append('</P>\n')
        elif self.sectionlevel == 0:
            self.head.append('<TITLE>%s</TITLE>\n'
                             % self.encode(node.astext()))
            self.body.append(self.starttag(node, 'H1', '', CLASS='title'))
            self.context.append('</H1>\n')
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

    def visit_topic(self, node):
        self.body.append(self.starttag(node, 'div', CLASS='topic'))
        self.topic_class = node.get('class')

    def depart_topic(self, node):
        self.body.append('</DIV>\n')
        self.topic_class = ''

    def visit_transition(self, node):
        self.body.append(self.starttag(node, 'hr'))

    def depart_transition(self, node):
        pass

    def visit_version(self, node):
        self.visit_docinfo_item(node, 'version')

    def depart_version(self, node):
        self.depart_docinfo_item()

    def visit_warning(self, node):
        self.visit_admonition(node, 'warning')

    def depart_warning(self, node):
        self.depart_admonition()

    def unimplemented_visit(self, node):
        raise NotImplementedError('visiting unimplemented node type: %s'
                                  % node.__class__.__name__)

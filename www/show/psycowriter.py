from docutils import nodes
from wikiwriter import HTMLTranslatorBase


CONTENTS = [
    ('Home - News',  'Home Page',     'index.html'),
    ('Intro',        'Introduction',  'introduction.html'),
    ('Doc',          'Documentation', 'doc.html'),
    ('Download',     'Download',      'download.html'),
    ('Source (subversion)',   None,   'http://codespeak.net/svn/psyco/dist/'),
    ('SF',           None,            'http://sourceforge.net/projects/psyco'),
    ('Links',        'Links',         'links.html'),
    ]

FOOTER = '''<A href="http://sourceforge.net"> <IMG src="http://sourceforge.net/sflogo.php?group_id=41036&amp;type=5" width="210" height="62" border="0" alt="SourceForge Logo"></A>
'''

CUSTOM_HACK_HTML = '''
<div style="visibility:hidden" blame="easy_install"> <a href="http://wyvern.cs.uni-duesseldorf.de/psyco/psyco-snapshot.tar.gz">psyco snapshot</a> </div>
'''


class HTMLTranslator(HTMLTranslatorBase):
    TDOpen = (
'''<TR>
  <TD WIDTH="72" BACKGROUND="pipe-h.jpg">
    &nbsp;
  </TD>
  <TD WIDTH="100%">
''')
    TDClose = (
'''  <BR></TD>
</TR>
''')

    def __init__(self, doctree, stylesheet='ignored', title=0):
        HTMLTranslatorBase.__init__(self, doctree)
        self.title = title
        self.body = ['</HEAD>\n<BODY TEXT="#000000" BGCOLOR="#FFFFFF" LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">\n']
        self.body.append(CUSTOM_HACK_HTML)
        if not self.title:
            self.foot.insert(0, FOOTER)

    def visit_title(self, node):
        #self.context.append(self.TEXT_TO_HTML)
        if isinstance(node.parent, nodes.topic):
            self.body.append(
                  self.starttag(node, 'P', '', CLASS='topic-title'))
            self.context.append('</P>\n')
            return

        #print self.sectionlevel, node.astext()
        bodystart = len(self.body)
        postcontext = self.TDOpen
        if self.sectionlevel == 0:
            self.titletext = node.astext()
            self.head.append('<TITLE>Psyco - %s</TITLE>\n'
                             % self.encode(self.titletext))
            self.body.append('\n')
            self.foot.insert(0,
'''  </TD>
</TR>
<TR>
  <TD WIDTH="72">
    <IMG SRC="pipe-bot.jpg" WIDTH="72" HEIGHT="24">
  </TD>
</TR>
</TABLE>
</DIV>
''')
            self.body.append(
'''<TABLE BORDER="0" CELLSPACING="0" CELLPADDING="0" WIDTH="100%">
''')
            self.body.append(self.starttag(node, 'TR', ''))
            self.body.append(
'''
  <TD WIDTH="72" BACKGROUND="pipe-h.jpg">
    <IMG SRC="pipe-corner.jpg" WIDTH="72" HEIGHT="128">
  </TD>
''')
            if self.title:
                self.body.append(
'''  <TD WIDTH="100%" ALIGN="LEFT" BACKGROUND="pipe-title-v.jpg">
    <IMG SRC="pipe-title.jpg" ALT="''')
                context = (
'''" WIDTH="320" HEIGHT="128">
''')
            else:
                self.body.append(
'''  <TD WIDTH="100%" ALIGN="CENTER" VALIGN="CENTER" BACKGROUND="pipe-title-v.jpg">
    <P><FONT SIZE="+5"><I><STRONG>''')
                context = (
'''&nbsp;&nbsp;&nbsp;&nbsp;</STRONG></I></FONT></P>
''')

            context += (
'''  </TD>
  <TD WIDTH="16" ALIGN=LEFT BACKGROUND="pipe-title-v.jpg">
    <IMG SRC="pipe-title-end.jpg" WIDTH="16" HEIGHT="128">
  </TD>
</TR>
''')
            if node.parent.findclass(nodes.subtitle) is None:
                postcontext += self.build_contents()

            #self.TEXT_TO_HTML = self.TEXT_TO_HTML_NBSP
        elif self.sectionlevel == 1:
            self.body.append(self.TDClose)
            self.body.append(self.starttag(node, 'TR', ''))
            self.body.append(
'''
  <TD WIDTH="72" BACKGROUND="pipe-h.jpg" VALIGN="top">
    <IMG SRC="pipe-hdr.jpg" WIDTH="72" HEIGHT="64">
  </TD>
  <TD WIDTH="100%" BACKGROUND="pipe-hdr-bg.jpg" VALIGN="center">
    <FONT SIZE="+3"><STRONG>&nbsp;&nbsp;&nbsp;&nbsp;''')
            context = (
'''</STRONG></FONT>
  </TD>
</TR>
''')
        else:
            self.body.append(
                self.starttag(node, 'H%s' % self.sectionlevel, ''))
            context = '</H%s>' % self.sectionlevel
            postcontext = ''

        if node.hasattr('refid'):
            self.body.insert(bodystart, '<A HREF="#%s">' % node['refid'])
            context += '</A>'
        self.context.append(context + postcontext)

    def depart_title(self, node):
        self.body.append(self.context.pop())
        #self.TEXT_TO_HTML = self.context.pop()

    def visit_section(self, node):
        HTMLTranslatorBase.visit_section(self, node)
        self.body.append('</DIV>\n\n')

    def depart_section(self, node):
        HTMLTranslatorBase.depart_section(self, node)
        assert self.body[-1] == '</DIV>\n'
        del self.body[-1]

    def depart_document(self, node):
        pass

    def build_contents(self):
        result = ['<P><CENTER><TABLE BORDER><TR>']
        for key, title, url in CONTENTS:
            if title == self.titletext:
                text = '<STRONG>%s</STRONG>' % key
            else:
                text = '<A HREF="%s">%s</A>' % (url, key)
            result.append('  <TD>&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;&nbsp;&nbsp;</TD>'
                             % text)
        result.append('</TR></TABLE></CENTER></P>')
        #result.append('<BR>')
        return '\n'.join(result)

    def visit_subtitle(self, node):
        self.body.append('<P><I>' + '&nbsp;'*14)
        context = '</I></P>' + self.build_contents()
        self.context.append(context)

    def depart_subtitle(self, node):
        self.body.append(self.context.pop())

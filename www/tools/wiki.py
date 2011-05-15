from docutils import nodes
#from BTrees.OOBTree import OOBTree
import types


def literalnode(obj):
    if isinstance(obj, types.StringType):
        string = obj
    elif isinstance(obj, nodes.Node):
        return obj
    else:
        string = repr(obj)
    return nodes.literal(string, string)

def linknode(modname, linkurl=None):
    newnode = nodes.reference(modname, modname)
    newnode['refuri'] = linkurl or modname
    newnode.resolved = 1
    return newnode

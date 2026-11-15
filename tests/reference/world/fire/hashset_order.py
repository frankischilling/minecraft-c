"""JDK17 HashMap insertion/resize/tree traversal for distinct source hashes.

The declared Chunk pairs have unique packed hashes, so comparable/tie ordering,
removal and iterator mutation are not reached. No native collection is used.
"""


class Node:
    def __init__(self, key):
        self.key=key
        x,z=key
        packed=(0x80000000 if x<0 else 0)|((x&32767)<<16)|(32768 if z<0 else 0)|(z&32767)
        self.hash=packed^(packed>>16)
        self.parent=None
        self.child=[None,None]
        self.red=False
        self.tree=False


def signed(h):return ((h+(1<<31))&0xffffffff)-(1<<31)


def rotate(root,node,side):
    pivot=node.child[side]
    node.child[side]=pivot.child[1-side]
    if node.child[side] is not None:node.child[side].parent=node
    parent=node.parent;pivot.parent=parent
    if parent is None:root=pivot
    else:parent.child[int(parent.child[1] is node)]=pivot
    pivot.child[1-side]=node;node.parent=pivot
    return root


def balance(root,node):
    node.red=True
    while True:
        parent=node.parent
        if parent is None:node.red=False;return node
        grand=parent.parent
        if not parent.red or grand is None:return root
        side=int(grand.child[1] is parent)
        uncle=grand.child[1-side]
        if uncle is not None and uncle.red:
            parent.red=uncle.red=False;grand.red=True;node=grand
        else:
            if node is parent.child[1-side]:
                root=rotate(root,parent,1-side);node=parent;parent=node.parent;grand=parent.parent
            parent.red=False;grand.red=True
            return rotate(root,grand,side)


def front(bucket,root):
    if bucket[0] is not root:bucket.remove(root);bucket.insert(0,root)


def treeify(bucket):
    root=None
    for node in list(bucket):
        node.parent=None;node.child=[None,None];node.red=False;node.tree=True
        if root is None:root=node;continue
        parent=root
        while True:
            side=int(signed(node.hash)>signed(parent.hash))
            next_node=parent.child[side]
            if next_node is None:
                parent.child[side]=node;node.parent=parent
                root=balance(root,node)
                break
            parent=next_node
    front(bucket,root)


class Order:
    def __init__(self):self.buckets=[];self.size=0
    def resize(self):
        old=self.buckets;capacity=len(old)
        if capacity==0:self.buckets=[[] for _ in range(16)];return
        self.buckets=[[] for _ in range(capacity*2)]
        for index,bucket in enumerate(old):
            if not bucket:continue
            lo=[n for n in bucket if not n.hash&capacity]
            hi=[n for n in bucket if n.hash&capacity]
            self.buckets[index],self.buckets[index+capacity]=lo,hi
            if not bucket[0].tree:continue
            for group,other in ((lo,hi),(hi,lo)):
                if not group:continue
                if len(group)<=6:
                    for n in group:n.tree=False;n.parent=None;n.child=[None,None]
                elif other:treeify(group)
    def add(self,key):
        node=Node(key)
        if not self.buckets:self.resize()
        bucket=self.buckets[node.hash&(len(self.buckets)-1)]
        if any(n.key==key for n in bucket):return
        if bucket and bucket[0].tree:
            root=bucket[0]
            while root.parent is not None:root=root.parent
            parent=root
            while True:
                side=int(signed(node.hash)>signed(parent.hash))
                child=parent.child[side]
                if child is None:break
                parent=child
            node.tree=True;node.parent=parent;parent.child[side]=node
            bucket.insert(bucket.index(parent)+1,node)
            root=balance(root,node);front(bucket,root)
        else:
            bucket.append(node)
            if len(bucket)>=9:
                if len(self.buckets)<64:self.resize()
                else:treeify(bucket)
        self.size+=1
        if self.size>len(self.buckets)*3//4:self.resize()
    def order(self):return [list(n.key) for bucket in self.buckets for n in bucket]


def random_chunks():
    order=Order()
    for x in range(-10,9):
        for z in range(-7,12):order.add((x,z))
    return order.order()

SUMMARY

NUB is a set of high performance C++ codes providing indexes for
variable-length keys, compressed resource files, and an istream interface for
blocks of memory. Now supports Unicode keys and up to 17 trillion gigabyte file
sizes.


A Note on NUB Index Files

NUB indexes target storing ASCIIZ variable-length keys to resource files in a
highly efficient manner. Because of this requirement, nub indexes are not
B-trees but very similar. B-trees are a variant of M-way branching trees.
NUB indexes are not of any particular order M. Instead, each node packs in as 
many keys as will fit.

There are currently no special codes to balance the tree during insert and
delete so a tree can become badly unbalanced under certain dynamic
circumstances. To compensate for this, I am working on a function Index::pack()
which will balance the tree and optimize it for fastest access. This would be 
used once your resource file is complete (prior to release). I should warn you 
that for very large indexes this function takes a good bit of time.

Since NUB now supports huge files, I am re-thinking the pack() approach. It may
be more appropriate to work on the extra codes needed to do some balancing on
the fly. 

2009/02/06: I have addressed the main area where unbalance can occur.  During
key deletion, sibling nodes are now combined in the majority of cases where this
is possible.

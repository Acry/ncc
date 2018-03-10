#!/usr/bin/env python
"""I'd rather we did this in perl because
it is strictly a practical extraction and report
application. Anyway, this compresses nccout files
by removing duplicate data as structure declarations
from header files which are reported in every source
file."""

import sys

have = {}

def getsub (line, set):
    TS = ''
    TL = [ line ]
    for j in sys.stdin:
        if j[0] not in set:
            break
	j = j[:-1]
	TS += j
	TL.append (j)
    if line in have:
	X = have [line]
	if type(X) == type (''):
	    if X == TS:
		return
	    have [line] = [X, TS]
	else:
	    for k in have [line]:
	        if k == TS:
		    return
	    have [line].append (TS)
    else:
	have [line] = TS
    for j in TL:
	print j;
    print

for i in sys.stdin:
    if i [0] == '#':
	continue
    if i [0] == 'D':
	getsub (i[:-1], 'FgGsS')
    elif i [0] == 'P':
	getsub (i[:-1], 'YL')
    else:
	print i[:-1]


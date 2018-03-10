#!/usr/bin/env python

# This file is installed and used automatically by ncc each time it
# "links" nccout object files.

import sys

have = {}

def getsub (line, set):
    global outlines
    TS = ''
    TL = [ line ]
    for j in instream:
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
    outlines += len (TL)
    output ("\n".join (TL) + "\n")

infile, outfile = sys.argv [1:]
outlines = inlines = 0
def readinput (infile):
	global inlines
	for l in infile:
		yield l
		inlines += 1

instream = readinput (open (infile))
if infile == outfile:
	replace = True
	outfile = "nccstriptmp"
else:
	replace = False

write = open (outfile, "w").write
def output (x):
    global outlines
    outlines += 1
    write (x + "\n")

for i in instream:
    if i [0] == '#':
	continue
    if i [0] == 'D':
	getsub (i[:-1], 'FgGsS')
    elif i [0] == 'P':
	getsub (i[:-1], 'YL')
    else:
	output (i[:-1])

if inlines:
	print "nccstrip: -%.2f%%"% ((inlines - outlines) * 100.0 / inlines)
if replace:
	import os
	os.rename ("nccstriptmp", infile)

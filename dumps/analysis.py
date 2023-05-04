# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

from os import listdir
from os.path import isfile, join

import csv
import numpy

cur_files = [f for f in listdir(".") if isfile(join(".", f))]
cur_files.remove(__file__)          # Extract this script file.
cur_files.remove('summary.csv')     # Extract existing CSV file.
cur_files.remove('.DS_Store')       # Extract existing CSV file.


summary = []

for fname in cur_files:

    print("Processing {}".format(fname))

    with open(fname, "r") as f:
        lines = [int(line.rstrip()) for line in f]
        lines.sort()

        reqs = len(lines)

        avg = numpy.mean(lines)
        avg = round(avg, 2)
        tail_50 = lines[int(round(reqs * 0.5))]
        tail_90 = lines[int(round(reqs * 0.9))]
        tail_95 = lines[int(round(reqs * 0.95))]
        tail_99 = lines[int(round(reqs * 0.99))]
        

        summary.append({
            "file": fname,
            "reqs": reqs,
            "avg": avg,
            "50th": tail_50,
            "90th": tail_90,
            "95th": tail_95,
            "99th": tail_99
        })


with open("summary.csv", "w") as f:
    writer = csv.DictWriter(f, fieldnames=['file', 'avg', 'reqs', '50th', '90th', '95th', '99th'], delimiter='\t')

    writer.writeheader()
    writer.writerows(summary)





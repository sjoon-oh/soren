# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

from os import listdir
from os.path import isfile, join
import copy

import csv
import numpy

cur_files = [f for f in listdir(".") if isfile(join(".", f))]
exclude_files = []

for fname in cur_files:
    if 'txt' in fname: 
        continue
    else: exclude_files.append(fname)

for fname in exclude_files:   
    cur_files.remove(fname)

for fname in cur_files:
    if ('live' in fname):
        
        new_fname = ""
        graph = []
        print("Processing {}".format(fname))

        with open(fname, "r") as f:
            new_fname = fname.replace('.txt', '.graph')

            lines = [int(line.rstrip()) for line in f]
            reqs = len(lines)

            for nth_req in range(1, 1 + reqs):

                throu = nth_req * (10 ** 9) / lines[nth_req - 1]
                
                # Sample!
                if (nth_req % 1000 == 0):
                    graph.append({
                        'timepoints': lines[nth_req - 1] / (10 ** 9),
                        'throughput': throu * 0.001
                    })

            with open(new_fname, "w") as f2:
                
                print("Writing to: {}".format(new_fname))

                writer = csv.DictWriter(f2, fieldnames=['timepoints', 'throughput'], delimiter='\t')
                writer.writerows(graph)

        del graph
    
    else:
        continue



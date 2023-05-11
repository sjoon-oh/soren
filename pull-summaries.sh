#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

cd dumps

scp -r -q oslab@node1:~/sjoon/workspace/git/soren/dumps ./dumps-1
scp -r -q oslab@node2:~/sjoon/workspace/git/soren/dumps ./dumps-2
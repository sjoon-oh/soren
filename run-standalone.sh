#!/bin/bash

# github.com/sjoon-oh/soren
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 
# Project SOREN
# 

rm *log
rm -r logs

mkdir logs

./make.sh
wait

key_sz=16
# payload_sz=(32 50 64 128 256 512)
payload_sz=512

# for psz in ${payload_sz[@]}
# do
#     echo "Standalone test for payload size ${psz}"
#     ./build/soren-standalone.demo ${psz} ${key_sz}
#     wait

#     dump_file="soren-st-pl${psz}-k${key_sz}-$(date '+%y.%m.%d-%H:%M').txt"
#     mv soren-dump.txt dumps/${dump_file}
# done


echo "Standalone test for payload size ${payload_sz}"
./build/soren-standalone.demo ${payload_sz} 16
wait

dump_file="soren-st-pl${payload_sz}-k16-$(date '+%y.%m.%d-%H:%M').txt"
mv soren-dump.txt dumps/${dump_file}

mv *.log ./logs
mv *.json ./logs

cd dumps
python3 analysis.py

echo "***** Script finished successfully. *****"
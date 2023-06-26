cd redis-7.0.5
# diff -uNr src/Makefile-orig src/Makefile > ../redis-soren.patch
diff -u7 -Nr src/server-orig.c src/server.c > ../redis-soren.patch
diff -u7 -Nr src/server-orig.h src/server.h >> ../redis-soren.patch

diff -u7 -Nr src/sds-orig.c src/sds.c >> ../redis-soren.patch
diff -u7 -Nr src/sds-orig.h src/sds.h >> ../redis-soren.patch
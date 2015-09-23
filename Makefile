#
# makefile for csc424-152 project 4
# author:
# created: 29 mar 2015
# last update: 
#

COPTS= -Wall
P= 4434
H= localhost
PWD= HeLlO
MACFIX= -D MACFIX
HR= antietam
PR= 5545

ttftp: ttftp.c ttftp-client.o aes.o ttftp.h mynetfunctions.o
	cc ${COPTS} ${MACFIX} -o $@ $< ttftp-client.o mynetfunctions.o aes.o

ttftp-client.o: ttftp-client.c aes.o ttftp.h
	cc ${COPTS} ${MACFIX} -c $<

mynetfunctions.o: mynetfunctions.c ttftp.h
	cc ${COPTS} ${MACFIX} -c $<

aes.o:
	@echo Assumes the tiny AES code is moved into directory ./tiny-AES1238-C
	cd tiny-AES128-C ; make clean ; make ;
	cp tiny-AES128-C/aes.h .
	cp tiny-AES128-C/aes.o .

run-server: ttftp
	./ttftp -vdl -k ${PWD} ${P}
	
test-basic: ttftp
	./ttftp ${H} ${P} myfile.txt > myfile.out 
	./ttftp -k ${PWD} ${H} ${P} myfile.txt > myfile_e.out 
	diff myfile.out myfile_e.out

test-basic-remote: ttftp
	make test-basic-clean
	make test-basic-make
	for file in abc4.txt abc16.txt zero31.bin zero32.bin zero33.bin zero488.bin zero504.bin zero512.bin zero520.bin ; do \
		./ttftp ${HR} ${PR} $$file > $$file.out ; \
	done

test-basic-clean:
	-rm myfile.out myfile_e.out
	-rm abc4.txt.out abc16.txt.out zero31.bin.out zero32.bin.out zero33.bin.out 
	-rm zero488.bin.out zero504.bin.out zero512.bin.out zero520.bin.out
	-rm zero31.bin zero32.bin zero33.bin zero488.bin zero504.bin zero512.bin zero520.bin 

test-basic-make:
	for count in 31 32 33 488 504 512 520 ; do \
		dd if=/dev/zero of=zero$$count.bin bs=1 count=$$count ; \
	done

clean:
	make test-basic-clean
	-rm ttftp mynetfunctions.o ttftp-client.o myfile.out
	-rm aes.h aes.o
	-rm test.report


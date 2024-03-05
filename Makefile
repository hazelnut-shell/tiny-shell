tsh: tsh.o auth.o helper.o history.o job.o proc.o
	gcc tsh.o auth.o helper.o history.o job.o proc.o -o tsh

tsh.o: tsh.c
	gcc -c -o tsh.o -I ./include tsh.c

auth.o: auth.c
	gcc -c -o auth.o -I ./include auth.c

helper.o: helper.c
	gcc -c -o helper.o -I ./include helper.c

history.o: history.c
	gcc -c -o history.o -I ./include history.c

job.o: job.c
	gcc -c -o job.o -I ./include job.c

proc.o: proc.c
	gcc -c -o proc.o -I ./include proc.c

.PHONY: clean run

clean: 
	rm -f tsh.o auth.o helper.o history.o job.o proc.o tsh  

run:
	./tsh

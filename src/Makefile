# Makefile for student project Compilers 2009-2010
#
# Dries Van Dyck
# Department WNI
# Theoretical Computer Science Group
# Hasselt University 
#
# januari 2010
#
# Options for development
CC=g++
CCOPT=-c -g -Wall 
LEX=flex
CCLIBOPTIONS=-lfl
BINDIR=

all: mybc

mybc: bc.o lex.yy.o y.tab.o absyn.o
	$(CC) -g -Wall -o $(BINDIR)mybc bc.o lex.yy.o y.tab.o absyn.o

bc.o: main.c lexer.h bc.h y.tab.c
	$(CC) $(CCOPT) -o bc.o main.c 

absyn.o: absyn.h absyn.cpp
	$(CC) $(CCOPT) -o absyn.o absyn.cpp

y.tab.o: y.tab.c y.tab.h absyn.h
	$(CC) $(CCOPT) -o y.tab.o y.tab.c

y.tab.c: bc.y
	bison -d -y -v bc.y 

lexer: lexer.o lex.yy.o
	$(CC) -o $(BINDIR)lexer lexer.o lex.yy.o $(CCLIBOPTIONS)

lexer.o: lexer.c tokens.h lexer.h
	$(CC) $(CCOPT) lexer.c

lex.yy.c: bc.lex
	$(LEX) bc.lex	

lex.yy.o: lex.yy.c y.tab.h #tokens.h
	$(CC) $(CCOPT) lex.yy.c

clean: 
	rm -f *.o *.tab.c *.tab.h *.out *.err *.bak *.yy.c *~ \#*\# y.output


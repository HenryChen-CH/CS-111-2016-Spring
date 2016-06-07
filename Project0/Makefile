lab0 : lab0.c
	@gcc -Wall -g -o lab0 lab0.c

check: lab0
	@touch a.txt
	@chmod a+rw a.txt
	@echo "Good job, hao!" > ./a.txt
	@echo "------------------------"
	@echo "input=a.txt output=b.txt"
	@./lab0 --input=a.txt --output=b.txt; echo "Exit with $$?"
	@diff a.txt b.txt; ([ $$? -eq 0 ] && echo "succeed same file") || echo "fail, different file" 
	@rm -f ./b.txt
	@echo "------------------------"
	@echo "input=c.txt output=b.txt : invalid input"
	@./lab0 --input=c.txt --output=b.txt; echo "Exit with $$?"
	@echo "------------------------"
	@echo "input=a.txt output=b.txt --segfault"	
	@./lab0 --input=a.txt --output=b.txt --segfault; echo "Exit with $$?"
	@echo "------------------------"
	@echo "input=a.txt output=b.txt --segfault --catch"	
	@./lab0 --input=a.txt --output=b.txt --segfault --catch; echo "Exit with $$?"
	
	@ mkdir tmp
	@ chmod a-w tmp
	@echo "------------------------"
	@echo "input=a.txt output=./tmp/b.txt tmp has no read permission"
	@./lab0 --input=a.txt --output=./tmp/b.txt; echo "Exit with $$?"
	@rm -f lab0 *.txt
	@ rm -rf tmp
	
clean:
	@rm -f lab0 *.txt
	@ rm -rf tmp


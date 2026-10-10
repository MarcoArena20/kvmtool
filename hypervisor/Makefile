all: split_file join_files

split_file: split_file.c
	gcc -Wall -Wextra -O2 split_file.c -o split_file

join_files: join_files.c
	gcc -Wall -Wextra -O2 join_files.c -o join_files

clean:
	rm split_file join_files

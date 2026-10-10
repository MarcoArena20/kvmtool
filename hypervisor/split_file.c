#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define BUFFER_SIZE (64 * 1024)
#define DEFAULT_CHUNK_MB 100ULL
#define BYTES_PER_MB 1000000ULL

int main(int argc, char *argv[])
{
    	const char *input_name;
    	unsigned long long chunk_mb = DEFAULT_CHUNK_MB;
    	unsigned long long chunk_size;
    	unsigned long long part_number = 1;
    	unsigned long long chunks_created = 0;
    	unsigned long long written_in_part = 0;

    	unsigned char buffer[BUFFER_SIZE];
    	FILE *input = NULL;
    	FILE *output = NULL;
    	char *output_name = NULL;
    	int status = EXIT_FAILURE;
	
	// Verifichiamo che lo splitter venga utilizzato correttamente
    	if (argc < 2 || argc > 3) {
        	
		printf("Uso: %s <file> [dimensione_chunk_MB]\n",argv[0]);
        	return EXIT_FAILURE;
    	}

    	input_name = argv[1];
	
	// Se viene passato il parametro opzionale [dimensione_chunk_MB]
	// andiamo a leggere tale valore e convertiamolo in unsigned long long
    	if (argc == 3) {
        	
		char *end = NULL;

        	errno = 0;
        	chunk_mb = strtoull(argv[2], &end, 10);
		
		// Verifichiamo la correttezza del valore passato
        	if (errno == ERANGE || end == argv[2] || *end != '\0' ||
            		chunk_mb == 0 || chunk_mb > ULLONG_MAX / BYTES_PER_MB) {
            		
			printf("Dimensione del chunk non valida.\n");
            		return EXIT_FAILURE;
        	
		}
    	}
	
	// Apriamo il file da splittare
    	chunk_size = chunk_mb * BYTES_PER_MB;

    	input = fopen(input_name, "rb");
    	if (input == NULL) {
        	
		perror("Errore apertura file di input");
        	goto cleanup;
    	
	}
	
	// Iteriamo la lettura per chunk fintanto che il file non è terminato:
	// ad ogni iterazione vengono letti al più un numero di byte pari a
	// BUFFER_SIZE
    	while (1) {

        	size_t bytes_read = fread(buffer, 1, sizeof(buffer), input);

        	if (bytes_read == 0) {
            		
			if (ferror(input)) {
                		
				perror("Errore durante la lettura");
                		goto cleanup;
            		}
            		
			// Arrivati alla fine del file usciamo dal loop
			break;
        	}

        	size_t position = 0;

        	// Presentandosi la possibilità di leggere un numero di byte
		// che non corrisponde ad un sottomultiplo della dimensione di un chunk
		// verifichiamo che la lettura della dimensione di un buffer non sia a
		// cavallo tra due chunk
        	while (position < bytes_read) {
			
            		if (output == NULL) {
                		size_t name_capacity = strlen(input_name) + 32;

                		output_name = malloc(name_capacity);
                		if (output_name == NULL) {
                    			perror("Errore allocazione memoria");
                    			goto cleanup;
                		}

                		snprintf(output_name, name_capacity,
                         				"%s.part%03llu",
                         			input_name, part_number);

				// Apriamo il file per scrivere il chunk i-esimo
                		output = fopen(output_name, "wbx");

                		if (output == NULL) {
                    			
					perror(output_name);
                    			free(output_name);
                    			output_name = NULL;
                    			goto cleanup;
                		}
            		}

            		unsigned long long remaining = chunk_size - written_in_part;
			size_t bytes_to_write = bytes_read - position;

            		if ((unsigned long long)bytes_to_write > remaining) {
                		
				bytes_to_write = (size_t)remaining;
            		
			}

            		size_t bytes_written = fwrite(buffer + position, 1, bytes_to_write, output);

            		if (bytes_written != bytes_to_write) {
                		
				printf("Errore durante la scrittura di %s\n",output_name);
                		goto cleanup;
            		
			}

            		position += bytes_written;
            		written_in_part += (unsigned long long)bytes_written;

            		// Terminato un chunk, passiamo al prossimo
            		if (written_in_part == chunk_size) {
                		
				if (fclose(output) != 0) {
                    			
					output = NULL;
                    			printf("Errore chiusura file %s\n",output_name);
                    			free(output_name);
                    			output_name = NULL;
                    			goto cleanup;
                		}

                	printf("Creato: %s (%llu byte)\n",output_name, written_in_part);

                	output = NULL;
                	free(output_name);
                	output_name = NULL;

                	written_in_part = 0;
                	part_number++;
                	chunks_created++;
            		
			}
        	}
    	}

    	// Salviamo l'ultimo chunk se è più piccolo della dimensione richiesta
    	if (output != NULL) {
        	
		if (fclose(output) != 0) {
            		
			output = NULL;
            		printf("Errore chiusura file %s\n",output_name);
            		free(output_name);
            		output_name = NULL;
            		goto cleanup;
        	
		}

        	output = NULL;

        	printf("Creato: %s (%llu byte)\n", output_name, written_in_part);

        	free(output_name);
        	output_name = NULL;
        	chunks_created++;
    	}

    	if (fclose(input) != 0) {
        	
		input = NULL;
        	perror("Errore chiusura file di input");
        	goto cleanup;
    	
	}

    	input = NULL;

    	if (chunks_created == 0) {
        	
		printf("Il file è vuoto: nessun chunk creato.\n");
    	
	} else {
        	
		printf("\nOperazione completata: %llu chunk creati.\n",
               	chunks_created);
    	
	}

    	status = EXIT_SUCCESS;

cleanup:
    	if (output != NULL) {
        	fclose(output);
    	}

    	if (input != NULL) {
        	fclose(input);
    	}

    	free(output_name);

    	return status;
}


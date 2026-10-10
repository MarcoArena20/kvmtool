#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE (64 * 1024)

int main(int argc, char *argv[])
{
    unsigned char buffer[BUFFER_SIZE];
    FILE *input = NULL;
    FILE *output = NULL;

    unsigned long long total_bytes = 0;
    const char *output_name;

    /*
     * Sintassi:
     * join_files <file_ricostruito> <chunk1> <chunk2> ...
     */
    if (argc < 3) {
        fprintf(stderr,
                "Uso: %s <file_output> <chunk1> <chunk2> ...\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    output_name = argv[1];

    /*
     * Crea il file di destinazione senza sovrascrivere
     * un file gia' esistente.
     */
    output = fopen(output_name, "wbx");

    if (output == NULL) {
        perror("Errore creazione file di output");
        return EXIT_FAILURE;
    }

    /*
     * Elabora i chunk nell'ordine ricevuto.
     * argv[2] e' il primo chunk, argv[3] il secondo, ecc.
     */
    for (int i = 2; i < argc; i++) {

        input = fopen(argv[i], "rb");

        if (input == NULL) {
            perror(argv[i]);
            goto cleanup;
        }

        /* Copia il chunk corrente a blocchi di 64 KiB. */
        size_t bytes_read;

        while ((bytes_read =
                    fread(buffer, 1, sizeof(buffer), input)) > 0) {

            size_t bytes_written =
                fwrite(buffer, 1, bytes_read, output);

            if (bytes_written != bytes_read) {
                fprintf(stderr,
                        "Errore scrittura file di destinazione.\n");
                goto cleanup;
            }

            total_bytes += (unsigned long long)bytes_written;
        }

        if (ferror(input)) {
            perror("Errore lettura chunk");
            goto cleanup;
        }

        if (fclose(input) != 0) {
            input = NULL;
            perror("Errore chiusura chunk");
            goto cleanup;
        }

        input = NULL;

        printf("Aggiunto: %s\n", argv[i]);
    }

    /*
     * Chiude il file ricostruito e controlla
     * che la chiusura termini senza errori.
     */
    if (fclose(output) != 0) {
        output = NULL;
        perror("Errore chiusura file ricostruito");
        goto cleanup;
    }

    output = NULL;

    printf("\nRicostruzione completata.\n");
    printf("File: %s\n", output_name);
    printf("Byte scritti: %llu\n", total_bytes);

    return EXIT_SUCCESS;

cleanup:
    if (input != NULL) {
        fclose(input);
    }

    if (output != NULL) {
        fclose(output);
    }

    /*
     * Elimina il file parziale in caso di errore.
     * Il file era stato creato in modo esclusivo.
     */
    remove(output_name);

    return EXIT_FAILURE;
}


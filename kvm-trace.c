#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "kvm/kvm-trace.h"
#include "kvm/kvm-cpu.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


struct kvm_trace *kvm_trace__create(size_t capacity)
{
    struct kvm_trace *trace;
    size_t state_size;

    if (capacity == 0) {
        errno = EINVAL;
        return NULL;
    }

    state_size = kvm_trace_arch__state_size();

    if (state_size == 0) {
        errno = EINVAL;
        return NULL;
    }

    /* Evita overflow durante il calcolo delle dimensioni. */
    if (capacity > SIZE_MAX / sizeof(*trace->entries) ||
        capacity > SIZE_MAX / state_size) {
        errno = EOVERFLOW;
        return NULL;
    }

    trace = calloc(1, sizeof(*trace));
    if (!trace)
        return NULL;

    trace->entries = calloc(capacity, sizeof(*trace->entries));
    if (!trace->entries)
        goto error;

    trace->arch_states = calloc(capacity, state_size);
    if (!trace->arch_states)
        goto error;

    trace->count = 0;
    trace->capacity = capacity;
    trace->arch_state_size = state_size;

    /*
     * Collega ogni entry alla propria porzione del buffer
     * contenente gli stati specifici dell'architettura.
     */
    for (size_t i = 0; i < capacity; i++) {
        trace->entries[i].arch_state =
            (struct kvm_trace_arch_state *)
            ((unsigned char *)trace->arch_states +
             i * state_size);
    }

    return trace;

error:
    free(trace->arch_states);
    free(trace->entries);
    free(trace);

    return NULL;
}

int kvm_trace__record(struct kvm_trace *trace, struct kvm_cpu *vcpu)
{
    struct kvm_trace_entry *entry;
    int ret;

    if (!trace || !vcpu)
        return -EINVAL;

    if (!trace->entries || !trace->arch_states ||
        trace->arch_state_size == 0)
        return -EINVAL;

    /*
     * Non scrivere oltre la capacità allocata.
     */
    if (trace->count >= trace->capacity)
        return -ENOSPC;

    /*
     * Recupera la prossima entry disponibile.
     */
    entry = &trace->entries[trace->count];

    /*
     * Inizializza i dati dello step corrente.
     * Il puntatore arch_state è già stato impostato
     * da kvm_trace__create().
     */
    entry->step = trace->count;
    entry->pc = 0;

    memset(entry->arch_state, 0, trace->arch_state_size);

    /*
     * Acquisisce il PC e lo stato architetturale.
     */
    ret = kvm_trace_arch__capture(vcpu,
                                  &entry->pc,
                                  entry->arch_state);
    if (ret < 0)
        return ret;

    /*
     * Lo step viene considerato registrato soltanto
     * dopo che la cattura è terminata correttamente.
     */
    trace->count++;

    return 0;
}

int kvm_trace__save(const struct kvm_trace *trace,
                    const char *path)
{
    FILE *fp;
    int ret = 0;

    if (!trace || !path)
        return -EINVAL;

    if (!trace->entries ||
        !trace->arch_states ||
        trace->arch_state_size == 0 ||
        trace->count > trace->capacity)
        return -EINVAL;

    fp = fopen(path, "wb");
    if (!fp)
        return -errno;

    /*
     * Scrive il numero di entry valide.
     */
    if (fwrite(&trace->count,
               sizeof(trace->count), 1, fp) != 1) {
        ret = -EIO;
        goto out;
    }

    /*
     * Scrive ciascuna entry:
     * step -> pc -> stato architetturale.
     */
    for (size_t i = 0; i < trace->count; i++) {
        const struct kvm_trace_entry *entry =
            &trace->entries[i];

        if (!entry->arch_state) {
            ret = -EINVAL;
            goto out;
        }

        if (fwrite(&entry->step,
                   sizeof(entry->step), 1, fp) != 1 ||
            fwrite(&entry->pc,
                   sizeof(entry->pc), 1, fp) != 1 ||
            fwrite(entry->arch_state,
                   trace->arch_state_size, 1, fp) != 1) {
            ret = -EIO;
            goto out;
        }
    }

out:
    /*
     * Controlla anche gli errori durante la chiusura.
     */
    if (fclose(fp) != 0 && ret == 0)
        ret = -EIO;

    return ret;
}

struct kvm_trace *kvm_trace__load(const char *path)
{
    FILE *fp;
    struct kvm_trace *trace = NULL;
    size_t count;
    int saved_errno = 0;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    /*
     * Legge il numero di entry salvato da kvm_trace__save().
     */
    if (fread(&count, sizeof(count), 1, fp) != 1) {
        saved_errno = ferror(fp) ? EIO : EINVAL;
        goto error;
    }

    /*
     * Una traccia vuota non è accettata dalla versione
     * corrente di kvm_trace__create().
     */
    if (count == 0) {
        saved_errno = EINVAL;
        goto error;
    }

    /*
     * Alloca gli array comuni e il buffer architetturale.
     * I puntatori arch_state vengono inizializzati da create().
     */
    trace = kvm_trace__create(count);
    if (!trace) {
        saved_errno = errno ? errno : ENOMEM;
        goto error;
    }

    /*
     * Ricostruisce tutte le entry.
     */
    for (size_t i = 0; i < count; i++) {
        struct kvm_trace_entry *entry = &trace->entries[i];

        if (fread(&entry->step,
                  sizeof(entry->step), 1, fp) != 1 ||
            fread(&entry->pc,
                  sizeof(entry->pc), 1, fp) != 1 ||
            fread(entry->arch_state,
                  trace->arch_state_size, 1, fp) != 1) {
            saved_errno = ferror(fp) ? EIO : EINVAL;
            goto error;
        }

        /*
         * L'entry viene considerata valida solo dopo
         * aver letto tutti i suoi campi.
         */
        trace->count++;
    }

    if (fclose(fp) != 0) {
        saved_errno = errno ? errno : EIO;
        kvm_trace__free(trace);
        errno = saved_errno;
        return NULL;
    }

    return trace;

error:
    fclose(fp);
    kvm_trace__free(trace);
    errno = saved_errno;
    return NULL;
}

void kvm_trace__free(struct kvm_trace *trace)
{
    if (!trace)
        return;

    free(trace->arch_states);
    free(trace->entries);
    free(trace);
}

const struct kvm_trace_entry *kvm_trace__get(const struct kvm_trace *trace, size_t index)
{
    if (!trace || !trace->entries)
        return NULL;

    if (index >= trace->count)
        return NULL;

    return &trace->entries[index];
}

static bool has_bin_extension(const char *name)
{
    size_t len;

    if (!name)
        return false;

    len = strlen(name);

    return len >= 4 && strcmp(name + len - 4, ".bin") == 0;
}

/*
 * Confronta due indirizzi PC per qsort().
 */
static int compare_pc(const void *a, const void *b)
{
    uint64_t pc_a = *(const uint64_t *)a;
    uint64_t pc_b = *(const uint64_t *)b;

    return (pc_a > pc_b) - (pc_a < pc_b);
}

/*
 * Conta gli indirizzi PC distinti nella traccia.
 */
static int count_unique_pcs(const struct kvm_trace *trace,
                            size_t *unique_count)
{
    uint64_t *pcs;

    if (!trace || !unique_count)
        return -EINVAL;

    *unique_count = 0;

    if (trace->count == 0)
        return 0;

    if (trace->count > SIZE_MAX / sizeof(*pcs))
        return -EOVERFLOW;

    pcs = malloc(trace->count * sizeof(*pcs));
    if (!pcs)
        return -ENOMEM;

    for (size_t i = 0; i < trace->count; i++) {
        const struct kvm_trace_entry *entry;

        entry = kvm_trace__get(trace, i);
        if (!entry) {
            free(pcs);
            return -EINVAL;
        }

        pcs[i] = entry->pc;
    }

    qsort(pcs, trace->count, sizeof(*pcs), compare_pc);

    *unique_count = 1;

    for (size_t i = 1; i < trace->count; i++) {
        if (pcs[i] != pcs[i - 1])
            (*unique_count)++;
    }

    free(pcs);
    return 0;
}

/*
 * Analizza tutti i file .bin presenti direttamente
 * nella directory specificata.
 */
int kvm_trace__analyze_dir(const char *dir_path)
{
    DIR *dir;
    struct dirent *entry;
    size_t files_found = 0;
    size_t files_analyzed = 0;
    size_t errors = 0;
    int dir_error = 0;

    if (!dir_path)
        return -EINVAL;

    dir = opendir(dir_path);
    if (!dir)
        return -errno;

    while (1) {
        char *path;
        size_t dir_len;
        size_t name_len;
        size_t path_len;
        bool has_slash;
        struct stat st;
        struct kvm_trace *trace;
        size_t unique_pcs;
        int ret;

        /*
         * Distingue la fine della directory da un errore
         * di readdir(). Le funzioni chiamate nel ciclo
         * possono modificare errno.
         */
        errno = 0;
        entry = readdir(dir);

        if (!entry) {
            if (errno)
                dir_error = -errno;
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (!has_bin_extension(entry->d_name))
            continue;

        /*
         * Costruisce il percorso completo senza assumere
         * una dimensione fissa massima.
         */
        dir_len = strlen(dir_path);
        name_len = strlen(entry->d_name);
        has_slash = dir_len > 0 &&
                    dir_path[dir_len - 1] == '/';

        if (dir_len > SIZE_MAX - name_len -
                      (has_slash ? 1 : 2)) {
            fprintf(stderr, "Path too long: %s\n",
                    entry->d_name);
            errors++;
            continue;
        }

        path_len = dir_len + name_len +
                   (has_slash ? 1 : 2);

        path = malloc(path_len);
        if (!path) {
            fprintf(stderr, "Out of memory\n");
            errors++;
            continue;
        }

        if (has_slash)
            snprintf(path, path_len, "%s%s",
                     dir_path, entry->d_name);
        else
            snprintf(path, path_len, "%s/%s",
                     dir_path, entry->d_name);

        /*
         * Ignora directory e altri oggetti che terminano
         * con ".bin": analizziamo soltanto file regolari.
         */
        if (stat(path, &st) < 0) {
            fprintf(stderr, "Cannot stat %s: %s\n",
                    path, strerror(errno));
            free(path);
            errors++;
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            free(path);
            continue;
        }

        files_found++;

        /*
         * Ricostruisce la traccia dal file binario.
         */
        trace = kvm_trace__load(path);
        if (!trace) {
            fprintf(stderr, "Cannot load trace: %s\n",
                    path);
            free(path);
            errors++;
            continue;
        }

        /*
         * Calcola gli indirizzi PC distinti.
         */
        ret = count_unique_pcs(trace, &unique_pcs);
        if (ret < 0) {
            fprintf(stderr,
                    "Cannot analyze %s: error %d\n",
                    path, ret);

            kvm_trace__free(trace);
            free(path);
            errors++;
            continue;
        }

        printf("%s: steps=%zu unique_pcs=%zu\n",
               path, trace->count, unique_pcs);

        files_analyzed++;

        kvm_trace__free(trace);
        free(path);
    }

    if (closedir(dir) < 0 && !dir_error)
        dir_error = -errno;

    printf("\nAnalysis summary\n");
    printf("Files found:     %zu\n", files_found);
    printf("Files analyzed:  %zu\n", files_analyzed);
    printf("Errors:          %zu\n", errors);

    if (dir_error)
        return dir_error;

    return errors ? -EIO : 0;
}

bool kvm_trace__detect_cycle(struct kvm_trace *trace,
                             size_t max_period,
                             size_t min_repetitions)
{
    size_t max_candidate;

    if (!trace || !trace->entries ||
        max_period == 0 || min_repetitions < 2)
        return false;

    /* Se il ciclo è già stato rilevato, non sovrascrivere i dati. */
    if (trace->has_cycle)
        return true;

    /* Non abbiamo ancora abbastanza entry. */
    if (trace->count / min_repetitions == 0)
        return false;

    max_candidate = trace->count / min_repetitions;

    if (max_candidate > max_period)
        max_candidate = max_period;

    /*
     * Prova periodi di lunghezza crescente.
     * Il primo ciclo trovato è quello più corto tra
     * quelli verificati.
     */
    for (size_t period = 1;
         period <= max_candidate;
         period++) {
        size_t start =
            trace->count - min_repetitions * period;
        bool repeated = true;

        /*
         * Confronta ogni blocco con la prima occorrenza.
         */
        for (size_t r = 1;
             r < min_repetitions && repeated;
             r++) {
            for (size_t j = 0; j < period; j++) {
                size_t first = start + j;
                size_t current = start + r * period + j;

                if (trace->entries[first].pc !=
                    trace->entries[current].pc) {
                    repeated = false;
                    break;
                }
            }
        }

        if (repeated) {
            trace->has_cycle = true;
            trace->cycle_start = start;
            trace->cycle_length = period;
            trace->cycle_repetitions = min_repetitions;

            return true;
        }
    }

    return false;
}

#include "utils.h"
#include <stdlib.h>
#include <string.h>
//implementazione_personalizzata_della_funzione_strdup_per_evitare_warning_POSIX
char *my_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = malloc(len);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}
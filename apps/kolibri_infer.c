
#include "knp_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* argv0){
    fprintf(stderr,
        "kolibri_infer — on-the-fly numeric inference\n"
        "Usage: %s --q <uint64> [--beam N] [--depth N] [--theta CSV]\n"
        "Env: KNP_THETA=\"csv\"\n", argv0);
}

static size_t load_theta_file(const char* path, double* theta){
    if (!path || !theta) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char buffer[512];
    size_t count = 0;
    if (fgets(buffer, sizeof(buffer), f)){
        char* cursor = buffer;
        while (*cursor && count < KNP_THETA_MAX){
            char* endptr = NULL;
            double value = strtod(cursor, &endptr);
            if (cursor == endptr) break;
            theta[count++] = value;
            cursor = endptr;
            while (*cursor == ',' || *cursor == ' ' || *cursor == '\t') cursor++;
        }
    }
    fclose(f);
    return count;
}

int main(int argc, char** argv){
    unsigned long long q=0ULL;
    int beam=8, depth=6;
    const char* theta_csv = getenv("KNP_THETA");
    const char* theta_file = getenv("KNP_THETA_FILE");
    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--q") && i+1<argc) q = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i],"--beam") && i+1<argc) beam = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--depth") && i+1<argc) depth = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--theta") && i+1<argc) theta_csv = argv[++i];
        else { usage(argv[0]); return 2; }
    }
    if (q==0ULL){ usage(argv[0]); return 2; }

    double theta[KNP_THETA_MAX]; size_t n_theta=0;
    if (theta_csv && *theta_csv){
        char* dup=strdup(theta_csv);
        char* tok=strtok(dup,",");
        while(tok && n_theta<KNP_THETA_MAX){
            theta[n_theta++]=atof(tok);
            tok=strtok(NULL,",");
        }
        free(dup);
    } else {
        if (!theta_file || !*theta_file){
            theta_file = "data/knp_theta.csv";
        }
        n_theta = load_theta_file(theta_file, theta);
        if (n_theta==0){
            theta[0]=1.0; theta[1]=0.3; theta[2]=-0.2; theta[3]=0.12; n_theta=4;
        }
    }

    uint64_t best_id=0; double v=0.0, s=0.0;
    if (knp_infer((uint64_t)q, theta, n_theta, beam, depth, &best_id, &v, &s)!=0){
        fprintf(stderr,"infer failed\n"); return 1;
    }
    printf("%llu %.17g %.17g\n", (unsigned long long)best_id, v, s);
    return 0;
}

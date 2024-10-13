#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITERS 100
#define NBENCHMARKS 5

typedef int(benchmark_fn)(const char *);
typedef void(cleanup_fn)(void);

benchmark_fn benchmark_ekjson, benchmark_jsmn, benchmark_jjson,
	benchmark_simdjson, benchmark_jsonc;
cleanup_fn cleanup_ekjson, cleanup_jsmn, cleanup_jjson,
	cleanup_simdjson, cleanup_jsonc;

static const struct benchmark {
	benchmark_fn	*fn;
	cleanup_fn	*cleanup;
	const char	*name;
} benchmarks[] = {
	{
		.fn = benchmark_ekjson,
		.cleanup = cleanup_ekjson,
		.name = "ekjson"
	},
	{
		.fn = benchmark_jjson,
		.cleanup = cleanup_jjson,
		.name = "jjson"
	},
	{
		.fn = benchmark_simdjson,
		.cleanup = cleanup_simdjson,
		.name = "simdjson"
	},
	{
		.fn = benchmark_jsonc,
		.cleanup = cleanup_jsonc,
		.name = "jsonc"
	},
	{
		.fn = benchmark_jsmn,
		.cleanup = cleanup_jsmn,
		.name = "jsmn"
	},
};

static volatile size_t __test;

// Vain attempt at trying to warm up shit
static void warmup(const char *src) {
	while (*src) src++, __test++;
}

//void cleanup_simdjson_end(void);

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("usage: ./benchmark [file]\n");
		return 1;
	}

	FILE *file = fopen(argv[1], "rb");
	if (!file) {
		printf("couldn't open file.\n");
		return 1;
	}

	fseek(file, 0, SEEK_END);
	size_t len = ftell(file);
	fseek(file, 0, SEEK_SET);
	char *str = malloc(len + 1);
	fread(str, 1, len, file);
	fclose(file);
	
	clock_t avg_time[NBENCHMARKS], total_time[NBENCHMARKS];

	double throughput[NBENCHMARKS];

	size_t filelen = strlen(str);
	printf("file len: %zu\n", filelen);
	for (int i = 0; i < 100; i++) warmup(str);

	for (int b = 0; b < NBENCHMARKS; b++) {
		clock_t times[ITERS];
		warmup(str);

		for (int i = 0; i < ITERS; i++) {
			clock_t start = clock();
			if (benchmarks[b].fn(str)) {
				printf("error!!!\n");
				return -1;
			}
			clock_t end = clock();
			times[i] = end - start;

			benchmarks[b].cleanup();
		}

		total_time[b] = 0;
		for (int a = 0; a < ITERS; a++) total_time[b] += times[a];
		avg_time[b] = total_time[b];
		avg_time[b] /= 1000;

		printf("benchmark %s\n", benchmarks[b].name);
		printf("avg time per parse (ms): %f\n",
			(double)avg_time[b]
			/ ((double)CLOCKS_PER_SEC / 1000.0f));
		printf("time total (%d iters) (ms): %f\n", ITERS,
			(double)total_time[b]
			/ ((double)CLOCKS_PER_SEC / 1000.0f));
		printf("%% of ekjson time (in total): %f%%\n",
			(double)total_time[b] / (double)total_time[0] * 100.0f);
		double secs = (double)avg_time[b] / (double)CLOCKS_PER_SEC;
		throughput[b] = ((double)filelen / 1024.0 / 1024.0 / 1024.0)
			/ secs;
		printf("Throughput (GB/s): %f\n\n", throughput[b]);
	}
	
	free(str);

	for (int i = 0; i < NBENCHMARKS; i++) {
		printf("%.2f, ", throughput[i]);
	}
	printf("\n");

	return 0;
}

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
	char* buf = NULL;
	size_t bufsize = 0;

	int N;
	printf("N=");
	scanf("%d\n", &N);

	int n = 0;
	while (!feof(stdin)) {
		getline(&buf, &bufsize, stdin);
		int status;
		if (waitpid(-1, &status, WNOHANG) > 0) {
			--n;
		}
		if (n == N) {
			fprintf(stderr, "\e[31merror: quota limit of N=%d was reached\e[39m\n", N);
			continue;
		}
		if (popen(buf, "r") != NULL) {
			++n;
			fprintf(stderr, "\e[32msuccessfully started (n=%d):\e[39m %s", n, buf);
		}
	}

	free(buf);
}
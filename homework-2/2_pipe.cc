#include <iostream>
#include <cstdio>
#include <exception>
#include <map>
#include <unistd.h>
#include <mutex>
#include <cstdlib>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/file.h>


std::map<int, int> parse_mapping(const std::string& filename) {
	FILE* f = fopen(filename.c_str(), "r");
	if (!f) { throw std::runtime_error("no such file"); }

	std::map<int, int> m;
	int a, b;
	while (fscanf(f, "%d:%d\n", &a, &b) == 2) {
		m[a] += b;
	}
	return m;
}

int main(int argc, char** argv) try {
	if (argc != 4) {
		printf("usage: %s dishes cleaner dryer\n", argv[0]);
		return 1;
	}
	const std::map<int, int> mapping_dishes = parse_mapping(argv[1]);
	const std::map<int, int> mapping_cleaner = parse_mapping(argv[2]);
	const std::map<int, int> mapping_dryer = parse_mapping(argv[3]);

	int tot_amount = 0;
	for (auto [dish, amount] : mapping_dishes) {
		tot_amount += amount;
	}

	const char* TABLE_LIMIT_str = getenv("TABLE_LIMIT");
	int TABLE_LIMIT;
	if (TABLE_LIMIT_str == nullptr || sscanf(getenv("TABLE_LIMIT"), "%d", &TABLE_LIMIT) != 1) {
		throw std::runtime_error("no TABLE_LIMIT specified");
	}
	printf("TABLE_LIMIT=%d\n", TABLE_LIMIT);

	int pipe_forward_fd[2];
	pipe(pipe_forward_fd);

	int pipe_backward_fd[2];
	pipe(pipe_backward_fd);

	int pid_cleaner = fork();

	if (pid_cleaner == 0) {
		int on_table = 0;
		for (auto [dish, amount] : mapping_dishes) {
			for (int i = 0; i < amount; ++i) {
				if (on_table == TABLE_LIMIT) {
					char flag;
					read(pipe_backward_fd[0], &flag, sizeof(flag));
					on_table -= 1;
				}

				sleep(mapping_cleaner.at(dish));
				on_table += 1;
				write(pipe_forward_fd[1], &dish, sizeof(dish));
				printf("\u001b[32mproduced:\u001b[0m %d\n", dish);
			}
			fflush(stdout);
		}
		printf("\u001b[38;5;8mexited\u001b[0m (cleaner)\n");
		return 0;
	}

	{
		int dish;
		int cnt = 0;
		char flag = 'd';
		while (cnt++ < tot_amount && read(pipe_forward_fd[0], &dish, sizeof(int)) > 0) {
            printf("\u001b[36mconsumed:\u001b[0m %d\n", dish);
			sleep(mapping_dryer.at(dish));
			write(pipe_backward_fd[1], &flag, sizeof(flag));
		}
		printf("\u001b[38;5;8mexited\u001b[0m (dryer)\n");
	}

	for (int i = 0; i < 2; ++i) {
		int status;
		wait(&status);
	}
} catch (std::exception& e) {
	fprintf(stderr, "error occured: \e[31m%s\e[39m\n", e.what());
	return 2;
}
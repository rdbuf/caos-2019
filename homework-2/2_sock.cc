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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>


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

	int pid_cleaner = fork();

	if (pid_cleaner == 0) {
		int on_table = 0;

		// server
		struct sockaddr_in saddr = {0};
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = htonl(INADDR_ANY);
		saddr.sin_port = htons(80000);
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		bind(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
		listen(sockfd, 0);

		// accept
		struct sockaddr_in caddr;
		socklen_t len = sizeof(caddr);
		int client_fd = accept(sockfd, (struct sockaddr*)&caddr, &len);

		for (auto [dish, amount] : mapping_dishes) {
			for (int i = 0; i < amount; ++i) {
				if (on_table == TABLE_LIMIT) {
					char flag;
					read(client_fd, (void*)&flag, sizeof(flag));
					on_table -= 1;
				}

				sleep(mapping_cleaner.at(dish));
				on_table += 1;
				write(client_fd, &dish, sizeof(dish));
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

		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		struct hostent* hptr = gethostbyname("127.0.0.1");
		struct sockaddr_in saddr = {0};
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = ((struct in_addr*) hptr->h_addr_list[0])->s_addr;
		saddr.sin_port = htons(80000);
		connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr));

		while (cnt++ < tot_amount && read(sockfd, &dish, sizeof(dish)) && write(sockfd, (void*)&flag, sizeof(flag))) {
			printf("\u001b[36mconsumed:\u001b[0m %d\n", dish);
			sleep(mapping_dryer.at(dish));
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
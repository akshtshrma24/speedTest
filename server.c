#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "server.h"
#include <curl/curl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>

#define BUFLEN 1024

double upload_speed = 0;
double download_speed = 0;

double uploadSpeed()
{
	CURLcode res;
	curl_mime *mime1;
	curl_mimepart *part1;
	CURL *curl;
	struct curl_slist *headers;
	FILE *file;
	struct stat file_info;
	curl_off_t file_size;

	// Initialize CURL and set headers
	curl = curl_easy_init();
	headers = curl_slist_append(NULL, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
	headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
	headers = curl_slist_append(headers, "Cache-Control: max-age=0");
	headers = curl_slist_append(headers, "Connection: keep-alive");
	headers = curl_slist_append(headers, "Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryHjG99dqsmQzqwhgB");
	headers = curl_slist_append(headers, "Origin: http://www.csm-testcenter.org");
	headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");

	// Open the file
	const char *file_path = "./1mb.txt";
	file = fopen(file_path, "rb");
	if (!file)
	{
		fprintf(stderr, "Error: Cannot open file\n");
		return -1.0;
	}

	// Get file size
	if (fstat(fileno(file), &file_info) != 0)
	{
		fprintf(stderr, "Error: Cannot get file size\n");
		fclose(file);
		return -1.0;
	}
	file_size = (curl_off_t)file_info.st_size;

	// Initialize MIME structure
	mime1 = curl_mime_init(curl);

	// Add file part
	part1 = curl_mime_addpart(mime1);
	curl_mime_filedata(part1, file_path);
	curl_mime_name(part1, "file_upload");

	// Perform the request
	curl_easy_setopt(curl, CURLOPT_URL, "http://www.csm-testcenter.org/test?do=test&subdo=file_upload");
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	FILE *devnull = fopen("/dev/null", "w+");
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
	res = curl_easy_perform(curl);

	// Clean up and close resources
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	curl_mime_free(mime1);
	fclose(file);

	// Check for errors
	if (res != CURLE_OK)
	{
		fprintf(stderr, "Error: %s\n", curl_easy_strerror(res));
		return -1.0;
	}

	// Get upload speed
	double speed;
	res = curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed);
	if (res != CURLE_OK)
	{
		fprintf(stderr, "Error getting upload speed: %s\n", curl_easy_strerror(res));
		return -1.0;
	}

	return speed;
}

double downloadSpeed()
{
	CURL *curl = curl_easy_init();
	if (curl)
	{
		CURLcode res;
		curl_easy_setopt(curl, CURLOPT_URL, "https://gist.githubusercontent.com/khaykov/a6105154becce4c0530da38e723c2330/raw/41ab415ac41c93a198f7da5b47d604956157c5c3/gistfile1.txt");
		FILE *devnull = fopen("/dev/null", "w+");
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);

		/* Perform the request */
		res = curl_easy_perform(curl);

		if (!res)
		{
			double speed;
			res = curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed);
			return speed;
		}
		else
		{
			return -1.0;
		}
		fclose(devnull);
		curl_easy_cleanup(curl);
	}
	else
	{
		return -1.0;
	}
	curl_global_cleanup();
}

struct Server server_Constructor(int domain, int port, int service, int protocol, int backlog, u_long interface, void (*launch)(struct Server *server))
{
	struct Server server;

	server.domain = domain;
	server.service = service;
	server.port = port;
	server.protocol = protocol;
	server.backlog = backlog;

	server.address.sin_family = domain;
	server.address.sin_port = htons(port);
	server.address.sin_addr.s_addr = htonl(interface);

	server.socket = socket(domain, service, protocol);
	if (server.socket < 0)
	{
		perror("Failed to initialize/connect to socket...\n");
		exit(EXIT_FAILURE);
	}

	if (bind(server.socket, (struct sockaddr *)&server.address, sizeof(server.address)) < 0)
	{
		perror("Failed to bind socket...\n");
		exit(EXIT_FAILURE);
	}

	if (listen(server.socket, server.backlog) < 0)
	{
		perror("Failed to start listening...\n");
		exit(EXIT_FAILURE);
	}

	server.launch = launch;
	return server;
}

// Loop here that runs in thread
// global variables that are locked and unlocked
// then the variables are in the returned html

void *scrape(void *vargp)
{
	while (0 == 0)
	{
		upload_speed = uploadSpeed();
		download_speed = downloadSpeed();
		sleep(5);
	}
	return NULL;
}

void launch(struct Server *server)
{
	char buffer[BUFFER_SIZE];
	while (1)
	{
		printf("=== Starting Server === \n");
		int addrlen = sizeof(server->address);
		int new_socket = accept(server->socket, (struct sockaddr *)&server->address, (socklen_t *)&addrlen);
		ssize_t bytesRead = read(new_socket, buffer, BUFFER_SIZE - 1);
		if (bytesRead >= 0)
		{
			buffer[bytesRead] = '\0'; // Null terminate the string
			puts(buffer);
		}
		else
		{
			perror("Error reading buffer...\n");
		}
		// Check if the requested URL is "/metrics"
		if (strstr(buffer, "GET /metrics ") != NULL)
		{
			// Respond with metrics data
			char response[BUFFER_SIZE];
			sprintf(response, "HTTP/1.1 200 OK\r\n"
							"Content-Type: text/html; charset=UTF-8\r\n\r\n"
							"Upload Speed: %.0f\r\n"
							"Download Speed: %.0f\r\n",
					upload_speed, download_speed);
			write(new_socket, response, strlen(response));
		}
		else
		{
			// Respond with a generic response for other URLs
			char *response = "HTTP/1.1 404 Not Found\r\n"
							"Content-Type: text/html; charset=UTF-8\r\n\r\n"
							"<img style='display: block;-webkit-user-select: none;margin: auto;cursor: zoom-in;background-color: hsl(0, 0%, 90%);transition: background-color 300ms;' src='http://one.sce/metrics' width='894' height='554'>";
			write(new_socket, response, strlen(response));
		}
		close(new_socket);
	}
}

int main()
{
	pthread_t thread_id;
	printf("Starting Thread\n");
	pthread_create(&thread_id, NULL, scrape, NULL);
	struct Server server = server_Constructor(AF_INET, 80, SOCK_STREAM, 0, 10, INADDR_ANY, launch);
	server.launch(&server);
	pthread_join(thread_id, NULL);
	return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <gpiod.h>
#include <curl/curl.h>

#define CHIP_NAME 	"gpiochip0"
#define GPIO_LINE 	4 // BCM GPIO4
#define SHELLY_HOST 	"shelly"
#define TIMEOUT_SECONDS (60 * 10)

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
	return size * nmemb;
}

bool set_shelly_power(const char* ip, bool on) {
	CURL *curl = curl_easy_init();
	if (!curl)
		return false;

	char url[128];
	snprintf(url, sizeof(url), "http://%s/rpc/Switch.Set", ip);

	char json_payload[64];
	snprintf(json_payload, sizeof(json_payload), "{\"id\":0,\"on\":%s}", on ? "true" : "false");

	struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	CURLcode res = curl_easy_perform(curl);
	bool success = (res == CURLE_OK);

	if (!success) {
		fprintf(stderr, "Fehler beim Schalten des Shelly: %s\n", curl_easy_strerror(res));
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return success;
}

int main() {
	struct gpiod_chip *chip;
	struct gpiod_line *line;
	struct gpiod_line_event event;
	struct timespec timeout = {0};
	timeout.tv_sec = 1;

	chip = gpiod_chip_open_by_name(CHIP_NAME);
	if (!chip) {
		perror("gpiod_chip_open_by_name");
		return EXIT_FAILURE;
	}

	line = gpiod_chip_get_line(chip, GPIO_LINE);
	if (!line) {
		perror("gpiod_chip_get_line");
		gpiod_chip_close(chip);
		return EXIT_FAILURE;
	}

	if (gpiod_line_request_both_edges_events(line, "event-wait") < 0) {
		perror("gpiod_line_request_both_edges_events");
		gpiod_chip_close(chip);
		return EXIT_FAILURE;
	}

	time_t last_high_time = 0;

	while (1) {
		int ret = gpiod_line_event_wait(line, &timeout);
		int value = 0;

		if (ret == 1) {
			if (gpiod_line_event_read(line, &event) < 0) {
				perror("gpiod_line_event_read");
			}
			else {
				value = (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE);
			}
		}

		time_t now = time(NULL);

		if (value) {
			last_high_time = now;
			set_shelly_power(SHELLY_HOST, false);
		} else {
			if ((now - last_high_time) >= TIMEOUT_SECONDS) {
				set_shelly_power(SHELLY_HOST, true);
			}
		}
	}

	gpiod_line_release(line);
	gpiod_chip_close(chip);
	return 0;
}

/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define AERA_MAGIC 0x41325049U
#define AERA_API 2U
#define AERA_PRIMARY 1U

enum kind {
  HELLO = 1, BEGIN_PAGE, ADD_BUTTON, COMMIT_PAGE, SET_STATUS,
  REQUEST_OPERATION, CLOSE_WORKER,
  HELLO_ACK = 64, ACTION, LIFECYCLE, OPERATION_RESULT
};
enum operation { START_USB_MIRROR = 5, STOP_MIRROR = 6, START_WIFI_MIRROR = 7 };

struct message {
  uint32_t magic, version, kind, request_id, value, flags;
  char title[96];
  char text[1024];
};

static int send_message(int fd, uint32_t kind, uint32_t request,
                        uint32_t value, uint32_t flags,
                        const char *title, const char *text) {
  struct message message = {AERA_MAGIC, AERA_API, kind, request, value, flags,
                            {0}, {0}};
  if (title) snprintf(message.title, sizeof(message.title), "%s", title);
  if (text) snprintf(message.text, sizeof(message.text), "%s", text);
  ssize_t sent;
  do {
    sent = send(fd, &message, sizeof(message), MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  return sent == (ssize_t)sizeof(message) ? 0 : -1;
}

static int valid(const struct message *message) {
  return message->magic == AERA_MAGIC && message->version == AERA_API &&
         memchr(message->title, 0, sizeof(message->title)) &&
         memchr(message->text, 0, sizeof(message->text));
}

static int publish_page(int fd) {
  return send_message(fd, BEGIN_PAGE, 0, 0, 0, "AERA Mirror",
      "Choose Wi-Fi for a zero-install browser connection, or USB for the "
      "fast desktop client. Wi-Fi requires both devices on the same network.") ||
    send_message(fd, ADD_BUTTON, 1, 0, AERA_PRIMARY,
                 "Start Wi-Fi Mirror", "Enter the shown phone IP in any browser") ||
    send_message(fd, ADD_BUTTON, 2, 0, 0,
                 "Start USB Mirror", "Use the AERA Mirror desktop client") ||
    send_message(fd, ADD_BUTTON, 3, 0, 0,
                 "Stop AERA Mirror", "Close the browser server and USB stream") ||
    send_message(fd, COMMIT_PAGE, 0, 0, 0, 0, 0);
}

int main(void) {
  const int fd = 4;
  if (send_message(fd, HELLO, 0, AERA_API, AERA_API, 0, "AERA Mirror"))
    return 78;
  uint32_t operation_request = 100;
  int page_published = 0;
  for (;;) {
    struct message message;
    ssize_t count;
    do {
      count = recv(fd, &message, sizeof(message), MSG_TRUNC);
    } while (count < 0 && errno == EINTR);
    if (count != (ssize_t)sizeof(message) || !valid(&message)) return 78;
    if (message.kind == HELLO_ACK) continue;
    if (message.kind == LIFECYCLE) {
      if (message.value == 3) return 0;
      if (message.value == 1 && !page_published) {
        if (publish_page(fd)) return 78;
        page_published = 1;
      }
      continue;
    }
    if (message.kind == ACTION && message.request_id >= 1 &&
        message.request_id <= 3) {
      const uint32_t operation = message.request_id == 1 ? START_WIFI_MIRROR :
          message.request_id == 2 ? START_USB_MIRROR : STOP_MIRROR;
      if (send_message(fd, REQUEST_OPERATION, ++operation_request, operation,
                       0, 0, 0)) return 78;
      continue;
    }
    if (message.kind == OPERATION_RESULT) {
      if (send_message(fd, SET_STATUS, 0, 0, 0, 0, message.text)) return 78;
      continue;
    }
    if (message.kind == CLOSE_WORKER) return 0;
  }
}

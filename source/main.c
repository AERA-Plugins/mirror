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

enum action {
  SHOW_WIFI_GUIDE = 1,
  SHOW_USB_GUIDE = 2,
  STOP_ALL = 3,
  START_WIFI = 4,
  START_USB = 5,
  SHOW_HOME = 6
};

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

static int publish_home(int fd, const char *notice) {
  char body[1024];
  snprintf(body, sizeof(body),
      "Mirror this recovery to a computer, tablet, or another phone and "
      "control it from a browser.%s%s\n\nChoose how you want to connect.",
      notice && notice[0] ? "\n\n" : "", notice && notice[0] ? notice : "");
  return send_message(fd, BEGIN_PAGE, 0, 0, 0, "AERA Mirror",
      body) ||
    send_message(fd, ADD_BUTTON, SHOW_WIFI_GUIDE, 0, AERA_PRIMARY,
                 "Connect over Wi-Fi",
                 "No download - both devices use the same Wi-Fi network") ||
    send_message(fd, ADD_BUTTON, SHOW_USB_GUIDE, 0, 0,
                 "Connect over USB",
                 "Fast and private - requires ADB and the desktop launcher") ||
    send_message(fd, ADD_BUTTON, STOP_ALL, 0, 0,
                 "Stop mirroring", "Close every active mirror connection") ||
    send_message(fd, COMMIT_PAGE, 0, 0, 0, 0, 0);
}

static int publish_wifi_guide(int fd, const char *result, int success) {
  char body[1024];
  if (result && result[0]) {
    snprintf(body, sizeof(body),
        "%s\n\n%s\n\n1. Keep AERA connected to Wi-Fi.\n"
        "2. On the other device, open Chrome, Firefox, Safari, or Edge.\n"
        "3. Type the exact http:// address shown above.\n"
        "4. Use the browser window to view and control recovery.",
        success ? "Wi-Fi Mirror is ready." : "Wi-Fi Mirror could not start.",
        result);
  } else {
    snprintf(body, sizeof(body),
        "Nothing needs to be installed.\n\n"
        "1. Connect AERA to Wi-Fi from Quick Settings or Menu > Wi-Fi.\n"
        "2. Connect the viewing device to the same Wi-Fi network.\n"
        "3. Tap Start below and approve the request.\n"
        "4. AERA will show its exact http:// address. Open it in any browser.");
  }
  return send_message(fd, BEGIN_PAGE, 0, 0, 0,
                      result && result[0] ? "Wi-Fi connection" : "Wi-Fi setup",
                      body) ||
    send_message(fd, ADD_BUTTON, START_WIFI, 0, AERA_PRIMARY,
                 success ? "Restart Wi-Fi Mirror" : "Start Wi-Fi Mirror",
                 "AERA will display the exact browser address") ||
    send_message(fd, ADD_BUTTON, SHOW_HOME, 0, 0,
                 "Choose another connection", "Return to Wi-Fi or USB selection") ||
    send_message(fd, ADD_BUTTON, STOP_ALL, 0, 0,
                 "Stop mirroring", "Close every active mirror connection") ||
    send_message(fd, COMMIT_PAGE, 0, 0, 0, 0, 0);
}

static int publish_usb_guide(int fd, const char *result, int success) {
  char body[1024];
  snprintf(body, sizeof(body),
      "%s%s%s"
      "1. On the computer, download the USB launchers from:\n"
      "github.com/AERA-Plugins/mirror/releases/latest\n"
      "2. Install Android platform-tools (ADB) and connect the USB cable.\n"
      "3. Tap Start below and approve the request.\n"
      "4. Run the .cmd on Windows, .sh on Linux, or .command on macOS.\n"
      "5. The launcher opens http://127.0.0.1:8080/ automatically.\n\n"
      "Python, Pillow, and Tk are not required.",
      result && result[0] ? (success ? "USB Mirror is ready.\n\n" :
                             "USB Mirror could not start.\n\n") : "",
      result && result[0] ? result : "",
      result && result[0] ? "\n\n" : "");
  return send_message(fd, BEGIN_PAGE, 0, 0, 0,
                      result && result[0] ? "USB connection" : "USB setup",
                      body) ||
    send_message(fd, ADD_BUTTON, START_USB, 0, AERA_PRIMARY,
                 success ? "Restart USB Mirror" : "Start USB Mirror",
                 "Keep the cable attached, then run the desktop launcher") ||
    send_message(fd, ADD_BUTTON, SHOW_HOME, 0, 0,
                 "Choose another connection", "Return to Wi-Fi or USB selection") ||
    send_message(fd, ADD_BUTTON, STOP_ALL, 0, 0,
                 "Stop mirroring", "Close every active mirror connection") ||
    send_message(fd, COMMIT_PAGE, 0, 0, 0, 0, 0);
}

int main(void) {
  const int fd = 4;
  if (send_message(fd, HELLO, 0, AERA_API, AERA_API, 0, "AERA Mirror"))
    return 78;
  uint32_t operation_request = 100;
  uint32_t pending_operation = 0;
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
        if (publish_home(fd, 0)) return 78;
        page_published = 1;
      }
      continue;
    }
    if (message.kind == ACTION) {
      if (message.request_id == SHOW_WIFI_GUIDE) {
        if (publish_wifi_guide(fd, 0, 0)) return 78;
      } else if (message.request_id == SHOW_USB_GUIDE) {
        if (publish_usb_guide(fd, 0, 0)) return 78;
      } else if (message.request_id == SHOW_HOME) {
        if (publish_home(fd, 0)) return 78;
      } else if (message.request_id == START_WIFI ||
                 message.request_id == START_USB ||
                 message.request_id == STOP_ALL) {
        pending_operation = message.request_id == START_WIFI ? START_WIFI_MIRROR :
            message.request_id == START_USB ? START_USB_MIRROR : STOP_MIRROR;
        if (send_message(fd, REQUEST_OPERATION, ++operation_request,
                         pending_operation, 0, 0, 0)) return 78;
      }
      continue;
    }
    if (message.kind == OPERATION_RESULT) {
      const int success = message.value != 0;
      if (send_message(fd, SET_STATUS, 0, 0, 0, 0, message.text)) return 78;
      if (pending_operation == START_WIFI_MIRROR) {
        if (publish_wifi_guide(fd, message.text, success)) return 78;
      } else if (pending_operation == START_USB_MIRROR) {
        if (publish_usb_guide(fd, message.text, success)) return 78;
      } else if (pending_operation == STOP_MIRROR) {
        if (publish_home(fd, message.text)) return 78;
      }
      pending_operation = 0;
      continue;
    }
    if (message.kind == CLOSE_WORKER) return 0;
  }
}

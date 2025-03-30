#include <uv.h>

static uv_fs_event_t handle;

static void fs_event_cb(uv_fs_event_t *event, const char *filename, int events,
                        int status) {
    fprintf(stderr, "%s %d %d\n", filename, events, status);
    uv_close((uv_handle_t *)&handle, NULL);
}

int main() {
    uv_loop_t loop;
    uv_loop_init(&loop);
    uv_fs_event_init(&loop, &handle);
    uv_fs_event_start(&handle, fs_event_cb, ".", UV_FS_EVENT_RECURSIVE);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);
    return 0;
}

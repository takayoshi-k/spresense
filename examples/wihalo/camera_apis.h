#ifndef __CAMERA_APIS_____H
#define __CAMERA_APIS_____H

#include <nuttx/video/video.h>

int initialize_voide_device(void);
int get_camimage(int fd, FAR struct v4l2_buffer *v4l2_buf);
int release_camimage(int fd, FAR struct v4l2_buffer *v4l2_buf);
void finalize_voide_device(int v_fd);

#endif // __CAMERA_APIS_____H

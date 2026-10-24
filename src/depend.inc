bcm_host_compat.o: bcm_host_compat.c compat/bcm_host.h
main.o: main.c config.h base.h pj.h
pj.o: pj.c config.h base.h pj.h graphics.h
video.o: video.c compat/bcm_host.h config.h base.h video.h
video_egl.o: video_egl.c compat/bcm_host.h config.h base.h video_egl.h
graphics.o: graphics.c compat/bcm_host.h config.h base.h video.h \
 video_egl.h graphics.h

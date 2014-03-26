/**
 ** gles1-athene
 ** Windowed athene that uses OpenGL ES 1.X for rendering API.
 **
 ** Features:
 **  - configurable window size through the -size=[width]x[height] option
 **  - adjustable swap interval through the -interval=[n] option;
 **    a swap interval of 0 lets the app run as fast as possible
 **    numbers of 1 or more limit the rate to the number of vsync periods
 **  - application creates itself on the default EGL display
 **  - configurable pixel format
 **  - application exits if power or mode event invalidates EGL surface
 **
 ** Copyright 2010, QNX Software Systems Ltd. All Rights Reserved
 **
 ** Permission to use, copy, modify, and distribute this software and its
 ** documentation for any purpose and without fee is hereby granted,
 ** provided that the above copyright notice appear in all copies and that
 ** both that copyright notice and this permission notice appear in
 ** supporting documentation.
 **
 ** This file is provided AS IS with no warranties of any kind.  The author
 ** shall have no liability with respect to the infringement of copyrights,
 ** trade secrets or any patents by this file or any part thereof.  In no
 ** event will the author be liable for any lost revenue or profits or
 ** other special, indirect and consequential damages.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/keycodes.h>
#include <time.h>

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <screen/screen.h>

#include "error.h"

extern void app_init(int width, int height);

extern void app_draw(void);

struct {
	EGLint surface_type;
	EGLint red_size;
	EGLint green_size;
	EGLint blue_size;
	EGLint alpha_size;
	EGLint samples;
	EGLint config_id;
} egl_conf_attr = {
	.surface_type = EGL_WINDOW_BIT,
	.red_size = EGL_DONT_CARE,
	.green_size = EGL_DONT_CARE,
	.blue_size = EGL_DONT_CARE,
	.alpha_size = EGL_DONT_CARE,
	.samples = EGL_DONT_CARE,
	.config_id = EGL_DONT_CARE,
};

EGLConfig choose_config(EGLDisplay egl_disp, const char* str)
{
	EGLConfig egl_conf = (EGLConfig)0;
	EGLConfig *egl_configs;
	EGLint egl_num_configs;
	EGLint val;
	EGLBoolean rc;
	const char *tok;
	EGLint i;

	if (str != NULL) {
		tok = str;
		while (*tok == ' ' || *tok == ',') {
			tok++;
		}

		while (*tok != '\0') {
			if (strncmp(tok, "rgba8888", strlen("rgba8888")) == 0) {
				egl_conf_attr.red_size = 8;
				egl_conf_attr.green_size = 8;
				egl_conf_attr.blue_size = 8;
				egl_conf_attr.alpha_size = 8;
				tok += strlen("rgba8888");
			} else if (strncmp(tok, "rgba5551", strlen("rgba5551")) == 0) {
				egl_conf_attr.red_size = 5;
				egl_conf_attr.green_size = 5;
				egl_conf_attr.blue_size = 5;
				egl_conf_attr.alpha_size = 1;
				tok += strlen("rgba5551");
			} else if (strncmp(tok, "rgba4444", strlen("rgba4444")) == 0) {
				egl_conf_attr.red_size = 4;
				egl_conf_attr.green_size = 4;
				egl_conf_attr.blue_size = 4;
				egl_conf_attr.alpha_size = 4;
				tok += strlen("rgba4444");
			} else if (strncmp(tok, "rgb565", strlen("rgb565")) == 0) {
				egl_conf_attr.red_size = 5;
				egl_conf_attr.green_size = 6;
				egl_conf_attr.blue_size = 5;
				egl_conf_attr.alpha_size = 0;
				tok += strlen("rgb565");
			} else if (isdigit(*tok)) {
				val = atoi(tok);
				while (isdigit(*(++tok)));
				if (*tok == 'x') {
					egl_conf_attr.samples = val;
					tok++;
				} else {
					egl_conf_attr.config_id = val;
				}
			} else {
				fprintf(stderr, "invalid configuration specifier: ");
				while (*tok != ' ' && *tok != ',' && *tok != '\0') {
					fputc(*tok++, stderr);
				}
				fputc('\n', stderr);
			}

			while (*tok == ' ' || *tok == ',') {
				tok++;
			}
		}
	}

	rc = eglGetConfigs(egl_disp, NULL, 0, &egl_num_configs);
	if (rc != EGL_TRUE) {
		egl_perror("eglGetConfigs");
		return egl_conf;
	}
	if (egl_num_configs == 0) {
		fprintf(stderr, "eglGetConfigs: could not find a configuration\n");
		return egl_conf;
	}

	egl_configs = malloc(egl_num_configs * sizeof(*egl_configs));
	if (egl_configs == NULL) {
		fprintf(stderr, "could not allocate memory for %d EGL configs\n", egl_num_configs);
		return egl_conf;
	}

	rc = eglGetConfigs(egl_disp, egl_configs,
		egl_num_configs, &egl_num_configs);
	if (rc != EGL_TRUE) {
		egl_perror("eglGetConfigs");
		free(egl_configs);
		return egl_conf;
	}

	for (i = 0; i < egl_num_configs; i++) {
		if (egl_conf_attr.config_id != EGL_DONT_CARE) {
			eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_CONFIG_ID, &val);
			if (val == egl_conf_attr.config_id) {
				egl_conf = egl_configs[i];
				break;
			} else {
				continue;
			}
		}

		eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_SURFACE_TYPE, &val);
		if ((val & egl_conf_attr.surface_type) != egl_conf_attr.surface_type) {
			continue;
		}

		eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_RENDERABLE_TYPE, &val);
		if (!(val & EGL_OPENGL_ES_BIT)) {
			continue;
		}

		eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_DEPTH_SIZE, &val);
		if (val == 0) {
			continue;
		}

		if (egl_conf_attr.red_size != EGL_DONT_CARE) {
			eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_RED_SIZE, &val);
			if (val != egl_conf_attr.red_size) {
				continue;
			}
		}
		if (egl_conf_attr.green_size != EGL_DONT_CARE) {
			eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_GREEN_SIZE, &val);
			if (val != egl_conf_attr.green_size) {
				continue;
			}
		}
		if (egl_conf_attr.blue_size != EGL_DONT_CARE) {
			eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_BLUE_SIZE, &val);
			if (val != egl_conf_attr.blue_size) {
				continue;
			}
		}
		if (egl_conf_attr.alpha_size != EGL_DONT_CARE) {
			eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_ALPHA_SIZE, &val);
			if (val != egl_conf_attr.alpha_size) {
				continue;
			}
		}
		if (egl_conf_attr.samples != EGL_DONT_CARE) {
			eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_SAMPLES, &val);
			if (val != egl_conf_attr.samples) {
				continue;
			}
		}

		egl_conf = egl_configs[i];
		break;
	}

	free(egl_configs);

	if (egl_conf == (EGLConfig)0) {
		fprintf(stderr, "eglChooseConfig: could not find a matching configuration\n");
	}

	return egl_conf;
}

int choose_format(EGLDisplay egl_disp, EGLConfig egl_conf)
{
	EGLint buffer_bit_depth, alpha_bit_depth;

	eglGetConfigAttrib(egl_disp, egl_conf, EGL_BUFFER_SIZE, &buffer_bit_depth);
	eglGetConfigAttrib(egl_disp, egl_conf, EGL_ALPHA_SIZE, &alpha_bit_depth);
	switch (buffer_bit_depth) {
	case 32: {
		return SCREEN_FORMAT_RGBA8888;
	}
	case 24: {
		return SCREEN_FORMAT_RGB888;
	}
	case 16: {
		switch (alpha_bit_depth) {
		case 4: {
			return SCREEN_FORMAT_RGBA4444;
		}
		case 1: {
			return SCREEN_FORMAT_RGBA5551;
		}
		default: {
			return SCREEN_FORMAT_RGB565;
		}
		}
	}
	default: {
		return 0;
	}
	}
}

int main(int argc, char *argv[])
{
	const int exit_area_size = 20;

	struct {
		EGLint width[2];
		EGLint height[2];
		EGLint none;
	} egl_pbuf_attr = {
		.width = { EGL_WIDTH, 256 },
		.height = { EGL_HEIGHT, 256 },
		.none = EGL_NONE
	};

	screen_context_t screen_ctx;
	screen_window_t screen_win;
	screen_event_t screen_ev;
	int usage = SCREEN_USAGE_OPENGL_ES1;
	int transp = SCREEN_TRANSPARENCY_NONE;
	int size[2] = { -1, -1 };
	int pos[2] = { 0, 0 };
	int nbuffers = 2;
	int format;
	int val;
	EGLNativeDisplayType disp_id = EGL_DEFAULT_DISPLAY;
	EGLDisplay egl_disp;
	EGLConfig egl_conf;
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLint interval = 1;
	EGLBoolean verbose = EGL_FALSE;
	const char *conf_str = NULL;
	const char *tok;
	int rval = EXIT_FAILURE;
	int pause = 0;
	int i, rc;
	struct timespec to;
	uint64_t t, last_t, delta;
	int frames = 0;
	screen_display_t *screen_disp;	/* native handle for our display */
	int screen_ndisplays;		/* number of displays */
	const char *display = NULL;	/* the display to create our window on */
	int screen_val;
	int pipeline = -1;
	int zorder = 0;

	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "-config=", strlen("-config=")) == 0) {
			conf_str = argv[i] + strlen("-config=");
		} else if (strncmp(argv[i], "-size=", strlen("-size=")) == 0) {
			tok = argv[i] + strlen("-size=");
			size[0] = egl_pbuf_attr.width[1] = atoi(tok);
			while (*tok >= '0' && *tok <= '9') {
				tok++;
			}
			size[1] = egl_pbuf_attr.height[1] = atoi(tok+1);
		} else if (strncmp(argv[i], "-pos=", strlen("-pos=")) == 0) {
			tok = argv[i] + strlen("-pos=");
			pos[0] = atoi(tok);
			while (*tok >= '0' && *tok <= '9') {
				tok++;
			}
			pos[1] = atoi(tok+1);
		} else if (strncmp(argv[i], "-interval=", strlen("-interval=")) == 0) {
			interval = atoi(argv[i] + strlen("-interval="));
		} else if (strncmp(argv[i], "-display=", strlen("-display=")) == 0) {
			display = argv[i] + strlen("-display=");
		} else if (strcmp(argv[i], "-single-buffer") == 0) {
			nbuffers = 1;
		} else if (strcmp(argv[i], "-double-buffer") == 0) {
			nbuffers = 2;
		} else if (strcmp(argv[i], "-pbuffer") == 0) {
			egl_conf_attr.surface_type = EGL_PBUFFER_BIT;
		} else if (strcmp(argv[i], "-verbose") == 0) {
			verbose = EGL_TRUE;
		} else if (strncmp(argv[i], "-pipeline=", strlen("-pipeline=")) == 0) {
			tok = argv[i] + strlen("-pipeline=");
			pipeline = atoi(tok);
		} else if (strncmp(argv[i], "-zorder=", strlen("-zorder=")) == 0) {
			tok = argv[i] + strlen("-zorder=");
			zorder = atoi(tok);
		} else {
			fprintf(stderr, "invalid command line option: %s\n", argv[i]);
		}
	}

	egl_disp = eglGetDisplay(disp_id);
	if (egl_disp == EGL_NO_DISPLAY) {
		egl_perror("eglGetDisplay");
		goto fail1;
	}

	rc = eglInitialize(egl_disp, NULL, NULL);
	if (rc != EGL_TRUE) {
		egl_perror("eglInitialize");
		goto fail2;
	}

	egl_conf = choose_config(egl_disp, conf_str);
	if (egl_conf == (EGLConfig)0) {
		goto fail2;
	}

	egl_ctx = eglCreateContext(egl_disp, egl_conf, EGL_NO_CONTEXT, NULL);
	if (egl_ctx == EGL_NO_CONTEXT) {
		egl_perror("eglCreateContext");
		goto fail2;
	}

	if (verbose) {
		printf("EGL_VENDOR = %s\n", eglQueryString(egl_disp, EGL_VENDOR));
		printf("EGL_VERSION = %s\n", eglQueryString(egl_disp, EGL_VERSION));
		printf("EGL_CLIENT_APIS = %s\n", eglQueryString(egl_disp, EGL_CLIENT_APIS));
		printf("EGL_EXTENSIONS = %s\n\n", eglQueryString(egl_disp, EGL_EXTENSIONS));

		i = -1;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_CONFIG_ID, &i);
		printf("EGL_CONFIG_ID = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_RED_SIZE, &i);
		printf("EGL_RED_SIZE = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_GREEN_SIZE, &i);
		printf("EGL_GREEN_SIZE = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_BLUE_SIZE, &i);
		printf("EGL_BLUE_SIZE = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_ALPHA_SIZE, &i);
		printf("EGL_ALPHA_SIZE = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_DEPTH_SIZE, &i);
		printf("EGL_DEPTH_SIZE = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_LEVEL, &i);
		printf("EGL_LEVEL = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_NATIVE_RENDERABLE, &i);
		printf("EGL_NATIVE_RENDERABLE = %s\n", i ? "EGL_TRUE" : "EGL_FALSE");

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_NATIVE_VISUAL_TYPE, &i);
		printf("EGL_NATIVE_VISUAL_TYPE = %d\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_RENDERABLE_TYPE, &i);
		printf("EGL_RENDERABLE_TYPE = 0x%04x\n", i);

		i = 0;
		eglGetConfigAttrib(egl_disp, egl_conf, EGL_SURFACE_TYPE, &i);
		printf("EGL_SURFACE_TYPE = 0x%04x\n", i);
	}

	if (egl_conf_attr.surface_type != EGL_PBUFFER_BIT) {
		rc = screen_create_context(&screen_ctx, 0);
		if (rc) {
			perror("screen_create_context");
			goto fail3;
		}

		rc = screen_create_window(&screen_win, screen_ctx);
		if (rc) {
			perror("screen_create_window");
			goto fail4;
		}

		if (display) {
			rc = screen_get_context_property_iv(screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT, 
				&screen_ndisplays);
			if (rc) {
				perror("screen_get_context_property_iv(SCREEN_PROPERTY_DISPLAY_COUNT)");
				goto fail5;
			}

			screen_disp = calloc(screen_ndisplays, sizeof(*screen_disp));
			if (screen_disp == NULL) {
				fprintf(stderr, "could not allocate memory for display list\n");
				goto fail5;
			}

			rc = screen_get_context_property_pv(screen_ctx, SCREEN_PROPERTY_DISPLAYS, 
				(void **)screen_disp);
			if (rc) {
				perror("screen_get_context_property_ptr(SCREEN_PROPERTY_DISPLAYS)");
				free(screen_disp);
				goto fail5;
			}

			if (isdigit(*display)) {
				int want_id = atoi(display);
				for (i = 0; i < screen_ndisplays; ++i) {
					int actual_id = 0;  // invalid
					(void)screen_get_display_property_iv(screen_disp[i],
							SCREEN_PROPERTY_ID, &actual_id);
					if (want_id == actual_id) {
						break;
					}
				}
			} else {
				int type = -1;
				if (strcmp(display, "internal") == 0) {
					type = SCREEN_DISPLAY_TYPE_INTERNAL;
				} else if (strcmp(display, "composite") == 0) {
					type = SCREEN_DISPLAY_TYPE_COMPOSITE;
				} else if (strcmp(display, "svideo") == 0) {
					type = SCREEN_DISPLAY_TYPE_SVIDEO;
				} else if (strcmp(display, "YPbPr") == 0) {
					type = SCREEN_DISPLAY_TYPE_COMPONENT_YPbPr;
				} else if (strcmp(display, "rgb") == 0) {
					type = SCREEN_DISPLAY_TYPE_COMPONENT_RGB;
				} else if (strcmp(display, "rgbhv") == 0) {
					type = SCREEN_DISPLAY_TYPE_COMPONENT_RGBHV;
				} else if (strcmp(display, "dvi") == 0) {
					type = SCREEN_DISPLAY_TYPE_DVI;
				} else if (strcmp(display, "hdmi") == 0) {
					type = SCREEN_DISPLAY_TYPE_HDMI;
				} else if (strcmp(display, "other") == 0) {
					type = SCREEN_DISPLAY_TYPE_OTHER;
				} else {
					fprintf(stderr, "unknown display type %s\n", display);
					free(screen_disp);
					goto fail5;
				}
				for (i = 0; i < screen_ndisplays; i++) {
					screen_get_display_property_iv(screen_disp[i], 
						SCREEN_PROPERTY_TYPE, &screen_val);
					if (screen_val == type) {
						break;
					}
				}
			}
			if (i >= screen_ndisplays) {
				fprintf(stderr, "couldn't find display %s\n", display);
				free(screen_disp);
				goto fail5;
			}

			rc = screen_set_window_property_pv(screen_win, SCREEN_PROPERTY_DISPLAY, 
			(void **)&screen_disp[i]);
				if (rc) {
				perror("screen_set_window_property_ptr(SCREEN_PROPERTY_DISPLAY)");
				free(screen_disp);
				goto fail5;
			}

			free(screen_disp);
		}

		rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &zorder);
		if (rc) {
			perror("screen_set_window_property_iv(SCREEN_PROPERTY_ZORDER)");
			goto fail5;
		}

		format = choose_format(egl_disp, egl_conf);
		rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format);
		if (rc) {
			perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
			goto fail5;
		}

		if (pipeline != -1) {
			usage |= SCREEN_USAGE_OVERLAY;
		}
		rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);
		if (rc) {
			perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
			goto fail5;
		}

		if (size[0] > 0 && size[1] > 0) {
			rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SIZE, size);
			if (rc) {
				perror("screen_set_window_property_iv(SCREEN_PROPERTY_SIZE)");
				goto fail5;
			}
		} else {
			rc = screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_SIZE, size);
			if (rc) {
				perror("screen_get_window_property_iv(SCREEN_PROPERTY_SIZE)");
				goto fail5;
			}
		}

		if (pos[0] != 0 || pos[1] != 0) {
			rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_POSITION, pos);
			if (rc) {
				perror("screen_set_window_property_iv(SCREEN_PROPERTY_POSITION)");
				goto fail5;
			}
		}

		rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_TRANSPARENCY, &transp);
		if (rc) {
			perror("screen_set_window_property_iv(SCREEN_PROPERTY_TRANSPARENCY)");
			goto fail5;
		}

		if (pipeline != -1) {
			rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_PIPELINE, &pipeline);
			if (rc) {
				perror("screen_set_window_property_iv(SCREEN_PROPERTY_PIPELINE)");
				goto fail5;
			}
		}

		// Set ID string for debugging via /dev/screen.
		{
			const char *idstr = "gles1-gears";
			screen_set_window_property_cv(screen_win,
					SCREEN_PROPERTY_ID_STRING, strlen(idstr), idstr);
		}

		rc = screen_create_window_buffers(screen_win, nbuffers);
		if (rc) {
			perror("screen_create_window_buffers");
			goto fail5;
		}

		egl_surf = eglCreateWindowSurface(egl_disp, egl_conf, screen_win, NULL);
		if (egl_surf == EGL_NO_SURFACE) {
			egl_perror("eglCreateWindowSurface");
			goto fail5;
		}
	} else {
		egl_surf = eglCreatePbufferSurface(egl_disp, egl_conf, (EGLint*)&egl_pbuf_attr);
		if (egl_surf == EGL_NO_SURFACE) {
			egl_perror("eglCreatePbufferSurface");
			goto fail3;
		}
	}

	rc = eglMakeCurrent(egl_disp, egl_surf, egl_surf, egl_ctx);
	if (rc != EGL_TRUE) {
		egl_perror("eglMakeCurrent");
		goto fail6;
	}

	if (interval != 1 && egl_conf_attr.surface_type != EGL_PBUFFER_BIT) {
		rc = eglSwapInterval(egl_disp, interval);
		if (rc != EGL_TRUE) {
			egl_perror("eglSwapInterval");
			goto fail7;
		}
	}

	rc = screen_create_event(&screen_ev);
	if (rc) {
		perror("screen_create_event");
		goto fail7;
	}

	if (verbose) {
		printf("GL_VENDOR = %s\n", (char *)glGetString(GL_VENDOR));
		printf("GL_RENDERER = %s\n", (char *)glGetString(GL_RENDERER));
		printf("GL_VERSION = %s\n", (char *)glGetString(GL_VERSION));
		printf("GL_EXTENSIONS = %s\n", (char *)glGetString(GL_EXTENSIONS));
	}

	glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
	app_init(size[0], size[1]);

	clock_gettime(CLOCK_REALTIME, &to);
	last_t = timespec2nsec(&to);

	while (1) {
		while (!screen_get_event(screen_ctx, screen_ev, pause ? ~0 : 0)) {
			rc = screen_get_event_property_iv(screen_ev, SCREEN_PROPERTY_TYPE, &val);
			if (rc || val == SCREEN_EVENT_NONE) {
				break;
			}
			switch (val) {
				case SCREEN_EVENT_CLOSE:
					goto end;
				case SCREEN_EVENT_POINTER:
					screen_get_event_property_iv(screen_ev, SCREEN_PROPERTY_BUTTONS, &val);
					if (val) {
						screen_get_event_property_iv(screen_ev, SCREEN_PROPERTY_SOURCE_POSITION, pos);
						if (pos[0] >= size[0] - exit_area_size &&
							pos[0] < size[0] &&
							pos[1] >= 0 &&
							pos[1] < exit_area_size) {
							goto end;
						}
					}
					break;
				case SCREEN_EVENT_KEYBOARD:
					screen_get_event_property_iv(screen_ev, SCREEN_PROPERTY_KEY_FLAGS, &val);
					if (val & KEY_DOWN) {
						screen_get_event_property_iv(screen_ev, SCREEN_PROPERTY_KEY_SYM, &val);
						switch (val) {
							case KEYCODE_ESCAPE:
								goto end;
							case KEYCODE_F:
								pause = !pause;
								break;
							default:
								break;
						}
					}
					break;
			}
		}

		if (pause) {
			continue;
		}

		app_draw();

		if (egl_conf_attr.surface_type == EGL_WINDOW_BIT) {
			rc = eglSwapBuffers(egl_disp, egl_surf);
			if (rc != EGL_TRUE) {
				egl_perror("eglSwapBuffers");
				break;
			}
		} else {
			glFinish();
		}

		frames++;

		clock_gettime(CLOCK_REALTIME, &to);
		t = timespec2nsec(&to);
		delta = t - last_t;
		if (delta >= 5000000000LL) {
			printf("%d frames i %6.3f seconds = %6.3f FPS\n",
				frames, 0.000000001f * delta, 1000000000.0f * frames / delta);
			last_t = t;
			frames = 0;
		}
	}
end:
	rval = EXIT_SUCCESS;
	screen_destroy_event(screen_ev);
fail7:
	eglMakeCurrent(egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
fail6:
	eglDestroySurface(egl_disp, egl_surf);
fail5:
	if (egl_conf_attr.surface_type != EGL_PBUFFER_BIT) {
		screen_destroy_window(screen_win);
	}
fail4:
	if (egl_conf_attr.surface_type != EGL_PBUFFER_BIT) {
		screen_destroy_context(screen_ctx);
	}
fail3:
	eglDestroyContext(egl_disp, egl_ctx);
fail2:
	eglTerminate(egl_disp);
fail1:
	eglReleaseThread();
	return rval;
}

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif

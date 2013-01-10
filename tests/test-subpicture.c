/*
 *  test-subpicture.c - Test GstVaapiSubpicture
 *
 *  Copyright (C) <2011> Intel Corporation
 *  Copyright (C) <2011> Collabora Ltd.
 *  Copyright (C) <2011> Thibault Saunier <thibault.saunier@collabora.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "config.h"
#include <string.h>
#include <gst/vaapi/gstvaapisurface.h>
#include "decoder.h"
#include "output.h"
#include "test-subpicture-data.h"

static inline void pause(void)
{
    g_print("Press any key to continue...\n");
    getchar();
}

static gchar *g_codec_str;

static GOptionEntry g_options[] = {
    { "codec", 'c',
      0,
      G_OPTION_ARG_STRING, &g_codec_str,
      "codec to test", NULL },
    { NULL, }
};

static void
upload_subpicture(GstBuffer *buffer, const VideoSubpictureInfo *subinfo)
{
    guint32 * const dst = (guint32 *)GST_BUFFER_DATA(buffer);
    const guint32 * const src = subinfo->data;
    guint i, len = subinfo->data_size / 4;

    /* Convert from RGBA source to ARGB */
    for (i = 0; i < len; i++) {
        const guint32 rgba = src[i];
        dst[i] = (rgba >> 8) | (rgba << 24);
    }
}

int
main(int argc, char *argv[])
{
    GstVaapiDisplay      *display;
    GstVaapiWindow       *window;
    GstVaapiDecoder      *decoder;
    GstVaapiSurface      *surface;
    GstBuffer            *buffer;
    VideoSubpictureInfo   subinfo;
    GstVaapiRectangle     subrect;
    GstVideoOverlayRectangle *overlay;
    GstVideoOverlayComposition *compo;
    guint flags = 0;

    static const guint win_width  = 640;
    static const guint win_height = 480;

    if (!video_output_init(&argc, argv, g_options))
        g_error("failed to initialize video output subsystem");

    g_print("Test subpicture\n");

    display = video_output_create_display(NULL);
    if (!display)
        g_error("could not create VA display");

    window = video_output_create_window(display, win_width, win_height);
    if (!window)
        g_error("could not create window");

    decoder = decoder_new(display, g_codec_str);
    if (!decoder)
        g_error("could not create decoder");

    if (!decoder_put_buffers(decoder))
        g_error("could not fill decoder with sample data");

    surface = decoder_get_surface(decoder);
    if (!surface)
        g_error("could not get decoded surface");

    subpicture_get_info(&subinfo);
    buffer = gst_buffer_new_and_alloc(subinfo.data_size);
    upload_subpicture(buffer, &subinfo);

    /* We position the subpicture at the bottom center */
    subrect.x = (gst_vaapi_surface_get_width(surface) - subinfo.width) / 2;
    subrect.y = gst_vaapi_surface_get_height(surface) - subinfo.height - 10;
    subrect.height = subinfo.height;
    subrect.width = subinfo.width;

    overlay = gst_video_overlay_rectangle_new_argb(buffer,
        subinfo.width, subinfo.height, subinfo.width * 4,
        subrect.x, subrect.y, subrect.width, subrect.height, flags);
    if (!overlay)
        g_error("could not create video overlay");
    gst_buffer_unref(buffer);

    compo = gst_video_overlay_composition_new(overlay);
    if (!compo)
        g_error("could not create video overlay composition");
    gst_video_overlay_rectangle_unref(overlay);

    if (!gst_vaapi_surface_set_subpictures_from_composition(surface, compo,
            FALSE))
        g_error("could not create subpictures from video overlay compoition");

    gst_vaapi_window_show(window);

    if (!gst_vaapi_window_put_surface(window, surface, NULL, NULL,
            GST_VAAPI_PICTURE_STRUCTURE_FRAME))
        g_error("could not render surface");

    pause();

    gst_video_overlay_composition_unref(compo);
    g_object_unref(decoder);
    g_object_unref(window);
    g_object_unref(display);
    g_free(g_codec_str);
    video_output_exit();
    return 0;
}

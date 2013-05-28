/* output_gstreamer.c - Output module for GStreamer
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
 *
 * Adapted to gstreamer-0.10 2006 David Siorpaes
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gst/gst.h>

//#define ENABLE_TRACING

#include "logging.h"
#include "upnp_connmgr.h"
#include "output_gstreamer.h"
#include "upnp_transport.h"

static void scan_caps(const GstCaps * caps)
{
	guint i;

	g_return_if_fail(caps != NULL);

	if (gst_caps_is_any(caps)) {
		return;
	}
	if (gst_caps_is_empty(caps)) {
		return;
	}

	for (i = 0; i < gst_caps_get_size(caps); i++) {
		GstStructure *structure = gst_caps_get_structure(caps, i);
		register_mime_type(gst_structure_get_name(structure));
	}

}

static void scan_pad_templates_info(GstElement * element, GstElementFactory * factory)
{
	const GList *pads;
	GstPadTemplate *padtemplate;
	GstPad *pad;
	GstElementClass *class;

	class = GST_ELEMENT_GET_CLASS(element);

	if (!class->numpadtemplates) {
		return;
	}

	pads = class->padtemplates;
	while (pads) {
		padtemplate = (GstPadTemplate *) (pads->data);
		pad = (GstPad *) (pads->data);
		pads = g_list_next(pads);

		if ((padtemplate->direction == GST_PAD_SINK) &&
		    ((padtemplate->presence == GST_PAD_ALWAYS) ||
		     (padtemplate->presence == GST_PAD_SOMETIMES) ||
		     (padtemplate->presence == GST_PAD_REQUEST)) &&
		    (padtemplate->caps)) {
			scan_caps(padtemplate->caps);
		}
	}

}


static void scan_mime_list(void)
{
	GList *plugins;
	GstRegistry *registry = gst_registry_get_default();

	ENTER();

	plugins = gst_default_registry_get_plugin_list();

	while (plugins) {
		GList *features;
		GstPlugin *plugin;

		plugin = (GstPlugin *) (plugins->data);
		plugins = g_list_next(plugins);

		features = gst_registry_get_feature_list_by_plugin(registry, gst_plugin_get_name(plugin));

		while (features) {
			GstPluginFeature *feature;

			feature = GST_PLUGIN_FEATURE(features->data);

			if (GST_IS_ELEMENT_FACTORY(feature)) {
				GstElementFactory *factory;
				GstElement *element;
				factory = GST_ELEMENT_FACTORY(feature);
				element = gst_element_factory_create(factory, NULL);
				if (element) {
					scan_pad_templates_info(element, factory);
				}
			}

			features = g_list_next(features);
		}
	}

	LEAVE();
}


static GstElement *play;
static char *gsuri = NULL;

void output_set_uri(const char *uri)
{
	ENTER();
	deg("%s: setting uri to '%s'\n", __FUNCTION__, uri);
	if (gsuri != NULL)
	{
		free(gsuri);
	}
	gsuri = strdup(uri);
	LEAVE();
}

int output_play(void)
{
	int result = -1;
	ENTER();
	if (gst_element_set_state(play, GST_STATE_READY) ==
	    GST_STATE_CHANGE_FAILURE) {
		deg("setting play state failed\n");
                goto out;
	}
	g_object_set(G_OBJECT(play), "uri", gsuri, NULL);
	if (gst_element_set_state(play, GST_STATE_PLAYING) ==
	    GST_STATE_CHANGE_FAILURE) {
		deg("setting play state failed\n");
		goto out;
	} 
	result = 0;
out:
	LEAVE();
	return result;
}

int output_play_continue(void)
{
	int result = -1;
	ENTER();
	if (gst_element_set_state(play, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		deg("setting play state failed\n");
		goto out;
	} 
	result = 0;
out:
	LEAVE();
	return result;
}

int output_stop(void)
{
	if (gst_element_set_state(play, GST_STATE_READY) ==
	    GST_STATE_CHANGE_FAILURE) {
		return -1;
	} else {
		return 0;
	}

}

int output_pause(void)
{
	if (gst_element_set_state(play, GST_STATE_PAUSED) ==
	    GST_STATE_CHANGE_FAILURE) {
		return -1;
	} else {
		return 0;
	}

}

#define UPNP_TIME_FORMAT "%02u:%02u:%02u"
int output_position(char *time)
{
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos, len;

	if (gst_element_query_position (play, &fmt, &pos) && gst_element_query_duration (play, &fmt, &len)) {
		//deg ("Time: %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r", GST_TIME_ARGS (pos), GST_TIME_ARGS (len));
		sprintf(time, UPNP_TIME_FORMAT, GST_TIME_ARGS (pos));
	}

	/* call me again */
	return 0;
}

int output_seek(int time_seconds)
{
	if (!gst_element_seek (play, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
				GST_SEEK_TYPE_SET, GST_SECOND*time_seconds,
				GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		deg ("Seek failed!\n");
		return -1;
	}
	return 0;
}

int output_get_volume(gdouble *volume)
{
	g_object_get (G_OBJECT (play), "volume", volume, NULL);
//	deg("volume %f\n", *volume);
	return 0;
}

int output_set_volume(gdouble volume)
{
	g_object_set (G_OBJECT (play), "volume", volume, NULL);
	return 0;
}

gboolean output_get_mute(void)
{
	gboolean mute = 0;
	g_object_get (G_OBJECT (play), "mute", &mute, NULL);
	deg("mute %d\n", mute);
	return 0;
}

int output_set_mute(gboolean mute)
{
	g_object_set (G_OBJECT (play), "mute", mute, NULL);
	return 0;
}

int output_loop()
{
	GMainLoop *loop;

	/* Create a main loop that runs the default GLib main context */
	loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(loop);
	return 0;
}

static const char *gststate_get_name(GstState state)
{
	switch(state) {
	case GST_STATE_VOID_PENDING:
		return "VOID_PENDING";
	case GST_STATE_NULL:
		return "NULL";
	case GST_STATE_READY:
		return "READY";
	case GST_STATE_PAUSED:
		return "PAUSED";
	case GST_STATE_PLAYING:
		return "PLAYING";
	default:
		return "Unknown";
	}
}


static gboolean my_bus_callback(GstBus * bus, GstMessage * msg,
				gpointer data)
{
	//GMainLoop *loop = (GMainLoop *) data;
	GstMessageType msgType;
	GstObject *msgSrc;
	gchar *msgSrcName;
	gchar *debug;
	GError *err;
	GstState oldstate, newstate, pending;

	msgType = GST_MESSAGE_TYPE(msg);
	msgSrc = GST_MESSAGE_SRC(msg);
	msgSrcName = GST_OBJECT_NAME(msgSrc);

	switch (msgType) {
		case GST_MESSAGE_EOS:
//			g_print("GStreamer: %s: End-of-stream\n", msgSrcName);
			set_transport_state(TRANSPORT_STOPPED);
			break;
		case GST_MESSAGE_ERROR:

			gst_message_parse_error(msg, &err, &debug);
			g_free(debug);

//			g_print("GStreamer: %s: Error: %s\n", msgSrcName, err->message);
			g_error_free(err);

			break;

		case GST_MESSAGE_STATE_CHANGED:
			gst_message_parse_state_changed(msg, &oldstate, &newstate, &pending);
			/*g_print("GStreamer: %s: State change: OLD: '%s', NEW: '%s', PENDING: '%s'\n",
					msgSrcName,
					gststate_get_name(oldstate),
					gststate_get_name(newstate),
					gststate_get_name(pending));*/
			break;

		case GST_MESSAGE_BUFFERING:
			break;

		default:
			/*g_print("GStreamer: %s: unhandled message type 0x%x (%s)\n",
					msgSrcName, msgType, gst_message_type_get_name(msgType));*/
			break;
	}

	return TRUE;
}

static gchar *audiosink = NULL;
static gchar *videosink = NULL;

/* Options specific to output_gstreamer */
static GOptionEntry option_entries[] = {
        { "gstout-audiosink", 0, 0, G_OPTION_ARG_STRING, &audiosink,
          "GStreamer audio sink to use "
	  "(autoaudiosink, alsasink, osssink, esdsink, ...)",
	  NULL },
        { "gstout-videosink", 0, 0, G_OPTION_ARG_STRING, &videosink,
          "GStreamer video sink to use "
	  "(autovideosink, xvimagesink, ximagesink, ...)",
	  NULL },
        { NULL }
};


int output_gstreamer_add_options(GOptionContext *ctx)
{
	GOptionGroup *option_group;
	ENTER();
	option_group = g_option_group_new("gstout", "GStreamer Output Options",
	                                  "Show GStreamer Output Options",
	                                  NULL, NULL);
	g_option_group_add_entries(option_group, option_entries);

	g_option_context_add_group (ctx, option_group);
	
	g_option_context_add_group (ctx, gst_init_get_option_group ());
	LEAVE();
	return 0;
}

int output_gstreamer_init(void)
{
	GstBus *bus;

	ENTER();

	//scan_mime_list();

	play = gst_element_factory_make("playbin2", "play");

	bus = gst_pipeline_get_bus(GST_PIPELINE(play));
	gst_bus_add_watch(bus, my_bus_callback, NULL);
	gst_object_unref(bus);

	if(audiosink != NULL){
		GstElement *sink = NULL;
		deg("Setting audio sink to %s\n", audiosink);
		sink = gst_element_factory_make (audiosink, "sink");
		g_object_set (G_OBJECT (play), "audio-sink", sink, NULL);
	}
	if(videosink != NULL){
		GstElement *sink = NULL;
		deg("Setting video sink to %s\n", videosink);
		sink = gst_element_factory_make (videosink, "sink");
		g_object_set (G_OBJECT (play), "video-sink", sink, NULL);
	}

	if (gst_element_set_state(play, GST_STATE_READY) ==
	    GST_STATE_CHANGE_FAILURE) {
		fprintf(stderr,
			"Error: pipeline doesn't want to get ready\n");
	}

	LEAVE();

	return 0;
}

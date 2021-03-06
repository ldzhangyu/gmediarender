/* upnp_transport.c - UPnP AVTransport routines
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

#include <upnp/upnp.h>
#include <upnp/ithread.h>
#include <upnp/upnptools.h>


#include "logging.h"

#include "xmlescape.h"
#include "upnp.h"
#include "upnp_device.h"
#include "upnp_transport.h"
#include "output_gstreamer.h"

//#define TRANSPORT_SERVICE "urn:upnp-org:serviceId:AVTransport"
#define TRANSPORT_SERVICE "urn:schemas-upnp-org:service:AVTransport"
#define TRANSPORT_TYPE "urn:schemas-upnp-org:service:AVTransport:1"
#define TRANSPORT_SCPD_URL "/upnp/rendertransportSCPD.xml"
#define TRANSPORT_CONTROL_URL "/upnp/control/rendertransport1"
#define TRANSPORT_EVENT_URL "/upnp/event/rendertransport1"


typedef enum {
	TRANSPORT_VAR_TRANSPORT_STATUS,
	TRANSPORT_VAR_NEXT_AV_URI,
	TRANSPORT_VAR_NEXT_AV_URI_META,
	TRANSPORT_VAR_CUR_TRACK_META,
	TRANSPORT_VAR_REL_CTR_POS,
	TRANSPORT_VAR_AAT_INSTANCE_ID,
	TRANSPORT_VAR_AAT_SEEK_TARGET,
	TRANSPORT_VAR_PLAY_MEDIUM,
	TRANSPORT_VAR_REL_TIME_POS,
	TRANSPORT_VAR_REC_MEDIA,
	TRANSPORT_VAR_CUR_PLAY_MODE,
	TRANSPORT_VAR_TRANSPORT_PLAY_SPEED,
	TRANSPORT_VAR_PLAY_MEDIA,
	TRANSPORT_VAR_ABS_TIME_POS,
	TRANSPORT_VAR_CUR_TRACK,
	TRANSPORT_VAR_CUR_TRACK_URI,
	TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS,
	TRANSPORT_VAR_NR_TRACKS,
	TRANSPORT_VAR_AV_URI,
	TRANSPORT_VAR_ABS_CTR_POS,
	TRANSPORT_VAR_CUR_REC_QUAL_MODE,
	TRANSPORT_VAR_CUR_MEDIA_DUR,
	TRANSPORT_VAR_AAT_SEEK_MODE,
	TRANSPORT_VAR_AV_URI_META,
	TRANSPORT_VAR_REC_MEDIUM,

	TRANSPORT_VAR_REC_MEDIUM_WR_STATUS,
	TRANSPORT_VAR_LAST_CHANGE,
	TRANSPORT_VAR_CUR_TRACK_DUR,
	TRANSPORT_VAR_TRANSPORT_STATE,
	TRANSPORT_VAR_POS_REC_QUAL_MODE,
	TRANSPORT_VAR_UNKNOWN,
	TRANSPORT_VAR_COUNT
} transport_variable;

typedef enum {
	TRANSPORT_CMD_GETCURRENTTRANSPORTACTIONS,
	TRANSPORT_CMD_GETDEVICECAPABILITIES,
	TRANSPORT_CMD_GETMEDIAINFO,
	TRANSPORT_CMD_GETPOSITIONINFO,
	TRANSPORT_CMD_GETTRANSPORTINFO,
	TRANSPORT_CMD_GETTRANSPORTSETTINGS,
	TRANSPORT_CMD_NEXT,
	TRANSPORT_CMD_PAUSE,
	TRANSPORT_CMD_PLAY,
	TRANSPORT_CMD_PREVIOUS,
	TRANSPORT_CMD_SEEK,
	TRANSPORT_CMD_SETAVTRANSPORTURI,             
	TRANSPORT_CMD_SETPLAYMODE,
	TRANSPORT_CMD_STOP,
	//TRANSPORT_CMD_SETNEXTAVTRANSPORTURI,
	//TRANSPORT_CMD_RECORD,
	//TRANSPORT_CMD_SETRECORDQUALITYMODE,
	TRANSPORT_CMD_UNKNOWN,                   
	TRANSPORT_CMD_COUNT
} transport_cmd ;

static const char *transport_variables[] = {
	[TRANSPORT_VAR_TRANSPORT_STATE] = "TransportState",
	[TRANSPORT_VAR_TRANSPORT_STATUS] = "TransportStatus",
	[TRANSPORT_VAR_PLAY_MEDIUM] = "PlaybackStorageMedium",
	[TRANSPORT_VAR_REC_MEDIUM] = "RecordStorageMedium",
	[TRANSPORT_VAR_PLAY_MEDIA] = "PossiblePlaybackStorageMedia",
	[TRANSPORT_VAR_REC_MEDIA] = "PossibleRecordStorageMedia",
	[TRANSPORT_VAR_CUR_PLAY_MODE] = "CurrentPlayMode",
	[TRANSPORT_VAR_TRANSPORT_PLAY_SPEED] = "TransportPlaySpeed",
	[TRANSPORT_VAR_REC_MEDIUM_WR_STATUS] = "RecordMediumWriteStatus",
	[TRANSPORT_VAR_CUR_REC_QUAL_MODE] = "CurrentRecordQualityMode",
	[TRANSPORT_VAR_POS_REC_QUAL_MODE] = "PossibleRecordQualityModes",
	[TRANSPORT_VAR_NR_TRACKS] = "NumberOfTracks",
	[TRANSPORT_VAR_CUR_TRACK] = "CurrentTrack",
	[TRANSPORT_VAR_CUR_TRACK_DUR] = "CurrentTrackDuration",
	[TRANSPORT_VAR_CUR_MEDIA_DUR] = "CurrentMediaDuration",
	[TRANSPORT_VAR_CUR_TRACK_META] = "CurrentTrackMetaData",
	[TRANSPORT_VAR_CUR_TRACK_URI] = "CurrentTrackURI",
	[TRANSPORT_VAR_AV_URI] = "AVTransportURI",
	[TRANSPORT_VAR_AV_URI_META] = "AVTransportURIMetaData",
	[TRANSPORT_VAR_NEXT_AV_URI] = "NextAVTransportURI",
	[TRANSPORT_VAR_NEXT_AV_URI_META] = "NextAVTransportURIMetaData",
	[TRANSPORT_VAR_REL_TIME_POS] = "RelativeTimePosition",
	[TRANSPORT_VAR_ABS_TIME_POS] = "AbsoluteTimePosition",
	[TRANSPORT_VAR_REL_CTR_POS] = "RelativeCounterPosition",
	[TRANSPORT_VAR_ABS_CTR_POS] = "AbsoluteCounterPosition",
	[TRANSPORT_VAR_LAST_CHANGE] = "LastChange",
	[TRANSPORT_VAR_AAT_SEEK_MODE] = "A_ARG_TYPE_SeekMode",
	[TRANSPORT_VAR_AAT_SEEK_TARGET] = "A_ARG_TYPE_SeekTarget",
	[TRANSPORT_VAR_AAT_INSTANCE_ID] = "A_ARG_TYPE_InstanceID",
	[TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS] = "CurrentTransportActions",	/* optional */
	[TRANSPORT_VAR_UNKNOWN] = NULL
};

const static char *transport_values_const[] = {
	[TRANSPORT_VAR_TRANSPORT_STATE] = "STOPPED",
	[TRANSPORT_VAR_TRANSPORT_STATUS] = "OK",
	[TRANSPORT_VAR_PLAY_MEDIUM] = "UNKNOWN",
	[TRANSPORT_VAR_REC_MEDIUM] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_PLAY_MEDIA] = "NETWORK,UNKNOWN",
	[TRANSPORT_VAR_REC_MEDIA] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_CUR_PLAY_MODE] = "NORMAL",
	[TRANSPORT_VAR_TRANSPORT_PLAY_SPEED] = "1",
	[TRANSPORT_VAR_REC_MEDIUM_WR_STATUS] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_CUR_REC_QUAL_MODE] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_POS_REC_QUAL_MODE] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_NR_TRACKS] = "0",
	[TRANSPORT_VAR_CUR_TRACK] = "0",
	[TRANSPORT_VAR_CUR_TRACK_DUR] = "00:00:00",
	[TRANSPORT_VAR_CUR_MEDIA_DUR] = "",
	[TRANSPORT_VAR_CUR_TRACK_META] = "",
	[TRANSPORT_VAR_CUR_TRACK_URI] = "",
	[TRANSPORT_VAR_AV_URI] = "",
	[TRANSPORT_VAR_AV_URI_META] = "",
	[TRANSPORT_VAR_NEXT_AV_URI] = "",
	[TRANSPORT_VAR_NEXT_AV_URI_META] = "",
	[TRANSPORT_VAR_REL_TIME_POS] = "",
	[TRANSPORT_VAR_ABS_TIME_POS] = "",
	[TRANSPORT_VAR_REL_CTR_POS] = "2147483647",
	[TRANSPORT_VAR_ABS_CTR_POS] = "2147483647",
	[TRANSPORT_VAR_LAST_CHANGE] = "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\"/>",

	[TRANSPORT_VAR_AAT_SEEK_MODE] = "TRACK_NR",
	[TRANSPORT_VAR_AAT_SEEK_TARGET] = "",
	[TRANSPORT_VAR_AAT_INSTANCE_ID] = "0",
	[TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS] = "Play,Stop",
	[TRANSPORT_VAR_UNKNOWN] = NULL
};

static char *transport_values[TRANSPORT_VAR_UNKNOWN+1] = {NULL};

static const char *transport_states[] = {
	"STOPPED",
	"PAUSED_PLAYBACK",
	"PAUSED_RECORDING",
	"PLAYING",
	"RECORDING",
	"TRANSITIONING",
	"NO_MEDIA_PRESENT",
	NULL
};
static const char *transport_stati[] = {
	"OK",
	"ERROR_OCCURRED",
	" vendor-defined ",
	NULL
};
static const char *media[] = {
	"UNKNOWN",
	"DV",
	"MINI-DV",
	"VHS",
	"W-VHS",
	"S-VHS",
	"D-VHS",
	"VHSC",
	"VIDEO8",
	"HI8",
	"CD-ROM",
	"CD-DA",
	"CD-R",
	"CD-RW",
	"VIDEO-CD",
	"SACD",
	"MD-AUDIO",
	"MD-PICTURE",
	"DVD-ROM",
	"DVD-VIDEO",
	"DVD-R",
	"DVD+RW",
	"DVD-RW",
	"DVD-RAM",
	"DVD-AUDIO",
	"DAT",
	"LD",
	"HDD",
	"MICRO-MV",
	"NETWORK",
	"NONE",
	"NOT_IMPLEMENTED",
	" vendor-defined ",
	NULL
};

static const char *playmodi[] = {
	"NORMAL",
	//"SHUFFLE",
	//"REPEAT_ONE",
	"REPEAT_ALL",
	//"RANDOM",
	//"DIRECT_1",
	"INTRO",
	NULL
};

static const char *playspeeds[] = {
	"1",
	" vendor-defined ",
	NULL
};

static const char *rec_write_stati[] = {
	"WRITABLE",
	"PROTECTED",
	"NOT_WRITABLE",
	"UNKNOWN",
	"NOT_IMPLEMENTED",
	NULL
};

static const char *rec_quality_modi[] = {
	"0:EP",
	"1:LP",
	"2:SP",
	"0:BASIC",
	"1:MEDIUM",
	"2:HIGH",
	"NOT_IMPLEMENTED",
	" vendor-defined ",
	NULL
};

static const char *aat_seekmodi[] = {
	"ABS_TIME",
	"REL_TIME",
	"ABS_COUNT",
	"REL_COUNT",
	"TRACK_NR",
	"CHANNEL_FREQ",
	"TAPE-INDEX",
	"FRAME",
	NULL
};

static struct param_range track_range = {
	0,
	4294967295LL,
	1
};

static struct param_range track_nr_range = {
	0,
	4294967295LL,
	0
};

static struct var_meta transport_var_meta[] = {
	[TRANSPORT_VAR_TRANSPORT_STATE] =		{ SENDEVENT_NO, DATATYPE_STRING, transport_states, NULL },
	[TRANSPORT_VAR_TRANSPORT_STATUS] =		{ SENDEVENT_NO, DATATYPE_STRING, transport_stati, NULL },
	[TRANSPORT_VAR_PLAY_MEDIUM] =			{ SENDEVENT_NO, DATATYPE_STRING, media, NULL },
	[TRANSPORT_VAR_REC_MEDIUM] =			{ SENDEVENT_NO, DATATYPE_STRING, media, NULL },
	[TRANSPORT_VAR_PLAY_MEDIA] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_REC_MEDIA] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_PLAY_MODE] =			{ SENDEVENT_NO, DATATYPE_STRING, playmodi, NULL, "NORMAL" },
	[TRANSPORT_VAR_TRANSPORT_PLAY_SPEED] =		{ SENDEVENT_NO, DATATYPE_STRING, playspeeds, NULL },
	[TRANSPORT_VAR_REC_MEDIUM_WR_STATUS] =		{ SENDEVENT_NO, DATATYPE_STRING, rec_write_stati, NULL },
	[TRANSPORT_VAR_CUR_REC_QUAL_MODE] =		{ SENDEVENT_NO, DATATYPE_STRING, rec_quality_modi, NULL },
	[TRANSPORT_VAR_POS_REC_QUAL_MODE] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_NR_TRACKS] =			{ SENDEVENT_NO, DATATYPE_UI4, NULL, &track_nr_range }, /* no step */
	[TRANSPORT_VAR_CUR_TRACK] =			{ SENDEVENT_NO, DATATYPE_UI4, NULL, &track_range },
	[TRANSPORT_VAR_CUR_TRACK_DUR] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_MEDIA_DUR] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_TRACK_META] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_TRACK_URI] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AV_URI] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AV_URI_META] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_NEXT_AV_URI] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_NEXT_AV_URI_META] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_REL_TIME_POS] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_ABS_TIME_POS] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_REL_CTR_POS] =			{ SENDEVENT_NO, DATATYPE_I4, NULL, NULL },
	[TRANSPORT_VAR_ABS_CTR_POS] =			{ SENDEVENT_NO, DATATYPE_I4, NULL, NULL },
	[TRANSPORT_VAR_LAST_CHANGE] =			{ SENDEVENT_YES, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AAT_SEEK_MODE] =			{ SENDEVENT_NO, DATATYPE_STRING, aat_seekmodi, NULL },
	[TRANSPORT_VAR_AAT_SEEK_TARGET] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AAT_INSTANCE_ID] =		{ SENDEVENT_NO, DATATYPE_UI4, NULL, NULL },
	[TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_UNKNOWN] =			{ SENDEVENT_NO, DATATYPE_UNKNOWN, NULL, NULL }
};	

static struct argument *arguments_setavtransporturi[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "CurrentURI", PARAM_DIR_IN, TRANSPORT_VAR_AV_URI },
	& (struct argument) { "CurrentURIMetaData", PARAM_DIR_IN, TRANSPORT_VAR_AV_URI_META },
	NULL
};

//static struct argument *arguments_setnextavtransporturi[] = {
//        & (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
//        & (struct argument) { "NextURI", PARAM_DIR_IN, TRANSPORT_VAR_NEXT_AV_URI },
//        & (struct argument) { "NextURIMetaData", PARAM_DIR_IN, TRANSPORT_VAR_NEXT_AV_URI_META },
//        NULL
//};

static struct argument *arguments_getmediainfo[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "NrTracks", PARAM_DIR_OUT, TRANSPORT_VAR_NR_TRACKS },
	& (struct argument) { "MediaDuration", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_MEDIA_DUR },
	& (struct argument) { "CurrentURI", PARAM_DIR_OUT, TRANSPORT_VAR_AV_URI },
	& (struct argument) { "CurrentURIMetaData", PARAM_DIR_OUT, TRANSPORT_VAR_AV_URI_META },
	& (struct argument) { "NextURI", PARAM_DIR_OUT, TRANSPORT_VAR_NEXT_AV_URI },
	& (struct argument) { "NextURIMetaData", PARAM_DIR_OUT, TRANSPORT_VAR_NEXT_AV_URI_META },
	& (struct argument) { "PlayMedium", PARAM_DIR_OUT, TRANSPORT_VAR_PLAY_MEDIUM },
	& (struct argument) { "RecordMedium", PARAM_DIR_OUT, TRANSPORT_VAR_REC_MEDIUM },
	& (struct argument) { "WriteStatus", PARAM_DIR_OUT, TRANSPORT_VAR_REC_MEDIUM_WR_STATUS },
	NULL
};

static struct argument *arguments_gettransportinfo[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "CurrentTransportState", PARAM_DIR_OUT, TRANSPORT_VAR_TRANSPORT_STATE },
	& (struct argument) { "CurrentTransportStatus", PARAM_DIR_OUT, TRANSPORT_VAR_TRANSPORT_STATUS },
	& (struct argument) { "CurrentSpeed", PARAM_DIR_OUT, TRANSPORT_VAR_TRANSPORT_PLAY_SPEED },
	NULL
};

static struct argument *arguments_getpositioninfo[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Track", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK },
	& (struct argument) { "TrackDuration", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK_DUR },
	& (struct argument) { "TrackMetaData", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK_META },
	& (struct argument) { "TrackURI", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK_URI },
	& (struct argument) { "RelTime", PARAM_DIR_OUT, TRANSPORT_VAR_REL_TIME_POS },
	& (struct argument) { "AbsTime", PARAM_DIR_OUT, TRANSPORT_VAR_ABS_TIME_POS },
	& (struct argument) { "RelCount", PARAM_DIR_OUT, TRANSPORT_VAR_REL_CTR_POS },
	& (struct argument) { "AbsCount", PARAM_DIR_OUT, TRANSPORT_VAR_ABS_CTR_POS },
	NULL
};

static struct argument *arguments_getdevicecapabilities[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "PlayMedia", PARAM_DIR_OUT, TRANSPORT_VAR_PLAY_MEDIA },
	& (struct argument) { "RecMedia", PARAM_DIR_OUT, TRANSPORT_VAR_REC_MEDIA },
	& (struct argument) { "RecQualityModes", PARAM_DIR_OUT, TRANSPORT_VAR_POS_REC_QUAL_MODE },
	NULL
};

static struct argument *arguments_gettransportsettings[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "PlayMode", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_PLAY_MODE },
	& (struct argument) { "RecQualityMode", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_REC_QUAL_MODE },
	NULL
};

static struct argument *arguments_stop[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
static struct argument *arguments_play[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Speed", PARAM_DIR_IN, TRANSPORT_VAR_TRANSPORT_PLAY_SPEED },
	NULL
};
static struct argument *arguments_pause[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
//static struct argument *arguments_record[] = {
//        & (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
//	NULL
//};

static struct argument *arguments_seek[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Unit", PARAM_DIR_IN, TRANSPORT_VAR_AAT_SEEK_MODE },
	& (struct argument) { "Target", PARAM_DIR_IN, TRANSPORT_VAR_AAT_SEEK_TARGET },
	NULL
};
static struct argument *arguments_next[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
static struct argument *arguments_previous[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
static struct argument *arguments_setplaymode[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "NewPlayMode", PARAM_DIR_IN, TRANSPORT_VAR_CUR_PLAY_MODE },
	NULL
};
//static struct argument *arguments_setrecordqualitymode[] = {
//        & (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
//        & (struct argument) { "NewRecordQualityMode", PARAM_DIR_IN, TRANSPORT_VAR_CUR_REC_QUAL_MODE },
//	NULL
//};
static struct argument *arguments_getcurrenttransportactions[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Actions", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS },
	NULL
};


static struct argument **argument_list[] = {
	[TRANSPORT_CMD_SETAVTRANSPORTURI] =         arguments_setavtransporturi,
	[TRANSPORT_CMD_GETDEVICECAPABILITIES] =     arguments_getdevicecapabilities,
	[TRANSPORT_CMD_GETMEDIAINFO] =              arguments_getmediainfo,
	//[TRANSPORT_CMD_SETNEXTAVTRANSPORTURI] =     arguments_setnextavtransporturi,
	[TRANSPORT_CMD_GETTRANSPORTINFO] =          arguments_gettransportinfo,
	[TRANSPORT_CMD_GETPOSITIONINFO] =           arguments_getpositioninfo,
	[TRANSPORT_CMD_GETTRANSPORTSETTINGS] =      arguments_gettransportsettings,
	[TRANSPORT_CMD_STOP] =                      arguments_stop,
	[TRANSPORT_CMD_PLAY] =                      arguments_play,
	[TRANSPORT_CMD_PAUSE] =                     arguments_pause,
	//[TRANSPORT_CMD_RECORD] =                    arguments_record,
	[TRANSPORT_CMD_SEEK] =                      arguments_seek,
	[TRANSPORT_CMD_NEXT] =                      arguments_next,
	[TRANSPORT_CMD_PREVIOUS] =                  arguments_previous,
	[TRANSPORT_CMD_SETPLAYMODE] =               arguments_setplaymode,
	//[TRANSPORT_CMD_SETRECORDQUALITYMODE] =      arguments_setrecordqualitymode,
	[TRANSPORT_CMD_GETCURRENTTRANSPORTACTIONS] = arguments_getcurrenttransportactions,
	[TRANSPORT_CMD_UNKNOWN] =	NULL
};

/* protects transport_values, and service-specific state */

static ithread_mutex_t transport_mutex;

static enum _transport_state transport_state = TRANSPORT_STOPPED;

static int get_media_info(struct action_event *event)
{
	char *value;
	int rc;
	ENTER();

	value = upnp_get_string(event, "InstanceID");
	if (value == NULL) {
		rc = -1;
		goto out;
	}
	deg("%s: InstanceID='%s'\n", __FUNCTION__, value);
	free(value);

	rc = upnp_append_variable(event, TRANSPORT_VAR_NR_TRACKS,
			"NrTracks");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_MEDIA_DUR,
			"MediaDuration");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_AV_URI,
			"CurrentURI");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_AV_URI_META,
			"CurrentURIMetaData");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_NEXT_AV_URI,
			"NextURI");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_NEXT_AV_URI_META,
			"NextURIMetaData");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REC_MEDIA,
			"PlayMedium");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REC_MEDIUM,
			"RecordMedium");
	if (rc)
		goto out;

	rc = upnp_append_variable(event,
			TRANSPORT_VAR_REC_MEDIUM_WR_STATUS,
			"WriteStatus");
	if (rc)
		goto out;

out:
	return rc;
}


static void notify_lastchange(struct action_event *event, char *value)
{
	const char *varnames[] = {
		"LastChange",
		NULL
	};
	char *varvalues[] = {
		NULL, NULL
	};


	deg("Event: '%s'\n", value);
	varvalues[0] = value;


	transport_values[TRANSPORT_VAR_LAST_CHANGE] = value;
	UpnpNotify(device_handle, event->request->DevUDN,
			event->request->ServiceID, varnames,
			(const char **) varvalues, 1);
}

/* warning - does not lock service mutex */
static void change_var(struct action_event *event, int varnum,
		char *new_value)
{
	char *buf;

	ENTER();

	if ((varnum < 0) || (varnum >= TRANSPORT_VAR_UNKNOWN)) {
		LEAVE();
		return;
	}
	if (new_value == NULL) {
		LEAVE();
		return;
	}

	if(strlen(new_value) > strlen(transport_values[varnum])){
	//	printf("dayu %d\n", strlen(new_value));
		transport_values[varnum] = (char *)realloc(transport_values[varnum], strlen(new_value)+1);
	}
	strcpy(transport_values[varnum], new_value);
		//printf("111\n");

	/*
	if (transport_values[varnum]) {
	      free(transport_values[varnum]);
	}
	transport_values[varnum] = strdup(new_value);
	*/
	if(event != NULL)
	{
		asprintf(&buf,
				"<Event xmlns = \"urn:schemas-upnp-org:metadata-1-0/AVT/\"><InstanceID val=\"0\"><%s val=\"%s\"/></InstanceID></Event>",
				transport_variables[varnum], xmlescape(transport_values[varnum], 1));
		notify_lastchange(event, buf);
		free(buf);
	}
		//printf("222\n");

	LEAVE();

	return;
}

static int obtain_instanceid(struct action_event *event, int *instance)
{
	char *value;
	int rc = 0;

	ENTER();

	value = upnp_get_string(event, "InstanceID");
	if (value == NULL) {
		upnp_set_error(event, UPNP_SOAP_E_INVALID_ARGS,
				"Missing InstanceID");
		return -1;
	}
	//printf("%s: InstanceID='%s'\n", __FUNCTION__, value);
	free(value);

	// TODO - parse value, and store in *instance, if instance!=NULL

	LEAVE();

	return rc;
}

/* UPnP action handlers */

static int set_avtransport_uri(struct action_event *event)
{
	char *value;
	int rc = 0;

	ENTER();

	if (obtain_instanceid(event, NULL)) {
		LEAVE();
		return -1;
	}
	value = upnp_get_string(event, "CurrentURI");
	if (value == NULL) {
		LEAVE();
		return -1;
	}

	ithread_mutex_lock(&transport_mutex);

	deg("%s: CurrentURI='%s'\n", __FUNCTION__, value);

	output_set_uri(value);


	change_var(event, TRANSPORT_VAR_AV_URI, value);
	free(value);
	printf("3333\n");

	value = upnp_get_string(event, "CurrentURIMetaData");
	if (value == NULL) {
		rc = -1;
	} else {
		deg("%s: CurrentURIMetaData='%s'\n", __FUNCTION__,
				value);
		change_var(event, TRANSPORT_VAR_AV_URI_META, value);
		free(value);
	}
	printf("4444\n");

	ithread_mutex_unlock(&transport_mutex);

	LEAVE();
	return rc;
}

//static int set_next_avtransport_uri(struct action_event *event)
//{
//	char *value;
//
//	ENTER();
//
//	if (obtain_instanceid(event, NULL)) {
//		LEAVE();
//		return -1;
//	}
//	value = upnp_get_string(event, "NextURI");
//	if (value == NULL) {
//		LEAVE();
//		return -1;
//	}
//	printf("%s: NextURI='%s'\n", __FUNCTION__, value);
//	free(value);
//	value = upnp_get_string(event, "NextURIMetaData");
//	if (value == NULL) {
//		LEAVE();
//		return -1;
//	}
//	printf("%s: NextURIMetaData='%s'\n", __FUNCTION__, value);
//	free(value);
//
//	LEAVE();
//	return 0;
//}

static int get_transport_info(struct action_event *event)
{
	int rc;
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		rc = -1;
		goto out;
	}

	rc = upnp_append_variable(event, TRANSPORT_VAR_TRANSPORT_STATE,
			"CurrentTransportState");
	deg("state %s\n", transport_values[TRANSPORT_VAR_TRANSPORT_STATE]);
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_TRANSPORT_STATUS,
			"CurrentTransportStatus");
	if (rc)
		goto out;

	rc = upnp_append_variable(event,
			TRANSPORT_VAR_TRANSPORT_PLAY_SPEED,
			"CurrentSpeed");
	if (rc)
		goto out;

out:
	LEAVE();
	return rc;
}

static int get_transport_settings(struct action_event *event)
{
	int rc = 0;
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		rc = -1;
		goto out;
	}

out:
	LEAVE();
	return rc;
}

static int get_position_info(struct action_event *event)
{
	int rc;
	char time[20] = "";
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		rc = -1;
		goto out;
	}

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK, "Track");
	if (rc)
		goto out;

	output_duration(time);
	deg("duration time: %s\n", time);

	rc = upnp_add_response(event, "TrackDuration", time);
	//rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK_DUR, "TrackDuration");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK_META,
			"TrackMetaData");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK_URI,
			"TrackURI");
	if (rc)
		goto out;

	output_position(time);
	deg("time: %s\n", time);

	//change_var(NULL, TRANSPORT_VAR_REL_TIME_POS, time);

	rc = upnp_add_response(event, "RelTime", time);
		/*
	rc = upnp_append_variable(event, TRANSPORT_VAR_REL_TIME_POS, "RelTime");
	if (rc)
		goto out;
		*/

	rc = upnp_append_variable(event, TRANSPORT_VAR_ABS_TIME_POS,
			"AbsTime");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REL_CTR_POS,
			"RelCount");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_ABS_CTR_POS,
			"AbsCount");
	if (rc)
		goto out;

out:
	LEAVE();
	return rc;
}

static int get_device_caps(struct action_event *event)
{
	int rc = 0;
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		rc = -1;
		goto out;
	}

out:
	LEAVE();
	return rc;
}

static int stop(struct action_event *event)
{
	int rc = 0;
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		return -1;
	}

	ithread_mutex_lock(&transport_mutex);
	switch (transport_state) {
		case TRANSPORT_STOPPED:
			break;
		case TRANSPORT_PLAYING:
		case TRANSPORT_TRANSITIONING:
		case TRANSPORT_PAUSED_RECORDING:
		case TRANSPORT_RECORDING:
		case TRANSPORT_PAUSED_PLAYBACK:
			transport_state = TRANSPORT_STOPPED;
			change_var(event, TRANSPORT_VAR_TRANSPORT_STATE,
					"STOPPED");
			output_stop();
			// Set TransportPlaySpeed to '1'
			break;

		case TRANSPORT_NO_MEDIA_PRESENT:
			/* action not allowed in these states - error 701 */
			upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA,
					"Transition not allowed");

			break;
	}
	ithread_mutex_unlock(&transport_mutex);

	LEAVE();

	return 0;
}


static int play(struct action_event *event)
{
	int rc = 0;

	ENTER();

	if (obtain_instanceid(event, NULL)) {
		LEAVE();
		return -1;
	}

	ithread_mutex_lock(&transport_mutex);
	switch (transport_state) {
		case TRANSPORT_PLAYING:
			// Set TransportPlaySpeed to '1'
			break;
		case TRANSPORT_STOPPED:
			if(output_play()){
				upnp_set_error(event, 704, "Playing failed");
				rc = -1;
			} else {
				transport_state = TRANSPORT_PLAYING;
				change_var(event, TRANSPORT_VAR_TRANSPORT_STATE,
						"PLAYING");
			}
			// Set TransportPlaySpeed to '1'
			break;
		case TRANSPORT_PAUSED_PLAYBACK:
			if (output_play_continue()) {
				upnp_set_error(event, 704, "Playing failed");
				rc = -1;
			} else {
				transport_state = TRANSPORT_PLAYING;
				change_var(event, TRANSPORT_VAR_TRANSPORT_STATE,
						"PLAYING");
			}
			// Set TransportPlaySpeed to '1'
			break;

		case TRANSPORT_NO_MEDIA_PRESENT:
		case TRANSPORT_TRANSITIONING:
		case TRANSPORT_PAUSED_RECORDING:
		case TRANSPORT_RECORDING:
			/* action not allowed in these states - error 701 */
			upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA,
					"Transition not allowed");
			rc = -1;

			break;
	}

	ithread_mutex_unlock(&transport_mutex);

	LEAVE();

	return rc;
}

static int upnp_pause(struct action_event *event)
{
	int rc = 0;

	ENTER();

	if (obtain_instanceid(event, NULL)) {
		LEAVE();
		return -1;
	}

	ithread_mutex_lock(&transport_mutex);
	switch (transport_state) {
		case TRANSPORT_PLAYING:
			if(output_pause() != 0){
				upnp_set_error(event, 704, "Playing failed");
				rc = -1;
			} else {
				transport_state = TRANSPORT_PAUSED_PLAYBACK;
				change_var(event, TRANSPORT_VAR_TRANSPORT_STATE, "PAUSED_PLAYBACK");
			}
			break;
		case TRANSPORT_STOPPED:
		case TRANSPORT_PAUSED_PLAYBACK:
			// Set TransportPlaySpeed to '1'
			break;

		case TRANSPORT_NO_MEDIA_PRESENT:
		case TRANSPORT_TRANSITIONING:
		case TRANSPORT_PAUSED_RECORDING:
		case TRANSPORT_RECORDING:
			/* action not allowed in these states - error 701 */
			upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA,
					"Transition not allowed");
			rc = -1;

			break;
	}
	ithread_mutex_unlock(&transport_mutex);

	LEAVE();

	return rc;
}

static int seek(struct action_event *event)
{
	int rc = 0;
	int hour = 0, minuter = 0, second = 0;
	char *value = NULL;
	char *p = NULL;
	int i = 0;
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		rc = -1;
	}
	value = upnp_get_string(event, "Unit");
	if (value == NULL) {
		upnp_set_error(event, UPNP_SOAP_E_INVALID_ARGS, "Missing Unit");
		return -1;
	}
	deg("%s: Unit='%s'\n", __FUNCTION__, value);
	if(strcmp(value, "REL_TIME") == 0)
	{
		free(value);
		value = upnp_get_string(event, "Target");
		if (value == NULL) {
			upnp_set_error(event, UPNP_SOAP_E_INVALID_ARGS, "Missing Target");
			return -1;
		}
		deg("%s: Target='%s'\n", __FUNCTION__, value);
		p = strtok(value, ":");
		hour = atoi(p);

		for(i=0;i<2;i++)
		{
			p = strtok(NULL, ":");
			if(0 == i)
				minuter = atoi(p);
			else if(1 == i)
				second = atoi(p);
		}
		second = hour*60*60 + minuter*60 +second;

		output_seek(second);
		free(value);
	}
	else
		free(value);

	LEAVE();

	return rc;
}

static int next(struct action_event *event)
{
	int rc = 0;

	ENTER();

	if (obtain_instanceid(event, NULL)) {
		rc = -1;
	}

	LEAVE();

	return rc;
}

static int previous(struct action_event *event)
{
	ENTER();

	if (obtain_instanceid(event, NULL)) {
		return -1;
	}

	LEAVE();

	return 0;
}

int set_transport_state(enum _transport_state state)
{
	transport_state = state;
	switch (transport_state) {
		case TRANSPORT_PLAYING:
			change_var(NULL, TRANSPORT_VAR_TRANSPORT_STATE, "PLAYING");
			break;

		case TRANSPORT_STOPPED:
			change_var(NULL, TRANSPORT_VAR_TRANSPORT_STATE, "STOPPED");
			break;
		default:
			deg("state error\n");
	}
}

int transport_init(void)
{
	int i = 0;

	for(i=0;i<TRANSPORT_VAR_UNKNOWN;i++)
	{
		if(strdup(transport_values_const[i]) != NULL)
			transport_values[i] = strdup(transport_values_const[i]);
	}
}


static struct action transport_actions[] = {
	[TRANSPORT_CMD_GETCURRENTTRANSPORTACTIONS] = {"GetCurrentTransportActions", NULL},	/* optional */
	[TRANSPORT_CMD_GETDEVICECAPABILITIES] =     {"GetDeviceCapabilities", get_device_caps},
	[TRANSPORT_CMD_GETMEDIAINFO] =              {"GetMediaInfo", get_media_info},
	[TRANSPORT_CMD_SETAVTRANSPORTURI] =         {"SetAVTransportURI", set_avtransport_uri},	/* RC9800i */
	//[TRANSPORT_CMD_SETNEXTAVTRANSPORTURI] =     {"SetNextAVTransportURI", set_next_avtransport_uri},
	[TRANSPORT_CMD_GETTRANSPORTINFO] =          {"GetTransportInfo", get_transport_info},
	[TRANSPORT_CMD_GETPOSITIONINFO] =           {"GetPositionInfo", get_position_info},
	[TRANSPORT_CMD_GETTRANSPORTSETTINGS] =      {"GetTransportSettings", get_transport_settings},
	[TRANSPORT_CMD_STOP] =                      {"Stop", stop},
	[TRANSPORT_CMD_PLAY] =                      {"Play", play},
	[TRANSPORT_CMD_PAUSE] =                     {"Pause", upnp_pause},	/* optional */
	//[TRANSPORT_CMD_RECORD] =                    {"Record", NULL},	/* optional */
	[TRANSPORT_CMD_SEEK] =                      {"Seek", seek},
	[TRANSPORT_CMD_NEXT] =                      {"Next", next},
	[TRANSPORT_CMD_PREVIOUS] =                  {"Previous", previous},
	[TRANSPORT_CMD_SETPLAYMODE] =               {"SetPlayMode", NULL},	/* optional */
	//[TRANSPORT_CMD_SETRECORDQUALITYMODE] =      {"SetRecordQualityMode", NULL},	/* optional */
	[TRANSPORT_CMD_UNKNOWN] =                  {NULL, NULL}
};


struct service transport_service = {
	.service_name =         TRANSPORT_SERVICE,
	.type =                 TRANSPORT_TYPE,
	.scpd_url =		TRANSPORT_SCPD_URL,
	.control_url =		TRANSPORT_CONTROL_URL,
	.event_url =		TRANSPORT_EVENT_URL,
	.actions =              transport_actions,
	.action_arguments =     argument_list,
	.variable_names =       transport_variables,
	.variable_values =      transport_values,
	.variable_meta =        transport_var_meta,
	.variable_count =       TRANSPORT_VAR_UNKNOWN,
	.command_count =        TRANSPORT_CMD_UNKNOWN,
	.service_mutex =        &transport_mutex
};


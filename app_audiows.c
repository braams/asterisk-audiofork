/*
 */

/*! \file
 *
 * \brief AudioWS application -- transmit and receive audio through a WebSocket
 *
 * \author Max Nesterov <braams@braams.ru>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/file.h"
#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/pbx.h"
#include "asterisk/http_websocket.h"
#include "asterisk/format_cache.h"


/*** DOCUMENTATION
	<application name="AudioWS" language="en_US">
		<synopsis>
			Transmit and receive audio between channel and WebSocket
		</synopsis>
		<syntax>
		<parameter name="url" required="true">
        				<argument name="url" required="true">
        					<para>the URL to the websocket server you want to send the audio to. </para>
        				</argument>
        </parameter>
		</syntax>
		<description>
			<para>Connects to the given Websocket service, then transmits channel audio over that socket.  In turn, audio is received from the socket and sent to the channel.  Only audio frames will be transmitted.</para>
			<para>This application does not automatically answer and should be
			preceeded by an application such as Answer() or Progress().</para>
		</description>
	</application>
 ***/

static const char app[] = "AudioWS";

static int audiows_exec(struct ast_channel *chan, const char *data)
{

  char *parse;
  struct ast_websocket *websocket;
  enum ast_websocket_result result;

  AST_DECLARE_APP_ARGS(args, AST_APP_ARG(url););

  if (ast_strlen_zero(data)) {
    ast_log(LOG_WARNING, "AudioWS requires an argument url\n");
    return -1;
  }


  parse = ast_strdupa(data);

  AST_STANDARD_APP_ARGS(args, parse);

  if (ast_strlen_zero(args.url)) {
    ast_log(LOG_WARNING, "AudioWS requires an argument (url)\n");
    return -1;
  }
  ast_log(AST_LOG_NOTICE, "AudioWS url: %s.\n",args.url);


  pbx_builtin_setvar_helper(chan, "AUDIOWS_URL", args.url);


  ast_verb(2, "Connecting websocket server at %s\n", args.url);

  websocket = ast_websocket_client_create(args.url, "echo", NULL,  &result);


  if (result != WS_OK) {
    ast_log(LOG_ERROR, "Could not connect to websocket on audio\n");
    return -1;
  }



	int res = -1;

	while (ast_waitfor(chan, -1) > -1) {
		struct ast_frame *f = ast_read(chan);
		if (!f) {
			break;
		}




		f->delivery.tv_sec = 0;
		f->delivery.tv_usec = 0;
		if (f->frametype == AST_FRAME_VOICE){

            ast_verb(2, "type %d, len %d\n", f->frametype, f->datalen);

            if (ast_websocket_write(websocket, AST_WEBSOCKET_OPCODE_BINARY, f->data.ptr,f->datalen)) {
                    ast_log(LOG_ERROR, "could not write to websocket on audiofork .\n");
            }

		char *payload;
		uint64_t payload_len;
		enum ast_websocket_opcode opcode;
		int fragmented;


		if (ast_websocket_read(websocket, &payload,	&payload_len, &opcode, &fragmented)) {
			ast_log(LOG_WARNING, "WebSocket read error: %s\n",	strerror(errno));
			return -1;
		}

		switch (opcode) {
		case AST_WEBSOCKET_OPCODE_CLOSE:
			ast_log(LOG_WARNING, "WebSocket closed\n");
			return -1;
		case AST_WEBSOCKET_OPCODE_BINARY:
                ast_verb(2, "ws type %ud, len %ld\n", opcode, payload_len);
			break;

		case AST_WEBSOCKET_OPCODE_TEXT:
//			message = ast_json_load_buf(payload, payload_len, NULL);
//			if (message == NULL) {
//				ast_log(LOG_WARNING,
//					"WebSocket input failed to parse\n");
//			}

			break;


		default:
			/* Ignore all other message types */
			break;
		}

//	struct ast_frame f1 = {
//		.frametype = AST_FRAME_VOICE,
//		.subclass.format = ast_format_slin,
//		.src = "AudioWS",
//		.mallocd = AST_MALLOCD_DATA,
//	};
//
//		f1.data.ptr = ( uint8_t* )payload;
//    	f1.datalen = payload_len;
//    	f1.samples = payload_len / 2;
////            f->data.ptr=( uint8_t* )payload;

            ast_frame_dump("name",f,"prefix");

			if (ast_write(chan, f)) {
			ast_frfree(f);
			goto end;}
			}

		if ((f->frametype == AST_FRAME_DTMF) && (f->subclass.integer == '#')) {
			res = 0;
			ast_frfree(f);
			goto end;
		}
		ast_frfree(f);
	}
end:
	return res;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, audiows_exec);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "AudioWS Application");
/* we do fingerprinting here. and assign tags */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <zmq.h>
#include "moloch.h"
#include "nids.h"

extern MolochConfig_t        config;
extern void                 *esServer;
static void *zsocket;

typedef struct {
    MolochSession_t *session;
    GString         *urlString;
    GString         *hostString;
    GString         *valueString[2];
    char             header[2][40];
    short            pos[2];
    http_parser      parsers[2];
    GChecksum       *checksum[2];
    uint16_t         wParsers:2;
    uint16_t         inHeader:2;
    uint16_t         inValue:2;
    uint16_t         inBody:2;
} HTTPInfo_t;


void zmqexp_handle_complete(MolochSession_t *session, http_parser *hp) {

    HTTPInfo_t *http = hp->data;

    GString *smsg;
    struct in_addr src, dst;

    if (http == NULL || http->urlString == NULL || http->hostString == NULL) {
        return;
    }
    //char *url = http->urlString->str;
    //char *host = http->hostString->str;
    //memcpy(&src, &session->addr1, sizeof(session->addr1));
    //memcpy(&dst, &session->addr2, sizeof(session->addr2));

    smsg =g_string_new("");
    g_string_printf(smsg,"http\t%u\t%u\t%u\t%u\t%s\t%s\t",htonl(session->addr1), session->port1, htonl(session->addr2), session->port2, http->hostString->str, http->urlString->str);

    //LOG("sending %s", smsg->str);

    zmq_send(zsocket, smsg->str, smsg->len, 0);
    g_string_free(smsg, FALSE);

}

void zmqexp_exit() {
    LOG("zmqexp exit");
}

void moloch_plugin_init()
{
    char *zmqconfig;
    void *context;
    char *zmqpattern;
    int zmqpattern_cfg = ZMQ_PUB;
    int rc;
    zmqconfig = moloch_config_str(NULL, "zmqConfig", NULL);
    zmqpattern = moloch_config_str(NULL, "zmqPattern", NULL);
    if (zmqpattern != NULL) {
        if (strcasecmp("push", zmqpattern) == 0) {
            zmqpattern_cfg = ZMQ_PUSH; // currently only 'push' and 'pub' patterns are supported
        }
    }
    if (zmqconfig == NULL) {
        LOG("specify zmqconfig");
        exit(0);
    }
    LOG("zmqexp pattern %s", zmqpattern_cfg==ZMQ_PUB?"pub":"push");
    context = zmq_ctx_new();
    assert(context);
    zsocket = zmq_socket(context, zmqpattern_cfg);
    assert(zsocket);


    rc = zmq_bind(zsocket, zmqconfig);
    if (rc == -1) {
        LOG("Error: bind failed %s\n", strerror(errno));
        exit(0);
    }
    moloch_plugins_register("zmqexport", FALSE);
   moloch_plugins_set_http_cb("zmqexport",
                              NULL,
                              NULL,//         zmqexp_handle_url,
                              NULL,//handle_header,
                              NULL,//zmqexp_handle_header, //NULL,// handle_header_val,
                              NULL,// handle_header_complete,
                              NULL,
                              zmqexp_handle_complete
                              );
    moloch_plugins_set_cb("zmqexport",
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,//zmqexp_save,
      NULL,
      zmqexp_exit,
      NULL
    );

    LOG("zmqexport registered");


}
